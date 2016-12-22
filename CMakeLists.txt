# enforce the right cmake version and module path
cmake_minimum_required (VERSION 2.8.11)
set (CMAKE_MODULE_PATH "${CMAKE_INSTALL_PREFIX}/share/cmake/modules")

project(firstaid)

# set compile flags for AbsInt internal compilation, to enforce right compilers/flags
if(ABSINT)
    # this sets the right compile flags and exports macros like for code-signing
    find_package (UR REQUIRED)
    include (${UR_USE_FILE})

    # static linking
    if (WIN32)
        add_definitions (-DAI_LINK_QT_STATIC_WINDOWS)
    elseif (APPLE)
        add_definitions (-DAI_LINK_QT_STATIC_APPLE)
    else()
        add_definitions (-DAI_LINK_QT_STATIC_LINUX)
    endif()
endif()

# Find includes in corresponding build directories
set (CMAKE_INCLUDE_CURRENT_DIR ON)

# Instruct CMake to run moc automatically when needed.
set (CMAKE_AUTOMOC ON)

find_package(Qt5Concurrent)
find_package(Qt5Core)
find_package(Qt5Widgets)
find_package(Qt5PrintSupport)
find_package(Qt5Xml)

include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${Qt5Core_INCLUDE_DIRS}
  ${Qt5Widgets_INCLUDE_DIRS}
  ${Qt5Gui_INCLUDE_DIRS}
  ${Qt5Xml_INCLUDE_DIRS}
  ../libpoppler/qt5/src
)

set(firstaid_SRCS
  abstractinfodock.cpp
  documentobserver.cpp
  embeddedfiles.cpp
  fonts.cpp
  info.cpp
  main_viewer.cpp
  metadata.cpp
  navigationtoolbar.cpp
  optcontent.cpp
  pageview.cpp
  permissions.cpp
  thumbnails.cpp
  toc.cpp
  viewer.cpp
)

IF(WIN32)
    SET(firstaid_SRCS ${firstaid_SRCS} firstaid.rc)
ENDIF(WIN32)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${Qt5Core_EXECUTABLE_COMPILE_FLAGS} ${Qt5Widgets_EXECUTABLE_COMPILE_FLAGS}")

add_executable(firstaid ${firstaid_SRCS} ${firstaid_UI_SRCS} ${firstaid_QRC_SRCS})
set_property(TARGET firstaid PROPERTY WIN32_EXECUTABLE true)

target_link_libraries(firstaid -lpoppler-qt5 -lpoppler -lz -ljpeg Qt5::Core Qt5::Widgets Qt5::Gui Qt5::PrintSupport Qt5::Concurrent Qt5::Xml)
install(PROGRAMS firstaid DESTINATION bin)

# sign result binary for AbsInt internal compilation
if(ABSINT)
    ur_install_win_executable (firstaid)
endif()
