# HalideNsightDemo

## Prerequisite

### Install Halide
Follow the Halide official instructions to install (https://github.com/halide/Halide)

### Install CUDA
Make sure CUDA dev toolkits are installed

### Install Nsight Compute:
https://developer.nvidia.com/nsight-compute

### Make sure access to GPU performance counter is enabled
For Windows, the setting is under 

```Nvidia Control Panel -> Developer -> Manage GPU Performance Counters```

### Install VisualStudio or Ninja

## Build the project with make
* NOTE: Our CMake setup is not copying Halide runtime library into the build binary fold, either copy Halide.dll (for windows) to the demo binary folder or make sure it is linked in system path

### Visual Studio
Run this command to generate Visual Studio Solution (In Visual Studio Developer PowerShell):

```cmake "Visual Studio 16 2019" -Thost=x64 -A x64 -DCMAKE_PREFIX_PATH=/path/to/Halide-install -S . -B build```

To build, either directly build with Visual Studio or run:

``` cmake --build .\build\ ```


### CMake with Ninja

```cmake -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=/path/to/Halide-install -S . -B build```

To build run:

``` cmake --build .\build\ ```
