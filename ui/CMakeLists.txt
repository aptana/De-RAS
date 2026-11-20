# De-RAS desktop application (Qt 6 Widgets)
find_package(Qt6 REQUIRED COMPONENTS Widgets)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
# Host rcc may support zstd while the target Qt was built without it; PNGs gain nothing from it anyway.
set(CMAKE_AUTORCC_OPTIONS "--no-zstd")

file(GLOB DERAS_UI_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp ${CMAKE_CURRENT_SOURCE_DIR}/*.h)
list(FILTER DERAS_UI_SOURCES EXCLUDE REGEX ".*/main\\.cpp$")

# Resources: official icons (copied unchanged from branding/icons) and the help catalogue.
add_library(deras_ui STATIC ${DERAS_UI_SOURCES})
target_link_libraries(deras_ui PUBLIC deras_core Qt6::Widgets)
target_include_directories(deras_ui PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(De-RAS WIN32 main.cpp ${CMAKE_CURRENT_SOURCE_DIR}/deras.qrc)
target_link_libraries(De-RAS PRIVATE deras_ui)
deras_add_win_resource(De-RAS De-RAS "De-RAS river hydraulics" ON)
if(MINGW)
  target_link_options(De-RAS PRIVATE -static-libgcc -static-libstdc++)
endif()

# Developer tool: offscreen screenshots of the UI (not installed).
if(NOT WIN32)
  add_executable(deras-ui-snapshot ${CMAKE_SOURCE_DIR}/tools/ui/ui_snapshot.cpp ${CMAKE_CURRENT_SOURCE_DIR}/deras.qrc)
  target_link_libraries(deras-ui-snapshot PRIVATE deras_ui)
  # View history test (review R3 of build 0.2.0), offscreen.
  find_package(Qt6 COMPONENTS Test)
  if(Qt6Test_FOUND AND DERAS_BUILD_TESTS)
    add_executable(test_ui_history ${CMAKE_SOURCE_DIR}/tests/test_ui_history.cpp ${CMAKE_CURRENT_SOURCE_DIR}/deras.qrc)
    target_include_directories(test_ui_history PRIVATE ${CMAKE_SOURCE_DIR}/tests)
    target_compile_definitions(test_ui_history PRIVATE DERAS_TEST_CUSTOM_MAIN)
    target_link_libraries(test_ui_history PRIVATE deras_ui Qt6::Test)
    add_test(NAME test_ui_history COMMAND test_ui_history)
  endif()
endif()
if(NOT MSVC)
  target_compile_options(deras_ui PRIVATE -Wno-missing-field-initializers)
endif()
