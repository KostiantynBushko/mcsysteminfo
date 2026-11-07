cmake_minimum_required(VERSION 3.16)

project(mcsysteminfo
    VERSION 0.1.0
    DESCRIPTION "System information utility for embedded Linux targets"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

# Static by default: an embedded target has no runtime loader games to play.
option(BUILD_SHARED_LIBS "Build shared libraries" OFF)
option(MCSYSTEMINFO_BUILD_TESTS "Build the unit test suite" ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# ---------------------------------------------------------------------------
# Generated version header
# ---------------------------------------------------------------------------
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/include/mcsysteminfo/version.hpp.in"
    "${CMAKE_CURRENT_BINARY_DIR}/generated/mcsysteminfo/version.hpp"
    @ONLY)

# ---------------------------------------------------------------------------
# Library
# ---------------------------------------------------------------------------
add_library(mcsysteminfo_lib
    src/sysinfo.cpp
    src/version.cpp)

target_include_directories(mcsysteminfo_lib
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/generated>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)

target_compile_options(mcsysteminfo_lib
    PRIVATE
        $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall;-Wextra;-Wpedantic>)

add_library(mcsysteminfo::mcsysteminfo ALIAS mcsysteminfo_lib)

# The archive must be libmcsysteminfo.a so that -lmcsysteminfo resolves.
set_target_properties(mcsysteminfo_lib PROPERTIES OUTPUT_NAME mcsysteminfo)

# ---------------------------------------------------------------------------
# Executable
# ---------------------------------------------------------------------------
add_executable(mcsysteminfo src/main.cpp)
target_link_libraries(mcsysteminfo PRIVATE mcsysteminfo::mcsysteminfo)
target_compile_options(mcsysteminfo
    PRIVATE
        $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall;-Wextra;-Wpedantic>)

# ---------------------------------------------------------------------------
# Install
# ---------------------------------------------------------------------------
install(TARGETS mcsysteminfo_lib
    EXPORT mcsysteminfoTargets
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")

install(TARGETS mcsysteminfo
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")

install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/include/"
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
    FILES_MATCHING PATTERN "*.hpp")

install(FILES "${CMAKE_CURRENT_BINARY_DIR}/generated/mcsysteminfo/version.hpp"
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/mcsysteminfo")

# pkg-config
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/mcsysteminfo.pc.in"
    "${CMAKE_CURRENT_BINARY_DIR}/mcsysteminfo.pc"
    @ONLY)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/mcsysteminfo.pc"
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/pkgconfig")

# CMake package config for find_package(mcsysteminfo)
install(EXPORT mcsysteminfoTargets
    FILE mcsysteminfoTargets.cmake
    NAMESPACE mcsysteminfo::
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/mcsysteminfo")

# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------
if(MCSYSTEMINFO_BUILD_TESTS)
    enable_testing()
    add_executable(mcsysteminfo_test tests/test_sysinfo.cpp)
    target_link_libraries(mcsysteminfo_test PRIVATE mcsysteminfo::mcsysteminfo)
    add_test(NAME mcsysteminfo_test COMMAND mcsysteminfo_test)
endif()
