cmake_minimum_required(VERSION 3.10)

# Set the project name
project(MyProject)

# Set the C++ standard
set(CMAKE_CXX_STANDARD 17)

# Add the executable
add_executable(MyProject main.cpp)

# Set the path to vcpkg
set(CMAKE_TOOLCHAIN_FILE "${CMAKE_SOURCE_DIR}/../vcpkg/scripts/buildsystems/vcpkg.cmake")

# Find and link the OpenCL library
find_package(OpenCL REQUIRED)
target_link_libraries(MyProject PRIVATE OpenCL::OpenCL)

# Find and link the nlohmann/json library
find_package(nlohmann_json CONFIG REQUIRED)
target_link_libraries(MyProject PRIVATE nlohmann_json::nlohmann_json)

# Find and link the CURL library
find_package(CURL CONFIG REQUIRED)
target_link_libraries(MyProject PRIVATE CURL::libcurl)
