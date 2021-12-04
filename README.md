## Build
This project uses conan + cmake + msbuild for its buildsystem as of ver 1.0.0

## Process
```
git clone ...
mkdir build && cd build
```

### Using conan build
```
conan install -s build_type=[Debug/Release] ..
conan build ..
bin/Vulkan.exe
```

### Using command line
```
conan install ..
cmake -DCMAKE_BUILD_TYPE=Debug ..
msbuild Vulkan.sln
sh ../compile.sh
bin/Vulkan.exe
```

#### Fast Compile
Using a shared library experimental functions can be compiled and tested quickly without rebuilding everything
```
conan install ..
cmake ..
msbuild shared_lib/shared_lib.sln
```