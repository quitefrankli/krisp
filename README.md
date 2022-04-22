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
conan install -s build_type=[Debug/Release] ..
cmake -DCMAKE_BUILD_TYPE=[Debug/Release] .. -A x64
cmake --build . --target Vulkan --config [Debug/Release]
sh ../compile.sh
bin/Vulkan.exe
```

### Fast Compile
Using a runtime linking shared library, experimental functionality can be tested without restarting the application

~~**1. Command Line**~~

~~compile and generate a new dll~~ **UPDATE: step 1 is no longer necessary, but leaving it in incase it's necessary for manual testing**
```
sh ../hot_reload.sh
```

**2. Vulkan**

reload the new dll file
```
[SHIFT]+R
```

## Contributing
run `git-clang-format --style=file` before commiting

## Testing
```
cmake --build . --target unittests --config Debug
```
