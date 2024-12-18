# C++ Server
A simple yet powerful cross-platform web server made using C++

# Examples
Checkout [main.cpp](main.cpp)

# Build
## Makefile (Linux)
Run `make` to build the project, the binary will be in the `server` directory or run `make run` to start the server immediately.
## CMake (Windows & Linux)
### Windows
(Make sure you have zlib installed using `vcpkg`)
1. mkdir build && cd build
2. cmake -G "Visual Studio 17 2022" -A x64 -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake ..
3. cmake --build . --config Release 
### Linux
1. mkdir build && cd build
2. cmake ..
3. cmake --build .

# TODO
So much... please kindly check the source and any contributors are welcome!
