#!/bin/bash

# ─────────────────────────────────────────────────────────────────────────────
#   ██████╗██████╗ ███████╗███████╗████████╗
#  ██╔════╝██╔══██╗██╔════╝██╔════╝╚══██╔══╝
#  ██║     ██████╔╝█████╗  ███████╗   ██║
#  ██║     ██╔══██╗██╔══╝  ╚════██║   ██║
#  ╚██████╗██║  ██║███████╗███████║   ██║
#   ╚═════╝╚═╝  ╚═╝╚══════╝╚══════╝   ╚═╝
#
#  Compact Real-time Embedded SysTem — build script
# ─────────────────────────────────────────────────────────────────────────────

set -e

# ── defaults ─────────────────────────────────────────────────────────────────
BOARD="stm32f446"
FLASH=0
CLEAN=0
DEBUG=0
BUILD_DIR="build"
EXTRA_FLAGS=""
JOBS=$(nproc 2>/dev/null || echo 4)

# ── colours ──────────────────────────────────────────────────────────────────
RED='\033[0;31m'; GREEN='\033[0;32m'; CYAN='\033[0;36m'
YELLOW='\033[1;33m'; BOLD='\033[1m'; RESET='\033[0m'

info()  { echo -e "${CYAN}[CREST]${RESET} $*"; }
ok()    { echo -e "${GREEN}[  OK  ]${RESET} $*"; }
warn()  { echo -e "${YELLOW}[ WARN ]${RESET} $*"; }
error() { echo -e "${RED}[ERROR ]${RESET} $*" >&2; exit 1; }

banner() {
echo -e "${CYAN}"
echo "  ██████╗██████╗ ███████╗███████╗████████╗"
echo " ██╔════╝██╔══██╗██╔════╝██╔════╝╚══██╔══╝"
echo " ██║     ██████╔╝█████╗  ███████╗   ██║   "
echo " ██║     ██╔══██╗██╔══╝  ╚════██║   ██║   "
echo " ╚██████╗██║  ██║███████╗███████║   ██║   "
echo "  ╚═════╝╚═╝  ╚═╝╚══════╝╚══════╝   ╚═╝   "
echo -e "${RESET}  Compact Real-time Embedded SysTem  ${BOLD}v0.1.0${RESET}"
echo ""
}

banner

# ── help ─────────────────────────────────────────────────────────────────────
usage() {
cat <<EOF

${BOLD}Usage:${RESET}
  ./build.sh [OPTIONS]

${BOLD}Options:${RESET}
  -b, --board  <name>   Target board (default: stm32f446)
  -f, --flash           Flash firmware after a successful build (cmake --build --target flash)
  -c, --clean           Wipe the build directory before configuring
  -d, --debug           Debug build: -g -O0 (default: -Os via CMAKE_BUILD_TYPE=MinSizeRel)
  -j, --jobs   <n>      Parallel build jobs (default: nproc)
  -o, --opt    <flags>  Append extra CMake definitions (e.g. -o "-DFOO=1")
  -h, --help            Show this help and exit

${BOLD}Examples:${RESET}
  ./build.sh                        # configure + build (stm32f446, optimised)
  ./build.sh -f                     # build + flash
  ./build.sh -b stm32f446 -d -f    # debug build + flash
  ./build.sh -c                     # clean rebuild
  ./build.sh -j8                    # build with 8 parallel jobs

EOF
}

# ── argument parsing ──────────────────────────────────────────────────────────
while [[ $# -gt 0 ]]; do
    case "$1" in
        -b|--board)  BOARD="$2"; shift 2 ;;
        -f|--flash)  FLASH=1; shift ;;
        -c|--clean)  CLEAN=1; shift ;;
        -d|--debug)  DEBUG=1; shift ;;
        -j|--jobs)   JOBS="$2"; shift 2 ;;
        -o|--opt)    EXTRA_FLAGS="$2"; shift 2 ;;
        -h|--help)   usage; exit 0 ;;
        *)           warn "Unknown option: $1"; usage; exit 1 ;;
    esac
done

# ── clean ─────────────────────────────────────────────────────────────────────
if [[ $CLEAN -eq 1 ]]; then
    info "Removing $BUILD_DIR/ ..."
    rm -rf "$BUILD_DIR"
    ok "Clean done"
fi

mkdir -p "$BUILD_DIR"

# ── cmake configure ───────────────────────────────────────────────────────────
if [[ $DEBUG -eq 1 ]]; then
    BUILD_TYPE="Debug"
else
    BUILD_TYPE="MinSizeRel"
fi

info "Configuring for board=${BOLD}${BOARD}${RESET}${CYAN}, build type=${BOLD}${BUILD_TYPE}${RESET}"

cmake -S . -B "$BUILD_DIR" \
    -DBOARD="$BOARD" \
    -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
    $EXTRA_FLAGS \
    --log-level=WARNING 2>&1

# ── cmake build ───────────────────────────────────────────────────────────────
info "Building (${JOBS} jobs) ..."

cmake --build "$BUILD_DIR" --target crest -j "$JOBS"

BIN="$BUILD_DIR/crest.bin"
if [[ -f "$BIN" ]]; then
    ok "Built: $BIN  ($(wc -c < "$BIN") bytes)"
else
    error "Expected binary not found: $BIN"
fi

# ── flash ─────────────────────────────────────────────────────────────────────
if [[ $FLASH -eq 1 ]]; then
    info "Flashing $BIN to $BOARD via ST-Link ..."
    cmake --build "$BUILD_DIR" --target flash
    ok "Flash complete — board is running"
else
    info "Skipping flash  (pass -f to flash)"
fi


set -e

# ── defaults ─────────────────────────────────────────────────────────────────
BOARD="stm32f446"
ARCH="arm/cortex-m4"
FLASH=0
CLEAN=0
BUILD_DIR="build"
TARGET="$BUILD_DIR/crest"
EXTRA_CFLAGS=""

# ── colours ──────────────────────────────────────────────────────────────────
RED='\033[0;31m'; GREEN='\033[0;32m'; CYAN='\033[0;36m'
YELLOW='\033[1;33m'; BOLD='\033[1m'; RESET='\033[0m'

info()    { echo -e "${CYAN}[CREST]${RESET} $*"; }
ok()      { echo -e "${GREEN}[  OK  ]${RESET} $*"; }
warn()    { echo -e "${YELLOW}[ WARN ]${RESET} $*"; }
error()   { echo -e "${RED}[ERROR ]${RESET} $*" >&2; exit 1; }

