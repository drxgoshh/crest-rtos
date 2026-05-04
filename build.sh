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
ERASE=0
BOOTLOADER=0
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
    -B, --bootloader      Build and flash the bootloader (bootloader target)
    -e, --erase           Mass-erase the device flash via OpenOCD (DESTROYS FLASH)
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
        -B|--bootloader) BOOTLOADER=1; shift ;;
        -e|--erase) ERASE=1; shift ;;
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

# If requested, perform a mass erase and exit. WARNING: this erases all flash!
if [[ $ERASE -eq 1 ]]; then
    info "Mass-erasing device flash via OpenOCD (DESTROYS FLASH)..."
    if ! command -v openocd >/dev/null 2>&1; then
        error "openocd not found in PATH"
    fi
    openocd -f interface/stlink.cfg -f target/stm32f4x.cfg \
      -c "init; reset halt; stm32f2x mass_erase 0; reset run; exit"
    ok "Mass erase complete"
    exit 0
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

# ── optionally build/flash bootloader first ───────────────────────────────────
if [[ $BOOTLOADER -eq 1 ]]; then
    info "Building bootloader (${JOBS} jobs) ..."
    cmake --build "$BUILD_DIR" --target bootloader -j "$JOBS"

    BOOT_BIN="$BUILD_DIR/bootloader.bin"
    if [[ -f "$BOOT_BIN" ]]; then
        ok "Built: $BOOT_BIN  ($(wc -c < "$BOOT_BIN") bytes)"
    else
        error "Expected bootloader binary not found: $BOOT_BIN"
    fi

    if [[ $FLASH -eq 1 ]]; then
        info "Flashing bootloader to $BOARD via ST-Link ..."
        cmake --build "$BUILD_DIR" --target flash-bootloader
        ok "Bootloader flash complete"
    fi
fi

# ── cmake build (kernel) ─────────────────────────────────────────────────────
info "Building kernel (${JOBS} jobs) ..."

cmake --build "$BUILD_DIR" --target crest -j "$JOBS"

BIN="$BUILD_DIR/crest.bin"
if [[ -f "$BIN" ]]; then
    ok "Built: $BIN  ($(wc -c < "$BIN") bytes)"
else
    error "Expected binary not found: $BIN"
fi

# ── flash kernel (to kernel base) ────────────────────────────────────────────
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

