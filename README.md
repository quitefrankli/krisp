## Build
This project uses conan + cmake + msbuild for its buildsystem as of ver 1.0.0

## Initial Setup
```
conan remote add bincrafters https://bincrafters.jfrog.io/artifactory/api/conan/public-conan
conan config set general.revisions_enabled=1
git clone $THIS_PROJECT
```

### Initial Setup for MACOS
1. download Vulkan SDK https://vulkan.lunarg.com/sdk/home (use portable installation)
2. add Vulkan SDK 'root' directory to .zshrc/.bashprofile/.bashrc i.e. `export VULKAN_SDK=~/VulkanSDK/1.3.211.0/macOS`
3. `source setup-env.sh`

(for debugging with vscode) 

4. `printenv > .env`
5. add the following lines in `launch.json`
```
"envFile": "${workspaceFolder}/.env",
"program": "${workspaceFolder}/build/bin/vulkan",
```

### Building
```
conan install . -if build/ --build=missing -s build_type=Debug
conan build . -bf build/ --configure
sh shader_compiler.sh
conan build . -bf build/ --build
build/bin/Vulkan.exe
```

### Additional notes for MacOS
on M1 Macs lldb replaces gdb, and "System Integrity Protection" prevents environment variables
from propagating to lldb, a work-around is to alias it...
```
alias lldb=/Applications/Xcode.app/Contents/Developer/usr/bin/lldb
lldb bin/Vulkan
```

on OSX it may also be necessary to set the compiler explicitly i.e.
```
cmake -DCMAKE_C_COMPILER=/opt/homebrew/opt/llvm@13/bin/clang -DCMAKE_CXX_COMPILER=/opt/homebrew/opt/llvm@13/bin/clang++ -DCMAKE_BUILD_TYPE=Debug ..
```

### Fast Compile
Using a runtime linking shared library, experimental functionality can be tested without restarting the application

**Note: only windows is supported currently**

~~**1. Command Line**~~

~~compile and generate a new dll~~ **UPDATE: step 1 is no longer necessary, but leaving it in incase it's necessary for manual testing**
```
sh hot_reload.sh
```

**2. Vulkan**

reload the new dll file
```
[SHIFT]+R
```

## Contributing
run `git-clang-format --style=file` before commiting

## Testing

Run this after build step

```
conan install . -if build/ --build=missing -s build_type=Debug
conan build . -bf build/ --configure
sh shader_compiler.sh
conan build . -bf build/ --testing
```

alternatively

```
cd build
cmake --build . --target unittests --config Debug
bin/unittests
```

## Benchmarking

**Note: only windows is supported currently**

To avoid performance degradation, benchmark often.
```
source ../quick_run.sh
benchmark
```
It should print out something like
```
Avg FPS=1317.3583, Avg TPS=151418.84
```