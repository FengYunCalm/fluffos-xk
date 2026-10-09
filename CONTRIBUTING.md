# Contributing

Thank you for considering contributing to this FluffOS fork.

## Scope of this fork

FluffOS_XK is an independent FluffOS engine fork for team use. The project scope and the boundary between the driver and downstream game data are described in [docs/project-scope.md](docs/project-scope.md).

## How to contribute

1. **Fork** the repository and create a feature branch.
2. **Keep changes focused** and avoid unrelated refactors.
3. **Add tests** where practical, especially for behavior changes.
4. **Update documentation** if the change affects build, usage, or public APIs.

## Build & test

Use the repository's [CMake presets](CMakePresets.json) to configure and build. For example:

```bash
cmake --preset dev-debug
cmake --build --preset dev-debug --target driver lpcc lpc_tests --parallel 4
```

Run C++ tests with `ctest --test-dir build-dev-debug --output-on-failure`. Run LPC tests from `testsuite/`; see the [isolated test runner guide](tools/testsuite/README.md) before choosing a test entry point. Do not remove or overwrite an existing build directory as part of the build instructions.

## Commit messages

Use clear, descriptive commit messages. Prefer this format:

```
<type>: <short summary>

- <bullet detail>
- <bullet detail>
```

Examples: `fix: stabilize Windows install`, `docs: rewrite README`.

## Code style

Follow the repository's [coding standard](docs/coding-standard.md). Preserve behavior and keep formatting-only changes focused.

## Code of Conduct

This project follows `CODE_OF_CONDUCT.md`. By participating, you agree to uphold it.

## License

By contributing, you agree that your contributions will be licensed under the terms in `LICENSE` and `NOTICE`.
