#!/bin/bash

set -e 
set -o pipefail

SRC_DIR=$(dirname -- "${BASH_SOURCE[0]}")

# only necessary for windows
SRC_DIR=$(echo $SRC_DIR | sed 's/\\/\//g')

compile_dir()
{
	shader=$1
	src_dir=$SRC_DIR/src/shaders/$shader
	build_dir=$SRC_DIR/build/shaders/$shader

	echo "compiling [$shader] shaders in [$src_dir] to [$build_dir]"

	mkdir -p $build_dir

	glslc -fshader-stage=vertex $src_dir/vertex_shader.glsl -o $build_dir/vertex_shader.spv
	glslc -fshader-stage=fragment $src_dir/fragment_shader.glsl -o $build_dir/fragment_shader.spv
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

# for each folder in under shaders except "library" compile each folder
for directory in $SRC_DIR/src/shaders/*/
do
	directory=$(basename $directory)
	if [ $directory == library ]
	then
		continue
	fi
	compile_dir $directory
done
echo -e "${GREEN}shader compilation complete!${NC}"
