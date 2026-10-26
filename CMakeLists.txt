cmake_minimum_required(VERSION 3.16 FATAL_ERROR)
project(wxSysInfoFrameTest CXX)

if(NOT(WIN32 OR UNIX))
  message(FATAL_ERROR "${PROJECT_NAME} does not support this platform.")
endif()

if(WIN32)
  # for the resource file with the application manifest
  enable_language(RC)
endif()

find_package(wxWidgets 3.2 COMPONENTS core base REQUIRED)

if(UNIX AND NOT APPLE)
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(GTK3 REQUIRED gtk+-3.0)
endif()

add_executable(${PROJECT_NAME}
  test/main.cpp
  code/wxsysinfoframe.cpp
  code/wxsysinfoframe.h
)
if(WIN32)
  target_sources(${PROJECT_NAME} PRIVATE test/test.rc)
endif()
set_target_properties(${PROJECT_NAME} PROPERTIES
  CXX_STANDARD 11
  CXX_STANDARD_REQUIRED YES
  CXX_EXTENSIONS NO
)
target_include_directories(${PROJECT_NAME} PRIVATE code)

if(wxWidgets_USE_FILE)
  include(${wxWidgets_USE_FILE})
endif()
target_link_libraries(${PROJECT_NAME} PRIVATE ${wxWidgets_LIBRARIES})

if(WIN32)
  set_target_properties(${PROJECT_NAME} PROPERTIES WIN32_EXECUTABLE YES)

  # wxsysinfoframe.cpp uses the theme API directly and wxWidgets DLLs do not bring uxtheme in
  target_link_libraries(${PROJECT_NAME} PRIVATE uxtheme)

  if(MSVC)
    target_compile_definitions(${PROJECT_NAME} PRIVATE
      _CRT_SECURE_NO_DEPRECATE _CRT_NON_CONFORMING_SWPRINTFS _SCL_SECURE_NO_WARNINGS)
    target_compile_options(${PROJECT_NAME} PRIVATE /W4 /MP)
    # the application manifest is embedded from test.rc, so the linker must not add its own
    target_link_options(${PROJECT_NAME} PRIVATE /MANIFEST:NO)
  else() # MinGW GCC or clang
    target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -Wno-deprecated-declarations)
    # wxWidgets 3.3 needs these when linking statically
    if(MINGW)
      target_link_libraries(${PROJECT_NAME} PRIVATE gdiplus msimg32)
    endif()
  endif()
elseif(APPLE)
  target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -Wno-deprecated-declarations)
else() # Linux and other Unix systems using GTK
  target_include_directories(${PROJECT_NAME} PRIVATE ${GTK3_INCLUDE_DIRS})
  target_link_libraries(${PROJECT_NAME} PRIVATE ${GTK3_LIBRARIES})
  target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -Wno-deprecated-declarations ${GTK3_CFLAGS_OTHER})
endif()
