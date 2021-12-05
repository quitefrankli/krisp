#!/bin/bash

set -e 
set -o pipefail

SCRIPT=$(readlink -f "$0")
SCRIPTPATH=$(dirname "$SCRIPT")

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
glslc -fshader-stage=vertex $SCRIPTPATH/src/shaders/vertex/vertex_shader.glsl -o vertex_shader.spv 
glslc -fshader-stage=vertex $SCRIPTPATH/src/shaders/vertex/vertex_shader_color.glsl -o vertex_shader_color.spv 
glslc -fshader-stage=fragment $SCRIPTPATH/src/shaders/fragment/fragment_shader.glsl -o fragment_shader.spv
glslc -fshader-stage=fragment $SCRIPTPATH/src/shaders/fragment/fragment_shader_color.glsl -o fragment_shader_color.spv
echo -e "${GREEN}shader compilation complete!${NC}"
