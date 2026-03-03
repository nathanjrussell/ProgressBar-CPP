add_executable(progressbar_example example.cpp)

target_link_libraries(progressbar_example PRIVATE ProgressBar::progressbar)

# Keep examples out of install by default.
set_target_properties(progressbar_example PROPERTIES
  OUTPUT_NAME "progressbar-example"
)
