cmake_minimum_required(VERSION 2.8) # DO NOT UPGRADE THIS OR YOU WILL LOSE wxWidgets 2.8 DEBUG BUILDS!

project(Kuvastin)

set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake/)
set(CMAKE_CXX_FLAGS "-Wall -Wno-sign-compare -std=gnu++0x")
#set(BOOST_DATE_TIME_NO_LIB   ON)
#set(Boost_USE_STATIC_LIBS    ON)
#set(Boost_USE_MULTITHREADED  ON)
#set(Boost_USE_STATIC_RUNTIME ON)

#find_package(Boost 1.55 REQUIRED iostreams)

find_package(wxWidgets REQUIRED adv core base)
include("${wxWidgets_USE_FILE}")

#include_directories(${Boost_INCLUDE_DIRS})

set(Kuvastin_SRC main.cpp frame.cpp resources.rc)

add_executable(Kuvastin WIN32 ${Kuvastin_SRC})
target_link_libraries(Kuvastin ${wxWidgets_LIBRARIES})
