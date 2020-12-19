#
# Basics: Right cmake version and right path for modules
#
cmake_minimum_required (VERSION 3.16)
set (CMAKE_MODULE_PATH "${CMAKE_INSTALL_PREFIX}/share/cmake/modules")

# FirstAid - PDF Help Viewer
project(firstaid)

# set compile flags for AbsInt internal compilation, to enforce right compilers/flags
if(ABSINT)
    # this sets the right compile flags and exports macros like for code-signing
    find_package (UR REQUIRED)
    include (${UR_USE_FILE})

    # prefer local poppler
    include_directories(${CMAKE_INSTALL_PREFIX}/include/poppler/qt6)

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

# Instruct CMake to run moc automatically when needed.
set (CMAKE_AUTOMOC ON)

# needed Qt parts
find_package(Qt6Concurrent)
find_package(Qt6Core)
find_package(Qt6Widgets)
find_package(Qt6PrintSupport)
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
)

set(link_checker_SRCS
    link_checker/main.cpp
)

qt6_add_resources(firstaid_QRC_SRCS firstaid.qrc)

# embedd icon on Windows
IF(WIN32)
    SET(firstaid_SRCS ${firstaid_SRCS} firstaid.rc)
ENDIF(WIN32)

# build FirstAid binary
add_executable(firstaid ${firstaid_SRCS} ${firstaid_UI_SRCS} ${firstaid_QRC_SRCS})

# link, we assume static libs on Windows ATM
if (WIN32)
    target_link_libraries(firstaid poppler-qt6.lib poppler.lib freetype.lib zlib.lib Qt6::Core Qt6::Widgets Qt6::Gui Qt6::PrintSupport Qt6::Concurrent Qt6::Xml)
elseif (APPLE)
    target_link_libraries(firstaid -lpoppler-qt6 -lpoppler -lfontconfig -lfreetype -lbz2 -lexpat -lz Qt6::Core Qt6::Widgets Qt6::Gui Qt6::PrintSupport Qt6::Concurrent Qt6::Xml)
else()
    target_link_libraries(firstaid -lpoppler-qt6 -lpoppler -lfontconfig -lfreetype Qt6::Core Qt6::Widgets Qt6::Gui Qt6::PrintSupport Qt6::Concurrent Qt6::Xml)
endif()

# build linkChecker binary
add_executable(linkChecker ${link_checker_SRCS})

# link, we assume static libs on Windows ATM
if (WIN32)
    target_link_libraries(linkChecker poppler-qt6.lib poppler.lib  freetype.lib zlib.lib Qt6::Core Qt6::PrintSupport Qt6::Xml)
elseif (APPLE)
    target_link_libraries(linkChecker -lpoppler-qt6 -lpoppler -lfontconfig -lfreetype -lbz2 -lexpat -lz Qt6::Core Qt6::PrintSupport Qt6::Xml)
else()
    target_link_libraries(linkChecker -lpoppler-qt6 -lpoppler -lfontconfig -lfreetype Qt6::Core Qt6::PrintSupport Qt6::Xml)
endif()

# install viewer to prefix
install(TARGETS firstaid DESTINATION bin)
install(TARGETS linkChecker DESTINATION bin)

# AbsInt internal stuff
if(ABSINT)
    # sign result binary for AbsInt internal compilation
    ur_install_win_executable (firstaid)

    #
    # generate "make reformat" target for given 'file-list bash expression'
    # here: all *.cpp + *.h files
    #
    ur_reformat ("`git ls-files | grep -e \".*\\\\.cpp$\" -e \".*\\\\.h$\" | xargs`")

    #
    # generate documentation
    #
    ur_gendoc (firstaid Doxyfile doc ${firstaid_SRCS})
endif()
