#!/usr/bin/env bash
#
# run.sh - builds and launches PianoSynth (Standalone, with GUI).
#
# Usage:
#   ./run.sh                 # Release build + open the Standalone app
#   ./run.sh --debug         # Debug build
#   ./run.sh --clean         # wipe build/ before compiling
#   ./run.sh --build-only    # compile but do not open the GUI
#   ./run.sh -h | --help
#
# Requirements: cmake >= 3.22, a C++20 compiler, and network access on the first
# build (it downloads JUCE 8 via FetchContent).

set -Eeuo pipefail
shopt -s inherit_errexit 2>/dev/null || true

readonly SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
readonly BUILD_DIR="${SCRIPT_DIR}/build"
readonly PRODUCT="FireSynth"

BUILD_TYPE="Release"
DO_CLEAN=0
BUILD_ONLY=0

log()  { printf '\033[1;34m[run]\033[0m %s\n' "$*" >&2; }
err()  { printf '\033[1;31m[error]\033[0m %s\n' "$*" >&2; }
die()  { err "$*"; exit 1; }

usage() {
  sed -n '2,13p' "${BASH_SOURCE[0]}" | sed 's/^# \{0,1\}//'
  exit 0
}

parse_args() {
  while (( $# )); do
    case "$1" in
      --debug)      BUILD_TYPE="Debug" ;;
      --release)    BUILD_TYPE="Release" ;;
      --clean)      DO_CLEAN=1 ;;
      --build-only) BUILD_ONLY=1 ;;
      -h|--help)    usage ;;
      *)            die "unknown option: $1 (use --help)" ;;
    esac
    shift
  done
}

check_deps() {
  command -v cmake >/dev/null 2>&1 || die "cmake not found. Install it with: brew install cmake"
  local v
  v="$(cmake --version | head -1 | grep -oE '[0-9]+\.[0-9]+' | head -1)"
  log "cmake ${v} detected"
}

build() {
  (( DO_CLEAN )) && { log "cleaning ${BUILD_DIR}"; rm -rf -- "${BUILD_DIR}"; }

  log "configuring (${BUILD_TYPE})... the first run downloads JUCE 8, may take a while"
  cmake -B "${BUILD_DIR}" -S "${SCRIPT_DIR}" -DCMAKE_BUILD_TYPE="${BUILD_TYPE}"

  log "building ${PRODUCT}_Standalone..."
  cmake --build "${BUILD_DIR}" --config "${BUILD_TYPE}" \
        --target "${PRODUCT}_Standalone" --parallel
}

# Locate the Standalone artifact robustly across macOS/Linux/Windows.
find_standalone() {
  local base="${BUILD_DIR}/${PRODUCT}_artefacts"
  local hit
  case "$(uname -s)" in
    Darwin)
      hit="$(find "${base}" -name "${PRODUCT}.app" -maxdepth 4 -type d 2>/dev/null | head -1)" ;;
    Linux)
      hit="$(find "${base}" -name "${PRODUCT}" -maxdepth 4 -type f -perm -u+x 2>/dev/null | head -1)" ;;
    *)  # Windows (Git Bash/MSYS)
      hit="$(find "${base}" -name "${PRODUCT}.exe" -maxdepth 4 -type f 2>/dev/null | head -1)" ;;
  esac
  [[ -n "${hit}" ]] || return 1
  printf '%s\n' "${hit}"
}

launch() {
  local app
  app="$(find_standalone)" || die "Standalone executable not found in ${BUILD_DIR}"
  log "opening GUI: ${app}"
  case "$(uname -s)" in
    Darwin) open "${app}" ;;
    Linux)  "${app}" & disown ;;
    *)      start "" "${app}" ;;
  esac
}

main() {
  parse_args "$@"
  check_deps
  build
  if (( BUILD_ONLY )); then
    log "build finished (--build-only); GUI not opened."
  else
    launch
  fi
}

main "$@"
