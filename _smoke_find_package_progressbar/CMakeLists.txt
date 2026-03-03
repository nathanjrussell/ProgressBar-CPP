cmake_minimum_required(VERSION 3.20)
project(ProgressBarFindPackageSmoke LANGUAGES CXX)

find_package(ProgressBar CONFIG REQUIRED)

add_executable(smoke main.cpp)
target_link_libraries(smoke PRIVATE ProgressBar::progressbar)

