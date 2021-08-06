## Build
This project uses conan + cmake + msbuild for its buildsystem as of ver 1.0.0

## Process
git clone ...
mkdir build && cd build

### Using conan build
conan install ..
conan build ..
bin/Vulkan.exe

### Using command line
conan install ..
cmake -DCMAKE_BUILD_TYPE=Debug ..
msbuild Vulkan.sln
sh ../compile.sh
bin/Vulkan.exe