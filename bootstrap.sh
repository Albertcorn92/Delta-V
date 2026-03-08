#!/usr/bin/env bash
# =============================================================================
# bootstrap.sh — DELTA-V Project Bootstrap  v4.0
# =============================================================================
# Creates a complete, ready-to-build DELTA-V project in seconds.
#
# Usage:
#   ./bootstrap.sh [options] [project-name]
#
# Common:
#   ./bootstrap.sh
#   ./bootstrap.sh my_spacecraft
#   ./bootstrap.sh --force-overwrite my_spacecraft
#   ./bootstrap.sh --with-devcontainer my_spacecraft
#
# One-liner from internet:
#   curl -sSL https://raw.githubusercontent.com/delta-v/framework/main/bootstrap.sh | bash
#
# What it does:
#   1. Checks system dependencies (cmake, python3, git, C++ compiler)
#   2. Creates a local Python virtual environment (.venv)
#   3. Installs required Python packages in that venv (pyyaml, streamlit)
#   4. Creates project directory structure
#   5. Copies framework sources from the release directory
#   6. Runs autocoder to generate C++ bindings from topology.yaml
#   7. Runs first cmake configure + build to verify install
#   8. Prints a quick "next steps" guide
#
# Supported platforms:
#   macOS 13+ (Homebrew), Ubuntu 22.04+, Debian 12+, Linux with gcc/clang C++20
# =============================================================================

set -euo pipefail

# ── Colours ──────────────────────────────────────────────────────────────────
RED='\033[0;31m'; GRN='\033[0;32m'; YLW='\033[1;33m'
BLU='\033[0;34m'; CYN='\033[0;36m'; BLD='\033[1m'; NC='\033[0m'

info() { echo -e "${CYN}[INFO]${NC}  $*"; }
ok()   { echo -e "${GRN}[OK]${NC}    $*"; }
warn() { echo -e "${YLW}[WARN]${NC}  $*"; }
die()  { echo -e "${RED}[FAIL]${NC}  $*" >&2; exit 1; }
hdr()  { echo -e "\n${BLD}${BLU}━━━ $* ━━━${NC}\n"; }

usage() {
    cat <<'USAGE'
DELTA-V bootstrap

Usage:
  ./bootstrap.sh [options] [project-name]

Options:
  -h, --help              Show help and exit
  -f, --force-overwrite   Overwrite destination if it already exists
  -y, --non-interactive   No prompts (uses defaults or fails on conflicts)
  --with-devcontainer     Scaffold .devcontainer setup for VS Code
  --no-devcontainer       Do not scaffold .devcontainer (default)

Examples:
  ./bootstrap.sh
  ./bootstrap.sh my_spacecraft
  ./bootstrap.sh -f my_spacecraft
  ./bootstrap.sh --with-devcontainer my_spacecraft
USAGE
}

run_with_spinner() {
    local label="$1"
    shift

    local logfile="${DEST}/.bootstrap_$(echo "${label}" | tr ' ' '_' | tr -cd '[:alnum:]_').log"
    local status=0

    if [[ -t 1 ]]; then
        "$@" >"${logfile}" 2>&1 &
        local pid=$!
        local spin='|/-\'
        local i=0
        while kill -0 "${pid}" 2>/dev/null; do
            printf "\r${CYN}[INFO]${NC}  %s ... %c" "${label}" "${spin:${i}:1}"
            i=$(( (i + 1) % 4 ))
            sleep 0.1
        done
        wait "${pid}" || status=$?
        printf "\r"
    else
        "$@" >"${logfile}" 2>&1 || status=$?
    fi

    if [[ ${status} -ne 0 ]]; then
        warn "${label} failed. Last output:"
        tail -40 "${logfile}" >&2 || true
        return "${status}"
    fi

    ok "${label}"
    rm -f "${logfile}" || true
}

scaffold_devcontainer() {
    mkdir -p "${DEST}/.devcontainer"
    cat > "${DEST}/.devcontainer/devcontainer.json" <<'EOF'
{
  "name": "DELTA-V DevContainer",
  "image": "mcr.microsoft.com/devcontainers/cpp:ubuntu-22.04",
  "features": {
    "ghcr.io/devcontainers/features/python:1": {
      "version": "3.11"
    }
  },
  "postCreateCommand": "python3 -m venv .venv && . .venv/bin/activate && if [ -f requirements.txt ]; then pip install -r requirements.txt; else pip install pyyaml streamlit; fi",
  "customizations": {
    "vscode": {
      "extensions": [
        "ms-vscode.cpptools",
        "ms-vscode.cmake-tools",
        "ms-python.python"
      ]
    }
  }
}
EOF
    ok "DevContainer scaffolded at .devcontainer/devcontainer.json"
}

show_banner() {
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
}

# ── Args ─────────────────────────────────────────────────────────────────────
DEFAULT_PROJECT="my_spacecraft"
PROJECT_ARG=""
FORCE_OVERWRITE=0
NON_INTERACTIVE=0
WITH_DEVCONTAINER=0

while (($#)); do
    case "$1" in
        -h|--help)
            usage
            exit 0
            ;;
        -f|--force-overwrite)
            FORCE_OVERWRITE=1
            ;;
        -y|--non-interactive)
            NON_INTERACTIVE=1
            ;;
        --with-devcontainer)
            WITH_DEVCONTAINER=1
            ;;
        --no-devcontainer)
            WITH_DEVCONTAINER=0
            ;;
        --)
            shift
            break
            ;;
        -*)
            die "Unknown option: $1 (use --help)"
            ;;
        *)
            if [[ -z "${PROJECT_ARG}" ]]; then
                PROJECT_ARG="$1"
            else
                die "Too many positional arguments (use --help)."
            fi
            ;;
    esac
    shift
done

if (($#)); then
    if [[ -z "${PROJECT_ARG}" && $# -eq 1 ]]; then
        PROJECT_ARG="$1"
    else
        die "Too many positional arguments (use --help)."
    fi
fi

if [[ -n "${PROJECT_ARG}" ]]; then
    PROJECT="${PROJECT_ARG}"
elif [[ ${NON_INTERACTIVE} -eq 1 ]]; then
    PROJECT="${DEFAULT_PROJECT}"
else
    read -r -p "Project name [${DEFAULT_PROJECT}]: " PROJECT
    PROJECT="${PROJECT:-${DEFAULT_PROJECT}}"
fi

[[ "${PROJECT}" =~ [[:space:]/] ]] && die "Project name must not contain spaces or '/'."

DEST="$(pwd)/${PROJECT}"
BOOTSTRAP_SUCCESS=0
BOOTSTRAP_CREATED_DEST=0

cleanup_on_error() {
    local code=$?
    trap - EXIT INT TERM
    if [[ ${code} -ne 0 && ${BOOTSTRAP_SUCCESS} -eq 0 ]]; then
        if [[ ${BOOTSTRAP_CREATED_DEST} -eq 1 && -n "${DEST}" && -d "${DEST}" ]]; then
            warn "Bootstrap failed. Cleaning partial directory: ${DEST}"
            rm -rf "${DEST}"
        fi
    fi
    exit "${code}"
}
trap cleanup_on_error EXIT INT TERM

while [[ -d "${DEST}" ]]; do
    if [[ ${FORCE_OVERWRITE} -eq 1 ]]; then
        warn "Overwriting existing directory: ${DEST}"
        rm -rf "${DEST}"
        break
    fi

    if [[ ${NON_INTERACTIVE} -eq 1 ]]; then
        die "Directory '${DEST}' exists. Re-run with -f or choose another name."
    fi

    read -r -p "Directory '${PROJECT}' exists. [o]verwrite, [n]ew name, [q]uit? [n]: " choice
    choice="${choice:-n}"
    case "${choice}" in
        o|O)
            warn "Overwriting existing directory: ${DEST}"
            rm -rf "${DEST}"
            break
            ;;
        n|N)
            read -r -p "Enter new project name: " PROJECT
            [[ -z "${PROJECT}" ]] && die "Project name cannot be empty."
            [[ "${PROJECT}" =~ [[:space:]/] ]] && die "Project name must not contain spaces or '/'."
            DEST="$(pwd)/${PROJECT}"
            ;;
        q|Q)
            die "Cancelled by user."
            ;;
        *)
            warn "Invalid choice. Use o, n, or q."
            ;;
    esac
done

info "Project name : ${BLD}${PROJECT}${NC}"
info "Location     : ${DEST}"
show_banner

# ── Locate framework directory ───────────────────────────────────────────────
FRAMEWORK_DIR="$(cd "$(dirname "${BASH_SOURCE[0]:-$0}")" 2>/dev/null && pwd)" || FRAMEWORK_DIR="."

# ── Dependency checks ────────────────────────────────────────────────────────
hdr "Checking Dependencies"

need_cmd() {
    if command -v "$1" &>/dev/null; then
        ok "$1 -> $(command -v "$1")"
    else
        die "$1 not found. $2"
    fi
}

need_cmd cmake "Install: sudo apt install cmake OR brew install cmake"
need_cmd python3 "Install: sudo apt install python3 OR brew install python3"
need_cmd git "Install: sudo apt install git OR brew install git"

CXX_FOUND=0
for cc in clang++ g++ c++; do
    if command -v "${cc}" &>/dev/null; then
        ok "C++ compiler -> ${cc} ($(${cc} --version 2>&1 | head -1))"
        CXX_FOUND=1
        break
    fi
done
[[ ${CXX_FOUND} -eq 0 ]] && die "No C++20 compiler found. Install clang-14+ or gcc-11+."

# ── Create directory tree ────────────────────────────────────────────────────
hdr "Creating Project Structure"

mkdir -p "${DEST}"/{src,tests,tools,.github/workflows,docs,gds}
BOOTSTRAP_CREATED_DEST=1

ok "Directory tree created."
echo "  ${PROJECT}/"
echo "  |- src/              (framework source)"
echo "  |- tests/            (test suite)"
echo "  |- tools/            (autocoder + utilities)"
echo "  |- gds/              (dashboard)"
echo "  |- docs/             (documentation)"
echo '  `- .github/workflows (CI)'

if [[ ${WITH_DEVCONTAINER} -eq 1 ]]; then
    hdr "DevContainer"
    if [[ -d "${FRAMEWORK_DIR}/.devcontainer" ]]; then
        cp -R "${FRAMEWORK_DIR}/.devcontainer" "${DEST}/.devcontainer"
        ok "Copied existing .devcontainer from framework."
    else
        scaffold_devcontainer
    fi
fi

# ── Python environment ───────────────────────────────────────────────────────
hdr "Python Environment"
VENV_DIR="${DEST}/.venv"
VENV_PY="${VENV_DIR}/bin/python"
VENV_PIP="${VENV_DIR}/bin/pip"
FRAMEWORK_REQUIREMENTS="${FRAMEWORK_DIR}/requirements.txt"

info "Creating local virtual environment at ${VENV_DIR}"
python3 -m venv "${VENV_DIR}" || die "Failed to create virtual environment."
[[ -x "${VENV_PY}" && -x "${VENV_PIP}" ]] || die "Virtual environment is missing python/pip."

if [[ -f "${FRAMEWORK_REQUIREMENTS}" ]]; then
    run_with_spinner "Installing Python packages from requirements.txt" \
        "${VENV_PIP}" install -r "${FRAMEWORK_REQUIREMENTS}" --quiet
else
    warn "requirements.txt not found in framework release; using minimal fallback deps."
    run_with_spinner "Installing fallback Python packages" \
        "${VENV_PIP}" install pyyaml streamlit --quiet
fi

ok "Python environment ready."

# ── Copy framework files ─────────────────────────────────────────────────────
hdr "Installing Framework"

cp_file() {
    local src="$1"
    local dst="$2"
    if [[ -f "${src}" ]]; then
        cp "${src}" "${dst}"
    else
        warn "Not found (skip): $(basename "${src}")"
    fi
}

info "Copying src/ tree..."
if [[ -d "${FRAMEWORK_DIR}/src" ]]; then
    cp -R "${FRAMEWORK_DIR}/src/." "${DEST}/src/"
else
    die "Source directory ${FRAMEWORK_DIR}/src not found."
fi

cp_file "${FRAMEWORK_DIR}/tools/autocoder.py" "${DEST}/tools/autocoder.py"
cp_file "${FRAMEWORK_DIR}/tools/dv-util.py" "${DEST}/tools/dv-util.py"
cp_file "${FRAMEWORK_DIR}/tools/requirements_trace.py" "${DEST}/tools/requirements_trace.py"
cp_file "${FRAMEWORK_DIR}/tools/qualification_report.py" "${DEST}/tools/qualification_report.py"
cp_file "${FRAMEWORK_DIR}/tools/legal_compliance_check.py" "${DEST}/tools/legal_compliance_check.py"

cp_file "${FRAMEWORK_DIR}/tests/unit_tests.cpp" "${DEST}/tests/unit_tests.cpp"

cp_file "${FRAMEWORK_DIR}/CMakeLists.txt" "${DEST}/CMakeLists.txt"
cp_file "${FRAMEWORK_DIR}/topology.yaml" "${DEST}/topology.yaml"
cp_file "${FRAMEWORK_DIR}/LICENSE" "${DEST}/LICENSE"
cp_file "${FRAMEWORK_DIR}/DISCLAIMER.md" "${DEST}/DISCLAIMER.md"
cp_file "${FRAMEWORK_DIR}/README.md" "${DEST}/README.md"
cp_file "${FRAMEWORK_DIR}/requirements.txt" "${DEST}/requirements.txt"
cp_file "${FRAMEWORK_DIR}/requirements-ci.txt" "${DEST}/requirements-ci.txt"

if [[ -f "${FRAMEWORK_DIR}/gds/gds_dash.py" ]]; then
    cp -R "${FRAMEWORK_DIR}/gds/." "${DEST}/gds/"
elif [[ -f "${FRAMEWORK_DIR}/gds_dash.py" ]]; then
    cp_file "${FRAMEWORK_DIR}/gds_dash.py" "${DEST}/gds/gds_dash.py"
fi

cp_file "${FRAMEWORK_DIR}/.github/workflows/ci.yml" "${DEST}/.github/workflows/ci.yml"

if [[ -d "${FRAMEWORK_DIR}/docs" ]]; then
    cp -R "${FRAMEWORK_DIR}/docs/." "${DEST}/docs/"
fi

ok "Framework files installed."

# ── Run autocoder ────────────────────────────────────────────────────────────
hdr "Running Autocoder"
cd "${DEST}"
run_with_spinner "Autocoder generation" "${VENV_PY}" tools/autocoder.py
ok "dictionary.json + src/TopologyManager.hpp generated."

# ── First build ──────────────────────────────────────────────────────────────
hdr "Running First Build"

mkdir -p build
cd build

run_with_spinner "CMake configure" \
    cmake .. -DCMAKE_BUILD_TYPE=Debug -DCMAKE_EXPORT_COMPILE_COMMANDS=ON

JOBS="$(nproc 2>/dev/null || sysctl -n hw.logicalcpu 2>/dev/null || echo 4)"
run_with_spinner "Build flight_software" cmake --build . --target flight_software -- -j"${JOBS}"

cd "${DEST}"

# ── Final guide ──────────────────────────────────────────────────────────────
hdr "Bootstrap Complete"

echo -e "${BLD}${GRN}✓ Your DELTA-V project is ready.${NC}"
echo -e "  ${BLD}${DEST}${NC}\n"

echo -e "${BLD}Quick Start:${NC}\n"
echo -e "  ${CYN}1. Enter project + activate venv:${NC}"
echo -e "     cd ${PROJECT}"
echo -e "     ${BLD}source .venv/bin/activate${NC}\n"

echo -e "  ${CYN}2. Run the flight software (SITL):${NC}"
echo -e "     ${BLD}./build/flight_software${NC}\n"

echo -e "  ${CYN}3. Open the Ground Data System:${NC}"
echo -e "     ${BLD}streamlit run gds/gds_dash.py${NC}\n"

echo -e "  ${CYN}4. Add a new component:${NC}"
echo -e "     ${BLD}python tools/dv-util.py add-component ThermalControl --rate norm${NC}"
echo -e "     ${BLD}python tools/autocoder.py${NC}"
echo -e "     ${BLD}cmake --build build${NC}\n"

echo -e "  ${CYN}5. Run assurance gates:${NC}"
echo -e "     ${BLD}cmake --build build --target flight_readiness${NC}"
echo -e "     ${BLD}cmake --build build --target qualification_bundle${NC}\n"

echo -e "  ${CYN}6. Inspect topology:${NC}"
echo -e "     ${BLD}python tools/dv-util.py list${NC}\n"

echo -e "${YLW}Tip: edit topology.yaml, run autocoder, rebuild.${NC}\n"

BOOTSTRAP_SUCCESS=1
