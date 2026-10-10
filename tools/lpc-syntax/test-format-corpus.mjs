// Contract tests for bin/format-corpus.mjs. The temporary corpus lives under
// testsuite so the same boundary checks used by the real entry point apply.
import {
  lstatSync,
  mkdtempSync,
  readFileSync,
  readlinkSync,
  rmSync,
  symlinkSync,
  writeFileSync,
} from 'node:fs';
import { fileURLToPath } from 'node:url';
import { dirname, join, relative } from 'node:path';
import { spawnSync } from 'node:child_process';

const toolDir = dirname(fileURLToPath(import.meta.url));
const repoRoot = join(toolDir, '..', '..');
const corpusRoot = join(repoRoot, 'testsuite');
const runner = join(toolDir, 'bin', 'format-corpus.mjs');
const tmpRoot = mkdtempSync(join(corpusRoot, '.format-corpus-test-'));
let failures = 0;

function check(name, condition, detail = '') {
  if (condition) console.log(`  OK  ${name}`);
  else {
    console.error(`FAIL  ${name}${detail ? ` -- ${detail}` : ''}`);
    failures++;
  }
}

function run(args, input = '') {
  return spawnSync(process.execPath, [runner, ...args], {
    cwd: repoRoot,
    input,
    encoding: 'utf8',
  });
}

try {
  const messy = join(tmpRoot, 'messy file.lpc');
  writeFileSync(messy, 'int  value=1;\n', 'utf8');
  const before = readFileSync(messy);
  const rel = relative(repoRoot, messy);

  let result = run(['--check', rel]);
  check('check reports an unformatted file', result.status === 1);
  check('check does not write', readFileSync(messy).equals(before));

  result = run(['--write', rel]);
  check('explicit write succeeds', result.status === 0);
  const formatted = readFileSync(messy);
  check('write changes the selected file', !formatted.equals(before));

  result = run(['--check', rel]);
  check('formatted file passes a second check', result.status === 0);

  const invalid = join(tmpRoot, 'invalid.c');
  const invalidBytes = Buffer.from([0x69, 0x6e, 0x74, 0x20, 0xff, 0x0a]);
  writeFileSync(invalid, invalidBytes);
  result = run(['--write', relative(repoRoot, invalid)]);
  check('invalid UTF-8 is rejected', result.status === 1);
  check('invalid UTF-8 remains byte-identical', readFileSync(invalid).equals(invalidBytes));

  const link = join(tmpRoot, 'link.lpc');
  symlinkSync(messy, link);
  result = run(['--write', relative(repoRoot, link)]);
  check('symlink input is rejected', result.status === 1);
  check('symlink remains a symlink', lstatSync(link).isSymbolicLink() && readlinkSync(link) === messy);

  const outside = join(repoRoot, 'format-corpus-outside.lpc');
  writeFileSync(outside, 'int  outside=1;\n', 'utf8');
  result = run(['--write', outside]);
  check('path outside testsuite is rejected', result.status === 1);
  rmSync(outside, { force: true });

  result = run(['--write']);
  check('write without a file list is rejected', result.status === 2);
} finally {
  rmSync(tmpRoot, { recursive: true, force: true });
}

process.exitCode = failures === 0 ? 0 : 1;
