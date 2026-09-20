#!/usr/bin/env bash

set -euo pipefail

# PowerShell などから MSYS2 の bash.exe を直接起動した場合にも、
# MSYS2 の基本コマンドと MINGW64 のコンパイラを見つけられるようにする。
if [[ -d /usr/bin ]]; then
        export PATH="/usr/bin:${PATH}"
fi
if [[ -d /mingw64/bin ]]; then
        export PATH="/mingw64/bin:${PATH}"
fi

project_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
build_dir="${project_dir}/build"
object_dir="${build_dir}/obj"
library_file="${build_dir}/libmyfont.a"
test_file="${build_dir}/glyph_cache_test.exe"

compiler="${MYTYPE_CC:-gcc}"
if ! command -v "${compiler}" >/dev/null 2>&1; then
        printf 'error: C compiler not found: %s\n' "${compiler}" >&2
        printf 'Run this script from the MSYS2 MINGW64 shell or set MYTYPE_CC.\n' >&2
        exit 1
fi

archiver="${MYTYPE_AR:-ar}"
if ! command -v "${archiver}" >/dev/null 2>&1; then
        printf 'error: archiver not found: %s\n' "${archiver}" >&2
        exit 1
fi

library_sources=(
        "${project_dir}/src/myfont.c"
        "${project_dir}/src/font_binary.c"
        "${project_dir}/src/font_table.c"
        "${project_dir}/src/font_cmap.c"
        "${project_dir}/src/font_loca.c"
        "${project_dir}/src/font_glyph.c"
        "${project_dir}/src/font_composite.c"
        "${project_dir}/src/font_bitmap.c"
)

common_compile_flags=(
        -std=c17
        -Wall
        -Wextra
)

library_compile_flags=(
        "${common_compile_flags[@]}"
        -I"${project_dir}/include"
        -I"${project_dir}/src/internal"
)

mkdir -p -- "${object_dir}"

objects=()
for source_file in "${library_sources[@]}"; do
        source_name="${source_file##*/}"
        object_file="${object_dir}/${source_name%.c}.o"
        objects+=("${object_file}")
        printf 'Compiling %s\n' "${source_name}"
        "${compiler}" \
                "${library_compile_flags[@]}" \
                -c "${source_file}" \
                -o "${object_file}"
done

printf 'Archiving %s\n' "${library_file}"
temporary_library="${library_file}.tmp"
rm -f -- "${temporary_library}"
"${archiver}" rcs "${temporary_library}" "${objects[@]}"
mv -f -- "${temporary_library}" "${library_file}"

printf 'Linking test %s\n' "${test_file}"
"${compiler}" \
        "${library_compile_flags[@]}" \
        "${project_dir}/tests/glyph_cache_test.c" \
        -L"${build_dir}" \
        -lmyfont \
        -lm \
        -o "${test_file}"

printf 'Library: %s\n' "${library_file}"
printf 'Public header: %s\n' "${project_dir}/include/myfont.h"
printf 'Test: %s\n' "${test_file}"
