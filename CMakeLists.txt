# enforce the right cmake version and module path
cmake_minimum_required (VERSION 2.8.11)
set (CMAKE_MODULE_PATH "${CMAKE_INSTALL_PREFIX}/share/cmake/modules")

# FirstAid - PDF Help Viewer
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

# needed Qt parts
find_package(Qt5Concurrent)
find_package(Qt5Core)
find_package(Qt5Widgets)
find_package(Qt5PrintSupport)
find_package(Qt5Xml)

# prefer local poppler
include_directories(${CMAKE_INSTALL_PREFIX}/include/poppler/qt5)

set(firstaid_SRCS
  abstractinfodock.cpp
  documentobserver.cpp
  embeddedfiles.cpp
  fonts.cpp
  info.cpp
  main.cpp
  metadata.cpp
  navigationtoolbar.cpp
  optcontent.cpp
  pageview.cpp
  permissions.cpp
  thumbnails.cpp
  toc.cpp
  viewer.cpp
)

qt5_add_resources(firstaid_QRC_SRCS firstaid.qrc)

# embedd icon on Windows
IF(WIN32)
    SET(firstaid_SRCS ${firstaid_SRCS} firstaid.rc)
ENDIF(WIN32)

# build FirstAid binary
add_executable(firstaid ${firstaid_SRCS} ${firstaid_UI_SRCS} ${firstaid_QRC_SRCS})

# link, we assume static libs on Windows ATM
if (WIN32)
    target_link_libraries(firstaid poppler-qt5.lib poppler.lib freetype.lib Qt5::Core Qt5::Widgets Qt5::Gui Qt5::PrintSupport Qt5::Concurrent Qt5::Xml)
else()
    target_link_libraries(firstaid -lpoppler-qt5 -lpoppler Qt5::Core Qt5::Widgets Qt5::Gui Qt5::PrintSupport Qt5::Concurrent Qt5::Xml)
endif()

# install viewer to prefix
install(TARGETS firstaid DESTINATION bin)

# sign result binary for AbsInt internal compilation
if(ABSINT)
    ur_install_win_executable (firstaid)
endif()
