cmake_minimum_required(VERSION 3.5)
project(async_http_client)

# Set pkg-config dependencies
set(deps GTEST LIBEV)
set(dep_pkg_names gtest libev)

find_package(PkgConfig REQUIRED)
list(LENGTH deps deps_size)
list(LENGTH dep_pkg_names dep_names_size)
if (NOT deps_size EQUAL dep_names_size)
    message(FATAL_ERROR "Dependencies size (${deps_size}) must be equal to dependency pkg-config names size (${dep_names_size})")
endif ()

# Check pkg_config deps
include_directories(. third_party/conlib)
set(COMMON_LINK_LIBRARIES dl pthread)
math(EXPR deps_size "${deps_size} - 1")
foreach (i RANGE ${deps_size})
    list(GET deps ${i} dep)
    list(GET dep_pkg_names ${i} name)

    pkg_check_modules(${dep} REQUIRED ${name})
    include_directories(${${dep}_INCLUDE_DIRS})
    link_directories(${${dep}_LIBRARY_DIRS})
    list(APPEND COMMON_LINK_LIBRARIES ${${dep}_LIBRARIES})
    message("${dep} Include Dir: ${${dep}_INCLUDE_DIRS}")
    message("${dep} Lib Dir: ${${dep}_LIBRARY_DIRS}")
    message("${dep} Libs: ${${dep}_LIBRARIES}")
endforeach (i)

list(APPEND other_directories third_party/conlib/common)
list(APPEND other_directories third_party/conlib/concurrent)
list(APPEND other_directories third_party/conlib/queue)

list(LENGTH other_directories others_size)
math(EXPR others_size "${others_size} - 1")
foreach (i RANGE ${others_size})
    list(GET other_directories ${i} other)
    file(GLOB_RECURSE tmp_files ${other}/*.h ${other}/*.hpp ${other}/*.c ${other}/*.cpp)
    list(APPEND OTHERS ${tmp_files})
endforeach (i)

file(GLOB_RECURSE SRCS async_http_client/*.h async_http_client/*.cpp)
file(GLOB_RECURSE TESTS test/*.cpp)

set(CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS} "-std=c++14 -fPIC -ggdb -Wno-deprecated-declarations")

add_subdirectory(third_party)

add_executable(async_http_client.core ${TESTS} ${SRCS} ${OTHERS})
add_dependencies(async_http_client.core libz libcurl)
set_target_properties(async_http_client.core PROPERTIES
        COMPILE_FLAGS "-O3 -fPIC"
        )
#target_include_directories(
#        async_http_client.core
#        PUBLIC $<TARGET_PROPERTY:libcurl,INCLUDE_DIRECTORIES>
#)
target_link_libraries(
        async_http_client.core
        PRIVATE libcurl
        PRIVATE libz
        PUBLIC ${COMMON_LINK_LIBRARIES}
)

#add_executable(async_http_client.test ${TESTS} async_http_client.core)
