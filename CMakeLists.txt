cmake_minimum_required(VERSION 3.17)
project(display_control_center)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find PkgConfig to handle GTK dependencies
find_package(PkgConfig REQUIRED)

# Check for gtkmm-4.0 (Required for your specific code)
pkg_check_modules(GTKMM REQUIRED gtkmm-4.0)

# Define the executable
# Note: If you have NOT moved main.cpp to src/ yet, change "src/main.cpp" to "main.cpp"
add_executable(display_control_center src/main.cpp)

# Link the libraries
target_link_libraries(display_control_center PRIVATE ${GTKMM_LIBRARIES})

# Include directories
target_include_directories(display_control_center PRIVATE ${GTKMM_INCLUDE_DIRS})

# Compiler flags (needed for GTK)
target_compile_options(display_control_center PRIVATE ${GTKMM_CFLAGS_OTHER})

include(GNUInstallDirs)
install(TARGETS display_control_center DESTINATION ${CMAKE_INSTALL_BINDIR})
