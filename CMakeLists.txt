cmake_minimum_required(VERSION 2.8.11)

project(firstaid)

add_definitions(${QT5_DEFINITIONS})
add_definitions(${Qt5Core_DEFINITIONS})
add_definitions(${Qt5Gui_DEFINITIONS})
add_definitions(${Qt5Widgets_DEFINITIONS})
add_definitions(${Qt5Xml_DEFINITIONS})

include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/../src
  ${CMAKE_CURRENT_BINARY_DIR}
  ${Qt5Core_INCLUDE_DIRS}
  ${Qt5Widgets_INCLUDE_DIRS}
  ${Qt5Gui_INCLUDE_DIRS}
  ${Qt5Xml_INCLUDE_DIRS}
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

target_link_libraries(firstaid ${Qt5Core_LIBRARIES} ${Qt5Gui_LIBRARIES} ${Qt5Widgets_LIBRARIES} ${Qt5Xml_LIBRARIES})
