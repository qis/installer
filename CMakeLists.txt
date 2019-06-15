cmake_minimum_required(VERSION 3.14.0 FATAL_ERROR)
project(installer DESCRIPTION "Installer" VERSION 0.1.0 LANGUAGES CXX)

set(PROJECT_VENDOR "Xiphos")
set(PROJECT_COPYRIGHT "2019 Alexej Harm")
configure_file(res/version.rc.in ${CMAKE_BINARY_DIR}/src/version.rc CRLF)

add_definitions(-D_UNICODE -DUNICODE -DWIN32_LEAN_AND_MEAN -DNOMINMAX -DWINVER=0x0A00 -D_WIN32_WINNT=0x0A00)
add_definitions(-D_CRT_SECURE_NO_DEPRECATE -D_CRT_SECURE_NO_WARNINGS -D_CRT_NONSTDC_NO_DEPRECATE)
add_definitions(-D_ATL_SECURE_NO_DEPRECATE -D_SCL_SECURE_NO_WARNINGS -D_VERSION_RC)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set_property(GLOBAL PROPERTY PREDEFINED_TARGETS_FOLDER build)
set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT ${PROJECT_NAME})

file(GLOB_RECURSE headers CONFIGURE_DEPENDS src/*.hpp)
file(GLOB_RECURSE sources CONFIGURE_DEPENDS src/*.cpp src/main.rc src/main.manifest)
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}/src PREFIX "" FILES ${headers} ${sources})

add_executable(${PROJECT_NAME} WIN32 ${headers} ${sources})
set_target_properties(${PROJECT_NAME} PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_BINARY_DIR}/src src)
target_include_directories(${PROJECT_NAME} PRIVATE "C:/Program Files (x86)/Windows Mobile 6 SDK/Activesync/inc")
target_link_libraries(${PROJECT_NAME} PRIVATE comctl32)

install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION .)
install(CODE [[
  file(GLOB libraries ${CMAKE_BINARY_DIR}/*.dll ${CMAKE_BINARY_DIR}/Release/*.dll)
  file(INSTALL ${libraries} DESTINATION ${CMAKE_INSTALL_PREFIX} PATTERN "gtest*.dll" EXCLUDE)
]])
