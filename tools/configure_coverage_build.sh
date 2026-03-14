#!/usr/bin/env bash

set -euo pipefail

build_dir="${1:-build_cov}"

gcc_candidates=(gcc-14 gcc-13 gcc-12 gcc)
gxx_candidates=(g++-14 g++-13 g++-12 g++)

cc_bin=""
cxx_bin=""

for i in "${!gcc_candidates[@]}"; do
    if command -v "${gcc_candidates[$i]}" >/dev/null 2>&1 && \
       command -v "${gxx_candidates[$i]}" >/dev/null 2>&1; then
        cc_bin="${gcc_candidates[$i]}"
        cxx_bin="${gxx_candidates[$i]}"
        break
    fi
done

if [[ -z "${cc_bin}" || -z "${cxx_bin}" ]]; then
    echo "[coverage-config] ERROR: no GCC/G++ pair found." >&2
    echo "[coverage-config] Install GCC and rerun this script." >&2
    exit 1
fi

echo "[coverage-config] Using ${cc_bin} / ${cxx_bin}"
cmake -B "${build_dir}" \
    -DCMAKE_BUILD_TYPE=Debug \
    -DCMAKE_C_COMPILER="${cc_bin}" \
    -DCMAKE_CXX_COMPILER="${cxx_bin}"

echo "[coverage-config] Coverage build configured in ${build_dir}"
