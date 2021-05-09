set -e 
set -o pipefail

glslc -fshader-stage=vertex ../src/shaders/vertex_shader.glsl -o vertex_shader.spv 
glslc -fshader-stage=fragment ../src/shaders/fragment_shader.glsl -o fragment_shader.spv
