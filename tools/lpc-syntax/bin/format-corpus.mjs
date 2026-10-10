// Validate or format a list of LPC files. The corpus entry point is
// testsuite/format.sh, which owns discovery and exclusions; this program
// owns the safety checks for every selected path and write.
//
// Usage:
//   node format-corpus.mjs --check < files.nul
//   node format-corpus.mjs --write path1 path2 ...
//
// Paths may also be supplied on stdin, separated by NUL (the preferred form)
// or by newlines for compatibility. The default is --check; writing requires
// the explicit --write flag.
import {
  closeSync,
  fsyncSync,
  lstatSync,
  openSync,
  readFileSync,
  renameSync,
  unlinkSync,
  writeSync,
} from 'node:fs';
import { fileURLToPath } from 'node:url';
import { dirname, isAbsolute, join, relative, resolve, sep } from 'node:path';
import { randomBytes, createHash } from 'node:crypto';

const here = dirname(fileURLToPath(import.meta.url));
const repoRoot = resolve(here, '..', '..', '..');
const corpusRoot = resolve(repoRoot, 'testsuite');
const { formatLPC, trimDirectiveTrailingBlanks } =
  await import(new URL('../format.mjs', import.meta.url));
const { tokenize } = await import(new URL('../tokenizer.mjs', import.meta.url));

function usageError(message) {
  console.error(`ERROR: ${message}`);
  console.error('Usage: format-corpus.mjs [--check|--write] [paths...]');
  process.exit(2);
}

const args = process.argv.slice(2);
let mode = 'check';
const explicit = [];
let endOptions = false;
for (const arg of args) {
  if (!endOptions && arg === '--') {
    endOptions = true;
  } else if (!endOptions && arg === '--check') {
    if (mode === 'write') usageError('--check and --write are mutually exclusive');
    mode = 'check';
  } else if (!endOptions && arg === '--write') {
    if (mode === 'check' && args.includes('--check')) {
      usageError('--check and --write are mutually exclusive');
    }
    mode = 'write';
  } else if (!endOptions && arg.startsWith('-')) {
    usageError(`unknown option: ${arg}`);
  } else {
    explicit.push(arg);
  }
}

function readStdinPaths() {
  if (process.stdin.isTTY) return [];
  const data = readFileSync(0, 'utf8');
  if (data.includes('\0')) return data.split('\0').filter(Boolean);
  return data.split(/\r?\n/).filter(Boolean);
}

const inputs = explicit.length ? explicit : readStdinPaths();
if (inputs.length === 0) usageError('no files selected');

function isInsideCorpus(path) {
  const rel = relative(corpusRoot, path);
  return rel !== '' && !isAbsolute(rel) && rel !== '..' && !rel.startsWith(`..${sep}`);
}

function validatePath(input) {
  const path = resolve(process.cwd(), input);
  if (!isInsideCorpus(path)) {
    throw new Error('path is outside testsuite');
  }
  if (!/\.(?:lpc|c)$/i.test(path)) {
    throw new Error('path is not an LPC corpus file (*.lpc or *.c)');
  }
  const stat = lstatSync(path);
  if (stat.isSymbolicLink()) throw new Error('symbolic links are not writable corpus inputs');
  if (!stat.isFile()) throw new Error('path is not a regular file');
  return path;
}

function statIdentity(stat) {
  return [
    stat.dev.toString(),
    stat.ino.toString(),
    stat.mode.toString(),
    stat.size.toString(),
    stat.mtimeNs.toString(),
  ].join(':');
}

function digest(bytes) {
  return createHash('sha256').update(bytes).digest('hex');
}

function readInput(path) {
  const bytes = readFileSync(path);
  if (bytes.length >= 3 && bytes[0] === 0xef && bytes[1] === 0xbb && bytes[2] === 0xbf) {
    throw new Error('UTF-8 BOM is not accepted for LPC corpus files');
  }
  let text;
  try {
    text = new TextDecoder('utf-8', { fatal: true }).decode(bytes);
  } catch (error) {
    throw new Error(`invalid UTF-8: ${error.message}`);
  }
  const stat = lstatSync(path, { bigint: true });
  return { bytes, text, identity: statIdentity(stat), digest: digest(bytes) };
}

function sameInput(path, original) {
  const current = readInput(path);
  return current.identity === original.identity && current.digest === original.digest;
}

// Literal-bearing token text must survive formatting BYTE-IDENTICAL -- a
// formatter may reflow code, never alter string/text-block/comment/char/
// directive content. Directive trailing blanks are intentionally normalized
// by formatLPC and therefore compare after the same normalization here.
const LITERAL_KINDS = new Set(['string', 'textblock', 'char', 'comment', 'directive']);
function literalText(source) {
  return tokenize(source).filter((token) => LITERAL_KINDS.has(token.kind))
    .map((token) => token.kind === 'directive'
      ? trimDirectiveTrailingBlanks(token.text)
      : token.text)
    .join('\u0000');
}

function writeAtomically(path, output, original) {
  if (!sameInput(path, original)) {
    throw new Error('input changed during validation; refusing to overwrite it');
  }
  const outputBytes = Buffer.from(output, 'utf8');
  const stat = lstatSync(path, { bigint: true });
  const temp = `${path}.tmp-${process.pid}-${randomBytes(8).toString('hex')}`;
  let fd = -1;
  try {
    fd = openSync(temp, 'wx', Number(stat.mode & 0o7777n));
    let written = 0;
    while (written < outputBytes.length) {
      written += writeSync(fd, outputBytes, written, outputBytes.length - written);
    }
    fsyncSync(fd);
    closeSync(fd);
    fd = -1;
    renameSync(temp, path);
  } catch (error) {
    if (fd !== -1) closeSync(fd);
    try { unlinkSync(temp); } catch {}
    throw error;
  }
}

const uniqueInputs = [];
const seen = new Set();
for (const input of inputs) {
  try {
    const path = validatePath(input);
    if (seen.has(path)) throw new Error('duplicate path');
    seen.add(path);
    uniqueInputs.push({ input, path });
  } catch (error) {
    console.error(`PATH ERROR ${input}: ${error.message}`);
  }
}

let written = 0;
let unchanged = 0;
let wouldChange = 0;
let errors = inputs.length - uniqueInputs.length;
const pending = [];

// Preflight every selected file before writing any file. A multi-file run is
// not transactional, but a malformed or unsupported input cannot cause an
// earlier file to be changed before the rest of the set has been validated.
for (const { input, path } of uniqueInputs) {
  try {
    const original = readInput(path);
    const output = formatLPC(original.text);
    if (output === original.text) {
      unchanged++;
      console.log(`unchanged: ${input}`);
      continue;
    }
    if (literalText(original.text) !== literalText(output)) {
      throw new Error('literal-bearing token content changed');
    }
    if (formatLPC(output) !== output) {
      throw new Error('formatted output is not idempotent');
    }
    pending.push({ input, path, original, output });
  } catch (error) {
    errors++;
    console.error(`FORMAT ERROR ${input}: ${error.message}`);
  }
}

if (errors === 0) {
  for (const record of pending) {
    if (mode === 'check') {
      wouldChange++;
      console.log(`would reformat: ${record.input}`);
      continue;
    }
    try {
      writeAtomically(record.path, record.output, record.original);
      written++;
      console.log(`reformatted: ${record.input}`);
    } catch (error) {
      errors++;
      console.error(`WRITE ERROR ${record.input}: ${error.message}`);
    }
  }
}

console.log(JSON.stringify({
  total: inputs.length,
  written,
  wouldChange,
  unchanged,
  errors,
}));
process.exit(errors > 0 || (mode === 'check' && wouldChange > 0) ? 1 : 0);
