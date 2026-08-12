#!/bin/bash

set -e
set -o pipefail

if [ "$#" -ne 2 ]
then
	echo "usage: $0 OUTPUT_DIRECTORY STAMP_FILE" >&2
	exit 2
fi

SRC_DIR=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
OUTPUT_DIR=$1
STAMP_FILE=$2
SHADERS_DIR=$SRC_DIR/shaders

compile_if_file_exists()
{
	shader_dir=$1
	build_dir=$2
	shader_file_name=$3
	shift 3
	shader_file=$shader_dir/$shader_file_name.glsl

	if [ -f "$shader_file" ]
	then
		glslc "$@" "$shader_file" -o "$build_dir/$shader_file_name.spv"
	fi
}

compile_shader_dir()
{
	shader_name=$1
	src_dir=$2
	build_dir=$OUTPUT_DIR/$shader_name

	echo "compiling [$shader_name] shaders"

	mkdir -p "$build_dir"

	compile_if_file_exists "$src_dir" "$build_dir" vertex_shader -fshader-stage=vertex
	compile_if_file_exists "$src_dir" "$build_dir" geometry_shader -fshader-stage=geometry
	compile_if_file_exists "$src_dir" "$build_dir" fragment_shader -fshader-stage=fragment
	compile_if_file_exists "$src_dir" "$build_dir" raygen_shader -fshader-stage=rgen --target-env=vulkan1.2
	compile_if_file_exists "$src_dir" "$build_dir" rayhit_shader -fshader-stage=rchit --target-env=vulkan1.2
	compile_if_file_exists "$src_dir" "$build_dir" raymiss_shader -fshader-stage=rmiss --target-env=vulkan1.2
}

#Black        0;30     Dark Gray     1;30
#Red          0;31     Light Red     1;31
#Green        0;32     Light Green   1;32
#Brown/Orange 0;33     Yellow        1;33
#Blue         0;34     Light Blue    1;34
#Purple       0;35     Light Purple  1;35
#Cyan         0;36     Light Cyan    1;36
#Light Gray   0;37     White         1;37

YELLOW='\033[1;33m' # yellow
GREEN='\033[0;32m' # green
NC='\033[0m' # No Color

echo -e "${YELLOW}compiling shaders...${NC}"
# for each folder in under $RASTERIZATION_SHADERS_DIR except "library" compile each folder
for directory in "$SHADERS_DIR"/*/*/
do
	if [[ $directory == "$SHADERS_DIR/raytracing_shaders/"* ]]
	then
		# Ray tracing is unsupported; keep its shaders out of the build.
		continue
	fi
	shader_name=$(basename -- "$directory")
	compile_shader_dir "$shader_name" "$directory"
done

touch "$STAMP_FILE"
echo -e "${GREEN}shader compilation complete!${NC}"
