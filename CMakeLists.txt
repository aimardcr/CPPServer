cmake_minimum_required(VERSION 3.10)

# Set vcpkg toolchain file if available
if(DEFINED ENV{VCPKG_ROOT})
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake" CACHE STRING "")
elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg/scripts/buildsystems/vcpkg.cmake")
    set(CMAKE_TOOLCHAIN_FILE "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg/scripts/buildsystems/vcpkg.cmake" CACHE STRING "")
endif()

project(server CXX)

# Compiler settings
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(MSVC)
    add_compile_options(/W0)
    add_definitions(-D_CRT_SECURE_NO_WARNINGS)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

set(SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/source/src)
set(INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/source/include)
set(BUILD_DIR ${CMAKE_CURRENT_SOURCE_DIR}/build)
set(BINARY_DIR ${BUILD_DIR}/bin)

set(SOURCES
    ${SOURCE_DIR}/MimeType.cpp
    ${SOURCE_DIR}/Utils.cpp
    ${SOURCE_DIR}/HttpRequest.cpp
    ${SOURCE_DIR}/HttpResponse.cpp
    ${SOURCE_DIR}/HttpServer.cpp
)

find_package(ZLIB REQUIRED)

add_library(server STATIC ${SOURCES})
target_include_directories(server PUBLIC ${INCLUDE_DIR})

if(WIN32)
    target_link_libraries(server PUBLIC wsock32 ws2_32)
else()
    target_link_libraries(server PUBLIC pthread)
endif()

target_link_libraries(server PUBLIC ZLIB::ZLIB)

add_executable(main main.cpp)
target_link_libraries(main PRIVATE server)

set_target_properties(main PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/bin
    RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/bin
)

add_custom_command(
    TARGET main PRE_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_SOURCE_DIR}/server
)

if(WIN32)
    set(EXECUTABLE_NAME "main.exe")
    add_custom_command(
        TARGET main POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_FILE:main>
                ${CMAKE_CURRENT_SOURCE_DIR}/server/${EXECUTABLE_NAME}

        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_FILE_DIR:ZLIB::ZLIB>/../bin/zlib1.dll
                ${CMAKE_CURRENT_SOURCE_DIR}/server/zlib1.dll
    )

    add_custom_target(run
        COMMAND ${EXECUTABLE_NAME}
        DEPENDS main
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/server
    )
else()
    set(EXECUTABLE_NAME "main")
    add_custom_command(
        TARGET main POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_FILE:main>
                ${CMAKE_CURRENT_SOURCE_DIR}/server/${EXECUTABLE_NAME}
        COMMAND chmod +x ${CMAKE_CURRENT_SOURCE_DIR}/server/${EXECUTABLE_NAME}
    )

    add_custom_target(run
        COMMAND ./${EXECUTABLE_NAME}
        DEPENDS main
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/server
    )
endif()