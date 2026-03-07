#!/usr/bin/env bash
# =============================================================================
# bootstrap.sh — DELTA-V Project Bootstrap  v4.0
# =============================================================================
# Creates a complete, ready-to-build DELTA-V project in seconds.
#
# Usage:
#   # From a downloaded release:
#   ./bootstrap.sh [project-name]
#
#   # One-liner from the internet:
#   curl -sSL https://raw.githubusercontent.com/delta-v/framework/main/bootstrap.sh | bash
#
# What it does:
#   1. Checks system dependencies (cmake, python3, g++/clang++)
#   2. Installs missing Python packages (pyyaml, streamlit)
#   3. Creates project directory structure
#   4. Copies framework sources from the DELTA-V release directory
#   5. Runs autocoder to generate C++ bindings from topology.yaml
#   6. Runs a first cmake configure + build to verify the install
#   7. Prints a friendly "next steps" guide
#
# Supported platforms:
#   macOS 13+  (Homebrew)
#   Ubuntu 22.04 / Debian 12
#   Any Linux with g++-11+ or clang-14+
# =============================================================================

set -euo pipefail

# ── Colours ──────────────────────────────────────────────────────────────────
RED='\033[0;31m'; GRN='\033[0;32m'; YLW='\033[1;33m'
BLU='\033[0;34m'; CYN='\033[0;36m'; BLD='\033[1m'; NC='\033[0m'

info()    { echo -e "${CYN}[INFO]${NC}  $*"; }
ok()      { echo -e "${GRN}[OK]${NC}    $*"; }
warn()    { echo -e "${YLW}[WARN]${NC}  $*"; }
die()     { echo -e "${RED}[FAIL]${NC}  $*" >&2; exit 1; }
hdr()     { echo -e "\n${BLD}${BLU}━━━ $* ━━━${NC}\n"; }

# ── Banner ────────────────────────────────────────────────────────────────────
echo -e "${BLD}${BLU}"
cat << 'BANNER'
  ██████╗ ███████╗██╗  ████████╗ █████╗       ██╗   ██╗
  ██╔══██╗██╔════╝██║  ╚══██╔══╝██╔══██╗      ██║   ██║
  ██║  ██║█████╗  ██║     ██║   ███████║█████╗╚██╗ ██╔╝
  ██║  ██║██╔══╝  ██║     ██║   ██╔══██║╚════╝ ╚████╔╝
  ██████╔╝███████╗███████╗██║   ██║  ██║        ╚██╔╝
  ╚═════╝ ╚══════╝╚══════╝╚═╝   ╚═╝  ╚═╝         ╚═╝
  High-Assurance Flight Software Framework  v4.0  — Bootstrap
BANNER
echo -e "${NC}"

# ── Project name ──────────────────────────────────────────────────────────────
PROJECT="${1:-my_spacecraft}"
DEST="$(pwd)/${PROJECT}"

info "Project name : ${BLD}${PROJECT}${NC}"
info "Location     : ${DEST}"

[[ -d "${DEST}" ]] && die "Directory '${DEST}' already exists. Choose a different name."

# ── Locate the framework release directory ────────────────────────────────────
# When run via  ./bootstrap.sh  the release lives in the same directory.
# When piped via curl the user must have a local release zip; we check CWD.
FRAMEWORK_DIR="$(cd "$(dirname "${BASH_SOURCE[0]:-$0}")" 2>/dev/null && pwd)" || FRAMEWORK_DIR="."

# ── Dependency checks ─────────────────────────────────────────────────────────
hdr "Checking Dependencies"

need_cmd() {
    if command -v "$1" &>/dev/null; then
        ok "$1 → $(command -v "$1")"
    else
        die "$1 not found.  $2"
    fi
}

need_cmd cmake  "Install: sudo apt install cmake   OR  brew install cmake"
need_cmd python3 "Install: sudo apt install python3  OR  brew install python3"
need_cmd git    "Install: sudo apt install git      OR  brew install git"

# C++ compiler
CXX_FOUND=0
for cc in clang++ g++ c++; do
    if command -v "$cc" &>/dev/null; then
        ok "C++ compiler → $cc ($($cc --version 2>&1 | head -1))"
        CXX_FOUND=1; break
    fi
done
[[ $CXX_FOUND -eq 0 ]] && die "No C++20 compiler found. Install clang-14+ or gcc-11+."

# Python packages
hdr "Python Packages"
MISSING_PY=()
for pkg in yaml streamlit; do
    if python3 -c "import $pkg" 2>/dev/null; then
        ok "python3: $pkg"
    else
        MISSING_PY+=("$pkg")
    fi
done

if [[ ${#MISSING_PY[@]} -gt 0 ]]; then
    warn "Installing: ${MISSING_PY[*]}"
    if ! pip3 install pyyaml streamlit --quiet 2>/dev/null; then
        warn "pip3 install failed — install manually:"
        warn "  pip3 install pyyaml streamlit"
    else
        ok "Installed: ${MISSING_PY[*]}"
    fi
fi

# ── Create directory tree ─────────────────────────────────────────────────────
hdr "Creating Project Structure"

mkdir -p "${DEST}"/{src,tests,tools,.github/workflows,docs,gds}

ok "Directory tree:"
echo "  ${PROJECT}/"
echo "  ├── src/             ← Framework source (.hpp)"
echo "  ├── tests/           ← Unit test suite"
echo "  ├── tools/           ← autocoder.py, dv-util.py"
echo "  ├── gds/             ← Ground dashboard and dictionary"
echo "  ├── docs/            ← Documentation"
echo "  └── .github/         ← CI/CD workflows"

# ── Copy framework files ──────────────────────────────────────────────────────
hdr "Installing Framework"

cp_file() {
    local src="$1" dst="$2"
    if [[ -f "$src" ]]; then
        cp "$src" "$dst"
    else
        warn "Not found (skip): $(basename "$src")"
    fi
}

# Core source headers — these are the framework .hpp files
HEADERS=(
    Component.hpp Os.hpp Os_FreeRTOS.hpp Port.hpp Hal.hpp
    Types.hpp Serializer.hpp TimeService.hpp ParamDb.hpp
    Scheduler.hpp TelemHub.hpp CommandHub.hpp EventHub.hpp
    TelemetryBridge.hpp LoggerComponent.hpp WatchdogComponent.hpp
    SensorComponent.hpp PowerComponent.hpp ImuComponent.hpp
    I2cDriver.hpp TopologyManager.hpp
    HeapGuard.hpp TmrStore.hpp Cobs.hpp MissionFsm.hpp
    Requirements.hpp RateGroupExecutive.hpp
)
for h in "${HEADERS[@]}"; do
    cp_file "${FRAMEWORK_DIR}/src/${h}" "${DEST}/src/${h}"
done
cp_file "${FRAMEWORK_DIR}/src/main.cpp" "${DEST}/src/main.cpp"

# Tools
cp_file "${FRAMEWORK_DIR}/tools/autocoder.py" "${DEST}/tools/autocoder.py"
cp_file "${FRAMEWORK_DIR}/tools/dv-util.py"   "${DEST}/tools/dv-util.py"
cp_file "${FRAMEWORK_DIR}/tools/requirements_trace.py" "${DEST}/tools/requirements_trace.py"
cp_file "${FRAMEWORK_DIR}/tools/qualification_report.py" "${DEST}/tools/qualification_report.py"
cp_file "${FRAMEWORK_DIR}/tools/legal_compliance_check.py" "${DEST}/tools/legal_compliance_check.py"

# Tests
cp_file "${FRAMEWORK_DIR}/tests/unit_tests.cpp" "${DEST}/tests/unit_tests.cpp"

# Root config
cp_file "${FRAMEWORK_DIR}/CMakeLists.txt"  "${DEST}/CMakeLists.txt"
cp_file "${FRAMEWORK_DIR}/topology.yaml"   "${DEST}/topology.yaml"
cp_file "${FRAMEWORK_DIR}/LICENSE"         "${DEST}/LICENSE"
cp_file "${FRAMEWORK_DIR}/DISCLAIMER.md"   "${DEST}/DISCLAIMER.md"

# GDS (support both old and new repo layouts)
if [[ -f "${FRAMEWORK_DIR}/gds/gds_dash.py" ]]; then
    cp -R "${FRAMEWORK_DIR}/gds/." "${DEST}/gds/"
elif [[ -f "${FRAMEWORK_DIR}/gds_dash.py" ]]; then
    cp_file "${FRAMEWORK_DIR}/gds_dash.py" "${DEST}/gds/gds_dash.py"
fi

# CI
cp_file "${FRAMEWORK_DIR}/.github/workflows/ci.yml" \
        "${DEST}/.github/workflows/ci.yml"

# Docs and top-level README
cp_file "${FRAMEWORK_DIR}/README.md" "${DEST}/README.md"
if [[ -d "${FRAMEWORK_DIR}/docs" ]]; then
    cp -R "${FRAMEWORK_DIR}/docs/." "${DEST}/docs/"
fi

ok "Framework files installed."

# ── Run autocoder ─────────────────────────────────────────────────────────────
hdr "Running Autocoder"

cd "${DEST}"
if python3 tools/autocoder.py; then
    ok "Autocoder complete — dictionary.json + src/TopologyManager.hpp generated."
else
    warn "Autocoder returned non-zero — check topology.yaml"
fi

# ── First build ───────────────────────────────────────────────────────────────
hdr "Running First Build"

mkdir -p build
cd build

if cmake .. -DCMAKE_BUILD_TYPE=Debug -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
        2>&1 | grep -E "(Error|error|WARNING|--)" | head -20; then
    ok "CMake configure OK"
else
    warn "CMake configure had output — check above"
fi

if cmake --build . --target flight_software -- -j"$(nproc 2>/dev/null || sysctl -n hw.logicalcpu 2>/dev/null || echo 4)" \
        2>&1 | tail -6; then
    ok "flight_software built successfully"
else
    warn "Build had warnings/errors — check output above"
fi

cd "${DEST}"

# ── Final guide ───────────────────────────────────────────────────────────────
hdr "Bootstrap Complete"

echo -e "${BLD}${GRN}✓ Your DELTA-V project is ready!${NC}"
echo -e "  ${BLD}${DEST}${NC}\n"

echo -e "${BLD}Quick Start:${NC}\n"

echo -e "  ${CYN}1. Run the flight software (SITL):${NC}"
echo -e "     cd ${PROJECT}"
echo -e "     ${BLD}./build/flight_software${NC}\n"

echo -e "  ${CYN}2. Open the Ground Data System:${NC}"
echo -e "     ${BLD}streamlit run gds/gds_dash.py${NC}\n"

echo -e "  ${CYN}3. Add a new component:${NC}"
echo -e "     ${BLD}python3 tools/dv-util.py add-component ThermalControl --rate norm${NC}"
echo -e "     ${BLD}python3 tools/autocoder.py${NC}"
echo -e "     ${BLD}cmake --build build${NC}\n"

echo -e "  ${CYN}4. Run the test suite:${NC}"
echo -e "     ${BLD}cmake --build build --target flight_readiness${NC}\n"

echo -e "  ${CYN}5. Generate qualification evidence:${NC}"
echo -e "     ${BLD}cmake --build build --target qualification_bundle${NC}\n"

echo -e "  ${CYN}6. Inspect the topology:${NC}"
echo -e "     ${BLD}python3 tools/dv-util.py list${NC}\n"

echo -e "  ${CYN}7. Read the docs:${NC}"
echo -e "     ${BLD}README.md${NC}  |  ${BLD}docs/SAFETY_ASSURANCE.md${NC}  |  ${BLD}docs/ESP32_BRINGUP.md${NC}\n"

echo -e "${YLW}Tip: topology.yaml is your spacecraft definition file.${NC}"
echo -e "${YLW}     Edit it, run the autocoder, and rebuild — C++ and GDS stay in sync.${NC}\n"
