#
# Basics: Right cmake version
#
cmake_minimum_required (VERSION 3.16)

# FirstAid - PDF Help Viewer
project(firstaid)

# set compile flags for AbsInt internal compilation, to enforce right compilers/flags
if(ABSINT)
    # this sets the right compile flags and exports macros like for code-signing
    find_package (UR REQUIRED)
    include (${UR_USE_FILE})

    # prefer local poppler
    include_directories(${UR_INCLUDE_DIR}/poppler/qt6)

    # setup AbsInt release info
    set (FIRSTAID_BUILD ${UR_BUILD})
    set (FIRSTAID_TAG ${UR_TAG})
    set (FIRSTAID_RELEASE ${UR_RELEASE})
else()
    # prefer system poppler
    include_directories(/usr/include/poppler/qt6)

    # setup public release info, fill in values if you like to have this in About
    set (FIRSTAID_BUILD "")
    set (FIRSTAID_TAG "")
    set (FIRSTAID_RELEASE "")
endif()

# config.h, e.g. for version
configure_file (config.h.cmake config.h)

# Find includes in corresponding build directories
set (CMAKE_INCLUDE_CURRENT_DIR ON)

# Instruct CMake to run moc & rcc automatically when needed.
set (CMAKE_AUTOMOC ON)
set (CMAKE_AUTORCC ON)

# needed Qt parts
find_package(Qt6Concurrent)
find_package(Qt6Core)
find_package(Qt6Network)
find_package(Qt6PrintSupport)
find_package(Qt6Widgets)
find_package(Qt6Xml)

set(firstaid_SRCS
  src/document.cpp
  src/document.h
  src/findbar.cpp
  src/findbar.h
  src/helpdialog.cpp
  src/helpdialog.h
  src/historystack.cpp
  src/historystack.h
  src/main.cpp
  src/navigationtoolbar.cpp
  src/navigationtoolbar.h
  src/pageview.cpp
  src/pageview.h
  src/searchengine.cpp
  src/searchengine.h
  src/tocdock.cpp
  src/tocdock.h
  src/viewer.cpp
  src/viewer.h
  firstaid.qrc
)

# build FirstAid binary, GUI app & embed icon on Windows
if (WIN32)
    add_executable(firstaid WIN32 ${firstaid_SRCS} firstaid.rc)
else ()
    add_executable(firstaid ${firstaid_SRCS})
endif ()

# link, we assume static libs on Windows ATM
if (WIN32)
    target_link_libraries(firstaid poppler-qt6.lib poppler.lib freetype.lib zlib.lib Qt6::Core Qt6::Widgets Qt6::Gui Qt6::Network Qt6::PrintSupport Qt6::Concurrent Qt6::Xml)
elseif (APPLE)
    target_link_libraries(firstaid -lpoppler-qt6 -lpoppler -lfontconfig -lfreetype -lexpat -lz Qt6::Core Qt6::Widgets Qt6::Gui Qt6::Network Qt6::PrintSupport Qt6::Concurrent Qt6::Xml)
else()
    target_link_libraries(firstaid -lpoppler-qt6 -lpoppler -lfontconfig -lfreetype -lexpat -lz Qt6::Core Qt6::Widgets Qt6::Gui Qt6::Network Qt6::PrintSupport Qt6::Concurrent Qt6::Xml)
endif()

set(link_checker_SRCS
    link_checker/main.cpp
)

# build linkChecker binary
add_executable(linkChecker ${link_checker_SRCS})

# link, we assume static libs on Windows ATM
if (WIN32)
    target_link_libraries(linkChecker poppler-qt6.lib poppler.lib freetype.lib zlib.lib Qt6::Core Qt6::PrintSupport Qt6::Xml)
elseif (APPLE)
    target_link_libraries(linkChecker -lpoppler-qt6 -lpoppler -lfontconfig -lfreetype -lexpat -lz Qt6::Core Qt6::PrintSupport Qt6::Xml)
else()
    target_link_libraries(linkChecker -lpoppler-qt6 -lpoppler -lfontconfig -lfreetype -lexpat -lz Qt6::Core Qt6::PrintSupport Qt6::Xml)
endif()

# install viewer to prefix
install(TARGETS firstaid DESTINATION bin)
install(TARGETS linkChecker DESTINATION bin)

# AbsInt internal stuff
if(ABSINT)
    # install pdbs
    ur_install_win_executable (firstaid)
    ur_install_win_executable (linkChecker)

    #
    # generate documentation
    #
    ur_gendoc (firstaid Doxyfile doc ${firstaid_SRCS})
endif()
