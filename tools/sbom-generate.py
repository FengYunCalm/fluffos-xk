#!/usr/bin/env python3
"""Generate and check the repository's deterministic CycloneDX 1.4 SBOM.

The manifest is the source of truth for dependency identity.  Vendored
components additionally pin a deterministic digest of the current source
tree.  The digest covers sorted relative paths, POSIX mode bits, and the
SHA-256 of every regular file:

    <path> NUL <mode in octal> NUL <file SHA-256> LF

The digest is then the SHA-256 of the concatenated records.  This makes local
source or patch drift fail closed without requiring network access.

Usage:
  tools/sbom-generate.py [--out third_party/sbom.json] [--validate]
  tools/sbom-generate.py --check [--out third_party/sbom.json] [--validate]
"""

import argparse
import hashlib
import json
import re
import stat
import sys
import uuid
from datetime import datetime, timezone
from pathlib import Path
from urllib.parse import quote, unquote, urlparse

try:
    import yaml
except ImportError:
    sys.stderr.write("PyYAML required: pip install pyyaml\n")
    sys.exit(2)

SCHEMA = "fluffos.third_party.manifest.v1"
CYCLONEDX_SPEC = "1.4"
SHA256_RE = re.compile(r"^[0-9a-fA-F]{64}$")
GIT_COMMIT_RE = re.compile(r"^[0-9a-fA-F]{40}$")
PURL_RE = re.compile(r"^pkg:[a-z0-9.+-]+/[^\s@]+@[^\s]+$")


def _sha256_file(path):
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def source_tree_sha256(root):
    """Return the canonical digest for a vendored source directory."""
    root = Path(root)
    if not root.is_dir():
        raise ValueError(f"source path is not a directory: {root}")

    records = []
    paths = sorted(root.rglob("*"), key=lambda path: path.relative_to(root).as_posix())
    for path in paths:
        if ".git" in path.relative_to(root).parts:
            continue
        if path.is_dir():
            continue
        if path.is_symlink() or not path.is_file():
            raise ValueError(f"unsupported non-regular source entry: {path}")
        relative = path.relative_to(root).as_posix()
        mode = stat.S_IMODE(path.stat().st_mode)
        records.append(
            f"{relative}\0{mode:04o}\0{_sha256_file(path)}\n".encode("utf-8")
        )
    return hashlib.sha256(b"".join(records)).hexdigest()


def _repo_root_for_manifest(manifest_path):
    return Path(manifest_path).resolve().parent.parent


def validate_manifest(manifest, repo_root):
    """Validate manifest structure and fail on vendored source drift."""
    errors = []
    if manifest.get("schema") != SCHEMA:
        errors.append(f"unexpected manifest schema: {manifest.get('schema')}")

    vendored = manifest.get("vendored")
    if not isinstance(vendored, list):
        errors.append("vendored must be a list")
        vendored = []
    seen_names = set()
    for index, dep in enumerate(vendored):
        label = f"vendored[{index}]"
        if not isinstance(dep, dict):
            errors.append(f"{label} must be an object")
            continue
        name = str(dep.get("name", "")).strip()
        if not name:
            errors.append(f"{label}.name is required")
        elif name in seen_names:
            errors.append(f"duplicate vendored name: {name}")
        seen_names.add(name)
        for field in ("path", "version", "source_url", "license"):
            if not str(dep.get(field, "")).strip():
                errors.append(f"{label}.{field} is required")

        source_status = dep.get("source_revision_status")
        source_revision = str(dep.get("source_revision", ""))
        if source_status not in ("verified", "unverified"):
            errors.append(f"{label}.source_revision_status must be verified or unverified")
        elif source_status == "verified" and not GIT_COMMIT_RE.fullmatch(source_revision):
            errors.append(f"{label}.source_revision must be a full commit for verified sources")
        elif source_status == "unverified" and source_revision != "unverified":
            errors.append(f"{label}.source_revision must be unverified when status is unverified")

        expected = str(dep.get("source_tree_sha256", ""))
        if not SHA256_RE.fullmatch(expected):
            errors.append(f"{label}.source_tree_sha256 must be a SHA-256 digest")
        else:
            source_path = repo_root / str(dep.get("path", ""))
            try:
                actual = source_tree_sha256(source_path)
            except ValueError as error:
                errors.append(f"{label}: {error}")
            else:
                if actual.lower() != expected.lower():
                    errors.append(
                        f"{label} source drift: manifest {expected}, actual {actual}"
                    )

        patches = dep.get("patches")
        if not isinstance(patches, dict):
            errors.append(f"{label}.patches must declare status and details")
            continue
        patch_status = patches.get("status")
        if patch_status not in ("verified", "unverified"):
            errors.append(f"{label}.patches.status must be verified or unverified")
        if patch_status == "verified" and source_status != "verified":
            errors.append(f"{label}.verified patches require a verified source revision")
        if patch_status == "unverified" and not str(patches.get("detail", "")).strip():
            errors.append(f"{label}.patches.detail is required when patch status is unverified")

        source_url = str(dep.get("source_url", ""))
        parsed = urlparse(source_url)
        if parsed.scheme not in ("http", "https") or not parsed.netloc:
            errors.append(f"{label}.source_url is not an HTTP(S) URL")

    for index, download in enumerate(manifest.get("external_downloads", [])):
        label = f"external_downloads[{index}]"
        digest = str(download.get("sha256", ""))
        if not SHA256_RE.fullmatch(digest):
            errors.append(f"{label}.sha256 must be a SHA-256 digest")
        if not str(download.get("url", "")).strip():
            errors.append(f"{label}.url is required")

    return errors


def _github_coordinates(url):
    parsed = urlparse(str(url))
    if parsed.hostname != "github.com":
        return None
    parts = [unquote(part) for part in parsed.path.strip("/").split("/") if part]
    if len(parts) < 2:
        return None
    owner, repository = parts[0], parts[1]
    if repository.endswith(".git"):
        repository = repository[:-4]
    if not owner or not repository:
        return None
    return owner, repository


def make_purl(dep):
    """Build a GitHub PURL only when owner/repository and version are known."""
    name = str(dep.get("name", "")).strip()
    version = str(dep.get("version", "")).strip()
    source_url = dep.get("source_url") or dep.get("url")
    if not name or not version or version in ("unknown", "unverified"):
        return None
    coordinates = _github_coordinates(source_url)
    if coordinates:
        owner, repository = coordinates
        return "pkg:github/{}/{}@{}".format(
            quote(owner, safe=".-_~"),
            quote(repository, safe=".-_~"),
            quote(version, safe=".-_+~"),
        )
    return None


def _property(name, value):
    return {"name": name, "value": str(value)}


def _vendored_properties(dep):
    patches = dep["patches"]
    properties = [
        _property("fluffos:source-path", dep["path"]),
        _property("fluffos:source-revision", dep["source_revision"]),
        _property("fluffos:source-revision-status", dep["source_revision_status"]),
        _property("fluffos:source-tree-sha256", dep["source_tree_sha256"]),
        _property("fluffos:patch-status", patches["status"]),
    ]
    if patches.get("basis"):
        properties.append(_property("fluffos:patch-basis", patches["basis"]))
    if patches.get("detail"):
        properties.append(_property("fluffos:patch-note", patches["detail"]))
    return properties


def build_sbom(manifest):
    components = []
    for dep in manifest.get("vendored", []):
        source_url = dep["source_url"]
        component = {
            "type": "library",
            "name": dep["name"],
            "version": str(dep["version"]),
            "licenses": [{"license": {"name": dep["license"]}}],
            "externalReferences": [{"type": "vcs", "url": source_url}],
            "supplier": {"name": source_url},
            "properties": _vendored_properties(dep),
            "hashes": [{"alg": "SHA-256", "content": dep["source_tree_sha256"]}],
        }
        purl = make_purl(dep)
        if purl:
            component["purl"] = purl
        components.append(component)

    for download in manifest.get("external_downloads", []):
        component = {
            "type": "library",
            "name": download["name"],
            "version": str(download["version"]),
            "licenses": [{"license": {"name": download.get("license", "unknown")}}],
            "externalReferences": [{"type": "distribution", "url": download["url"]}],
            "hashes": [{"alg": "SHA-256", "content": download["sha256"]}],
        }
        purl = make_purl(download)
        if purl:
            component["purl"] = purl
        components.append(component)

    for toolchain in manifest.get("toolchain", []):
        if toolchain.get("digest") and toolchain.get("version"):
            digest = str(toolchain["digest"])
            if digest.startswith("sha256:"):
                digest = digest[len("sha256:"):]
            components.append(
                {
                    "type": "operating-system",
                    "name": str(toolchain["name"]),
                    "version": str(toolchain["version"]),
                    "hashes": [{"alg": "SHA-256", "content": digest}],
                }
            )
        for entry in toolchain.get("entries", []):
            name = str(entry.get("name", entry)) if isinstance(entry, dict) else str(entry)
            version = (
                str(entry.get("version", "pinned-by-workflow"))
                if isinstance(entry, dict)
                else "pinned-by-workflow"
            )
            components.append(
                {
                    "type": "application",
                    "name": name,
                    "version": version,
                    "externalReferences": [
                        {"type": "vcs", "url": str(entry["upstream"])}
                    ]
                    if isinstance(entry, dict) and entry.get("upstream")
                    else [],
                }
            )

    manifest_bytes = json.dumps(manifest, sort_keys=True, default=str).encode()
    serial = str(
        uuid.uuid5(uuid.NAMESPACE_URL, hashlib.sha256(manifest_bytes).hexdigest())
    )
    last_reviewed = manifest.get("last_reviewed")
    timestamp = (
        f"{last_reviewed}T00:00:00Z"
        if last_reviewed
        else datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")
    )
    return {
        "bomFormat": "CycloneDX",
        "specVersion": CYCLONEDX_SPEC,
        "serialNumber": f"urn:uuid:{serial}",
        "version": 1,
        "metadata": {
            "timestamp": timestamp,
            "tools": [
                {"vendor": "FluffOS", "name": "sbom-generate.py", "version": "3"}
            ],
            "component": {"type": "application", "name": "FluffOS", "version": "XK"},
        },
        "components": components,
    }


def _render_sbom(sbom):
    return (json.dumps(sbom, indent=2, ensure_ascii=False) + "\n").encode("utf-8")


def validate_cyclonedx(sbom_path):
    """Run offline structural checks and a local schema check when available."""
    errors = []
    with open(sbom_path, encoding="utf-8") as stream:
        sbom = json.load(stream)
    if sbom.get("bomFormat") != "CycloneDX":
        errors.append("bomFormat must be CycloneDX")
    if sbom.get("specVersion") != CYCLONEDX_SPEC:
        errors.append(f"specVersion must be {CYCLONEDX_SPEC}")
    serial = sbom.get("serialNumber", "")
    if not serial.startswith("urn:uuid:"):
        errors.append("serialNumber must be urn:uuid:<uuid>")
    else:
        try:
            uuid.UUID(serial[len("urn:uuid:"):])
        except ValueError:
            errors.append(f"serialNumber is not a valid UUID: {serial}")

    components = sbom.get("components")
    if not isinstance(components, list) or not components:
        errors.append("components must be a non-empty list")
        components = []
    for comp in components:
        if "name" not in comp or "version" not in comp:
            errors.append("component missing name/version")
        if "purl" in comp and not PURL_RE.fullmatch(str(comp["purl"])):
            errors.append(f"invalid purl: {comp['purl']}")
        for digest in comp.get("hashes", []):
            if digest.get("alg") == "SHA-256" and not SHA256_RE.fullmatch(
                str(digest.get("content", ""))
            ):
                errors.append(f"invalid SHA-256 component hash: {digest}")
        for prop in comp.get("properties", []):
            if not isinstance(prop.get("name"), str) or not isinstance(prop.get("value"), str):
                errors.append(f"invalid component property: {prop}")

    schema_candidates = [
        Path(__file__).resolve().parent.parent / "third_party/cyclonedx.schema.json",
        Path(sbom_path).resolve().parent / "cyclonedx.schema.json",
    ]
    schema_path = next((path for path in schema_candidates if path.exists()), None)
    if schema_path:
        try:
            import jsonschema  # type: ignore
        except ImportError:
            pass
        else:
            with schema_path.open(encoding="utf-8") as stream:
                schema = json.load(stream)
            try:
                jsonschema.validate(sbom, schema)
            except jsonschema.ValidationError as error:  # type: ignore
                errors.append(f"CycloneDX schema validation failed: {error.message}")
    return errors


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--manifest", default="third_party/manifest.yaml")
    parser.add_argument("--out", default="third_party/sbom.json")
    parser.add_argument("--check", action="store_true", help="verify committed output without writing")
    parser.add_argument("--validate", action="store_true", help="validate the SBOM")
    args = parser.parse_args()

    manifest_path = Path(args.manifest)
    try:
        with manifest_path.open(encoding="utf-8") as stream:
            manifest = yaml.safe_load(stream)
    except (OSError, yaml.YAMLError) as error:
        print(f"manifest read failed: {error}", file=sys.stderr)
        return 1

    errors = validate_manifest(manifest, _repo_root_for_manifest(manifest_path))
    if errors:
        for error in errors:
            print(f"MANIFEST FAIL: {error}", file=sys.stderr)
        return 1

    sbom = build_sbom(manifest)
    rendered = _render_sbom(sbom)
    output_path = Path(args.out)
    if args.check:
        try:
            actual = output_path.read_bytes()
        except OSError as error:
            print(f"SBOM check: FAIL: {error}", file=sys.stderr)
            return 1
        if actual != rendered:
            print(f"SBOM check: FAIL: {output_path} is stale", file=sys.stderr)
            return 1
        print(f"SBOM check: OK ({output_path})")
    else:
        try:
            output_path.write_bytes(rendered)
        except OSError as error:
            print(f"SBOM write failed: {error}", file=sys.stderr)
            return 1
        print(f"SBOM written: {output_path} ({len(sbom['components'])} components)")

    if args.validate:
        errors = validate_cyclonedx(output_path)
        if errors:
            for error in errors:
                print(f"FAIL: {error}")
            return 1
        print("SBOM validation: OK")
    return 0


if __name__ == "__main__":
    sys.exit(main())
