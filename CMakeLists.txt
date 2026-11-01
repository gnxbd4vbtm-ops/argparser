cmake_minimum_required(VERSION 3.20)
project(ArgParser VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(GNUInstallDirs)
include(CTest)

add_library(argparser
    src/ArgParser.cpp
    src/Argument.cpp
    src/Conversion.cpp
    src/Formatter.cpp
    src/Serialization.cpp)
add_library(ArgParser::ArgParser ALIAS argparser)
set_target_properties(argparser PROPERTIES EXPORT_NAME ArgParser)
target_compile_features(argparser PUBLIC cxx_std_20)
target_include_directories(argparser PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)

if(BUILD_TESTING)
    add_executable(argparser_tests tests/test_argparser.cpp)
    target_link_libraries(argparser_tests PRIVATE ArgParser::ArgParser)
    add_test(NAME argparser_tests COMMAND argparser_tests)
endif()

add_executable(argparser_example examples/basic.cpp)
target_link_libraries(argparser_example PRIVATE ArgParser::ArgParser)

option(ARGPARSER_BUILD_BENCHMARKS "Build the standalone parser benchmarks" OFF)
if(ARGPARSER_BUILD_BENCHMARKS)
    add_executable(argparser_benchmark benchmarks/parse_benchmark.cpp)
    target_link_libraries(argparser_benchmark PRIVATE ArgParser::ArgParser)
endif()

install(TARGETS argparser EXPORT ArgParserTargets)
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(EXPORT ArgParserTargets
    FILE ArgParserTargets.cmake
    NAMESPACE ArgParser::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/ArgParser)

include(CMakePackageConfigHelpers)
configure_package_config_file(
    cmake/ArgParserConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/ArgParserConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/ArgParser)
write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/ArgParserConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion)
install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/ArgParserConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/ArgParserConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/ArgParser)
