cmake_minimum_required(VERSION 3.16)
project(c-tuikit VERSION 0.4.0 DESCRIPTION "Interactive panels for command line tools: live status, actions, results" LANGUAGES C)

option(C_TUIKIT_BUILD_TESTS "Build tests" OFF)
option(C_TUIKIT_BUILD_EXAMPLES "Build examples" OFF)
option(C_TUIKIT_BUILD_FUZZ "Build libFuzzer targets" OFF)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

file(GLOB C_TUIKIT_SOURCES CONFIGURE_DEPENDS ${PROJECT_SOURCE_DIR}/src/*.c)
add_library(c-tuikit ${C_TUIKIT_SOURCES})
add_library(c-tuikit::c-tuikit ALIAS c-tuikit)
target_include_directories(c-tuikit PUBLIC
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
set_target_properties(c-tuikit PROPERTIES
    OUTPUT_NAME tuikit
    C_STANDARD 99
    C_VISIBILITY_PRESET hidden
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR})

set(PC_CFLAGS "")
if(BUILD_SHARED_LIBS)
    target_compile_definitions(c-tuikit PRIVATE PNL_BUILD_SHARED INTERFACE PNL_SHARED)
    set(PC_CFLAGS "-DPNL_SHARED")
endif()

install(TARGETS c-tuikit EXPORT c-tuikitTargets)
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(EXPORT c-tuikitTargets
    NAMESPACE c-tuikit::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/c-tuikit)

configure_package_config_file(cmake/c-tuikitConfig.cmake.in
    ${PROJECT_BINARY_DIR}/c-tuikitConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/c-tuikit)
write_basic_package_version_file(${PROJECT_BINARY_DIR}/c-tuikitConfigVersion.cmake
    COMPATIBILITY SameMajorVersion)
install(FILES
    ${PROJECT_BINARY_DIR}/c-tuikitConfig.cmake
    ${PROJECT_BINARY_DIR}/c-tuikitConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/c-tuikit)

configure_file(c-tuikit.pc.in ${PROJECT_BINARY_DIR}/c-tuikit.pc @ONLY)
install(FILES ${PROJECT_BINARY_DIR}/c-tuikit.pc
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig)

if(C_TUIKIT_BUILD_TESTS)
    enable_testing()
    add_executable(test_tuikit tests/test_tuikit.c)
    target_link_libraries(test_tuikit PRIVATE c-tuikit)
    add_test(NAME c-tuikit COMMAND test_tuikit)
    add_library(tuikit_header_check OBJECT tests/header_check.c)
    target_include_directories(tuikit_header_check PRIVATE include)
endif()

if(C_TUIKIT_BUILD_EXAMPLES)
    add_executable(tuikit_basic examples/basic.c)
    target_link_libraries(tuikit_basic PRIVATE c-tuikit)
endif()

if(C_TUIKIT_BUILD_FUZZ)
    add_executable(fuzz_keys fuzz/fuzz_keys.c)
    target_link_libraries(fuzz_keys PRIVATE c-tuikit)
    target_link_options(fuzz_keys PRIVATE -fsanitize=fuzzer)
endif()
