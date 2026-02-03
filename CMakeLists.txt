cmake_minimum_required(VERSION 3.17)
project(display_control_center)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# nlohmann/json (header-only)
include(FetchContent)
FetchContent_Declare(
  json
  GIT_REPOSITORY https://github.com/nlohmann/json.git
  GIT_TAG        v3.11.3
)
FetchContent_MakeAvailable(json)

# Find PkgConfig to handle GTK dependencies
find_package(PkgConfig REQUIRED)

# Check for gtkmm-4.0 (Required for your specific code)
pkg_check_modules(GTKMM REQUIRED gtkmm-4.0)

# Define the executable
add_executable(display_control_center
  src/main.cpp
  src/display_settings.cpp
  src/theme_manager.cpp
)

# Link the libraries
target_link_libraries(display_control_center PRIVATE ${GTKMM_LIBRARIES} nlohmann_json::nlohmann_json)

# Include directories
target_include_directories(display_control_center PRIVATE ${GTKMM_INCLUDE_DIRS} ${CMAKE_SOURCE_DIR}/include)

# Compiler flags (needed for GTK)
target_compile_options(display_control_center PRIVATE ${GTKMM_CFLAGS_OTHER})

include(GNUInstallDirs)
install(TARGETS display_control_center DESTINATION ${CMAKE_INSTALL_BINDIR})
