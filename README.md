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
conan install -s build_type=Debug ..
cmake -DCMAKE_BUILD_TYPE=Debug ..
msbuild Vulkan.sln
sh ../compile.sh
bin/Vulkan.exe
```

#### Fast Compile
Using a runtime linking shared library, experimental functionality can be tested without restarting the application
While the application is running ...
```
sh ../hot_reload.sh
```
followed by executing a library reload in the engine