cmake_minimum_required(VERSION 3.20)

project(ProgressBar VERSION 0.3.0 LANGUAGES CXX)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

option(PROGRESSBAR_BUILD_EXAMPLES "Build ProgressBar examples" ON)
option(PROGRESSBAR_ENABLED "Compile-time enable ProgressBar" ON)

add_library(progressbar
  src/progress_bars.cpp
)
add_library(ProgressBar::progressbar ALIAS progressbar)

target_compile_features(progressbar PUBLIC cxx_std_17)

target_include_directories(progressbar
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

target_compile_definitions(progressbar
  PUBLIC
    PROGRESSBAR_ENABLED=$<IF:$<BOOL:${PROGRESSBAR_ENABLED}>,1,0>
)

if(PROGRESSBAR_BUILD_EXAMPLES)
  add_subdirectory(examples)
endif()

# -------- install + package config --------
# Install only the new public headers.
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/progressbar/
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/progressbar
  FILES_MATCHING PATTERN "*.hpp"
)

install(TARGETS progressbar
  EXPORT ProgressBarTargets
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(EXPORT ProgressBarTargets
  FILE ProgressBarTargets.cmake
  NAMESPACE ProgressBar::
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/ProgressBar
)

configure_package_config_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/cmake/ProgressBarConfig.cmake.in
  ${CMAKE_CURRENT_BINARY_DIR}/ProgressBarConfig.cmake
  INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/ProgressBar
)

write_basic_package_version_file(
  ${CMAKE_CURRENT_BINARY_DIR}/ProgressBarConfigVersion.cmake
  VERSION ${PROJECT_VERSION}
  COMPATIBILITY SameMajorVersion
)

install(FILES
  ${CMAKE_CURRENT_BINARY_DIR}/ProgressBarConfig.cmake
  ${CMAKE_CURRENT_BINARY_DIR}/ProgressBarConfigVersion.cmake
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/ProgressBar
)
