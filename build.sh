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
sample_file="${build_dir}/mytype.exe"
raylib_import_library="${RAYLIB_IMPORT_LIBRARY:-/mingw64/lib/libraylib.dll.a}"
raylib_dll="${RAYLIB_DLL:-/mingw64/bin/libraylib.dll}"
glfw_dll="${GLFW_DLL:-/mingw64/bin/glfw3.dll}"

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

for runtime_dll in "${raylib_dll}" "${glfw_dll}"; do
        if [[ ! -f "${runtime_dll}" ]]; then
                printf 'error: runtime DLL not found: %s\n' "${runtime_dll}" >&2
                exit 1
        fi
done

if [[ ! -f "${raylib_import_library}" ]]; then
        printf 'error: raylib import library not found: %s\n' \
                "${raylib_import_library}" >&2
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
        "${project_dir}/src/font_render.c"
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

sample_compile_flags=(
        "${common_compile_flags[@]}"
        -I"${project_dir}/include"
        -I"${project_dir}/src/internal"
)

link_flags=(
        # DLL 用の import library を明示し、libraylib.a への静的リンクを避ける。
        "${raylib_import_library}"
        -lopengl32
        -lgdi32
        -lwinmm
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
"${archiver}" rcs "${library_file}" "${objects[@]}"

printf 'Linking sample %s\n' "${sample_file}"
"${compiler}" \
        "${sample_compile_flags[@]}" \
        "${project_dir}/src/font_func.c" \
        -L"${build_dir}" \
        -lmyfont \
        -o "${sample_file}" \
        "${link_flags[@]}"

cp -f -- "${raylib_dll}" "${build_dir}/libraylib.dll"
cp -f -- "${glfw_dll}" "${build_dir}/glfw3.dll"

printf 'Library: %s\n' "${library_file}"
printf 'Public header: %s\n' "${project_dir}/include/myfont.h"
printf 'Sample: %s\n' "${sample_file}"
printf 'Runtime DLLs: %s, %s\n' \
        "${build_dir}/libraylib.dll" \
        "${build_dir}/glfw3.dll"
