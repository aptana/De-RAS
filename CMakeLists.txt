# De-RAS - top-level build
cmake_minimum_required(VERSION 3.21)
project(DeRAS VERSION 0.2.3 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(DERAS_BUILD_UI "Build the Qt 6 desktop application" ON)
option(DERAS_BUILD_TESTS "Build unit tests and benchmarks" ON)

set(DERAS_RELEASE_LABEL "ALPHA" CACHE STRING "Release stage label shown on the splash screen and About")
configure_file(cmake/Version.h.in ${CMAKE_BINARY_DIR}/generated/Version.h @ONLY)

if(MSVC)
  add_compile_options(/W4 /utf-8)
else()
  add_compile_options(-Wall -Wextra -Wno-unused-parameter)
endif()

# ---- core library: model, I/O, engines, application services (no Qt) ----
add_library(deras_core STATIC
  core/Messages.cpp core/Model.cpp core/Json.cpp core/Hash.cpp core/Results.cpp
  io/RasText.cpp io/GeometryFile.cpp io/ProjectFiles.cpp io/Project.cpp io/ResultsFile.cpp io/HdfWriter.cpp io/ResultsHdf.cpp io/GeoTiff.cpp io/ModuleData.cpp
  engines/common/XsHydraulics.cpp
  engines/steady/SteadySolver.cpp engines/steady/Structures.cpp engines/unsteady/UnsteadySolver.cpp engines/unsteady/Flow2D.cpp engines/sediment/Sediment.cpp engines/design/Design.cpp
  engines/mapping/Mapping.cpp
  app/ModelDoctor.cpp app/RunService.cpp app/Templates.cpp app/Report.cpp app/ConvertProject.cpp)
target_include_directories(deras_core PUBLIC ${CMAKE_SOURCE_DIR} ${CMAKE_BINARY_DIR}/generated)
if(WIN32 AND NOT MSVC)
  target_link_options(deras_core INTERFACE -static-libgcc -static-libstdc++)
endif()

# Windows version/icon resource helper
function(deras_add_win_resource target name description with_icon)
  if(WIN32)
    set(DERAS_RC_NAME ${name})
    set(DERAS_RC_DESCRIPTION ${description})
    if(with_icon)
      set(DERAS_RC_ICON "IDI_ICON1 ICON \"${CMAKE_SOURCE_DIR}/branding/icons/De-RAS.ico\"")
    else()
      set(DERAS_RC_ICON "")
    endif()
    configure_file(${CMAKE_SOURCE_DIR}/resources/windows/version.rc.in ${CMAKE_BINARY_DIR}/generated/${name}.rc @ONLY)
    target_sources(${target} PRIVATE ${CMAKE_BINARY_DIR}/generated/${name}.rc)
  endif()
endfunction()

# ---- engine process ----
add_executable(deras-steady engines/steady/main.cpp)
target_link_libraries(deras-steady PRIVATE deras_core)
deras_add_win_resource(deras-steady deras-steady "De-RAS 1D steady engine" ON)

# ---- tools ----
add_executable(deras-roundtrip tools/deras_roundtrip.cpp)
target_link_libraries(deras-roundtrip PRIVATE deras_core)
deras_add_win_resource(deras-roundtrip deras-roundtrip "De-RAS file round-trip checker" OFF)
# Developer tool: recreates tests/data/uji-standar (standard test project, not shipped).
add_executable(deras-make-uji-standar tools/testdata/make_uji_standar.cpp)
target_link_libraries(deras-make-uji-standar PRIVATE deras_core)

if(DERAS_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()

if(DERAS_BUILD_UI)
  add_subdirectory(ui)
endif()

install(TARGETS deras-steady deras-roundtrip RUNTIME DESTINATION bin)
