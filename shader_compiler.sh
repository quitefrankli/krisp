#!/bin/bash

set -e
set -o pipefail

SRC_DIR=$(dirname -- "${BASH_SOURCE[0]}")

# only necessary for windows
SRC_DIR=$(echo $SRC_DIR | sed 's/\\/\//g')
OUTPUT_DIR=$SRC_DIR/build/shaders
SHADERS_DIR=$SRC_DIR/shaders

compile_if_file_exists()
{
	arguments=$1
	shader_file_name=$2
	shader_dir=$3
	build_dir=$4

	if [ -f $shader_dir/$shader_file_name.glsl ]
	then
		glslc $arguments $shader_dir/$shader_file_name.glsl -o $build_dir/$shader_file_name.spv
	fi
}

compile_shader_dir()
{
	shader_name=$1
	src_dir=$2
	build_dir=$OUTPUT_DIR/$shader_name

	echo "compiling [$shader_name] shaders from [$src_dir] to [$build_dir]"

	mkdir -p $build_dir

	compile_if_file_exists "-fshader-stage=vertex" vertex_shader $src_dir $build_dir
	compile_if_file_exists "-fshader-stage=fragment" fragment_shader $src_dir $build_dir
	compile_if_file_exists "-fshader-stage=rgen --target-env=vulkan1.2" raygen_shader $src_dir $build_dir
	compile_if_file_exists "-fshader-stage=rchit --target-env=vulkan1.2" rayhit_shader $src_dir $build_dir
	compile_if_file_exists "-fshader-stage=rmiss --target-env=vulkan1.2" raymiss_shader $src_dir $build_dir
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
for directory in $SHADERS_DIR/*/*/
do
	shader_name=$(basename $directory)
	if [ $shader_name == library ]
	then
		continue
	fi
	compile_shader_dir $shader_name $directory
done

echo -e "${GREEN}shader compilation complete!${NC}"
