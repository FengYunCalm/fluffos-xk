#!/usr/bin/env bash
# 构建目录与证据目录的上限清理：每类只保留最近 KEEP 个（默认 3）。
# 这些目录都是可重建产物；源码、git 历史和受版本管理的 evidence/ 不计入。
set -euo pipefail

root=$(cd "$(dirname "$0")/.." && pwd)
keep=${KEEP:-3}

prune() {
  local pattern=$1
  local exclude=${2:-}
  local -a find_args=(-maxdepth 1 -mindepth 1 -type d -name "$pattern")
  if [[ -n $exclude ]]; then
    find_args+=(! -name "$exclude")
  fi
  local -a dirs=()
  mapfile -t dirs < <(
    find "$root" "${find_args[@]}" -printf '%T@ %p\n' | sort -rn | cut -d' ' -f2-
  )
  local index=0
  local dir
  for dir in "${dirs[@]}"; do
    index=$((index + 1))
    if ((index > keep)); then
      printf 'prune %s\n' "${dir#"$root"/}"
      rm -rf -- "$dir"
    fi
  done
}

prune 'build-*'
prune '*evidence*' evidence
