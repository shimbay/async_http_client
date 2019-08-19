include(ExternalProject)

ExternalProject_Add(
        libcurl_project
        PREFIX ${CMAKE_CURRENT_SOURCE_DIR}
        URL http://10.199.1.14:10000/other/curl-7.65.3.tar.gz
        URL_HASH SHA1=6468eea5e52ae0aac93f3995ec8e545ecb96f9e9
        DOWNLOAD_NAME curl-7.65.3.tar.gz
        CONFIGURE_COMMAND <SOURCE_DIR>/configure
        BUILD_COMMAND make -j 10
        INSTALL_COMMAND mkdir -p <INSTALL_DIR>/lib
                && cp -av <BINARY_DIR>/lib/.libs/libcurl.a <INSTALL_DIR>/lib
                && cp -r <SOURCE_DIR>/include <INSTALL_DIR>
)

ExternalProject_Get_Property(libcurl_project install_dir)

add_library(libcurl STATIC IMPORTED GLOBAL)
add_dependencies(libcurl libz libcurl_project)
set_target_properties(libcurl PROPERTIES
        IMPORTED_LOCATION "${install_dir}/lib/libcurl.a"
        INCLUDE_DIRECTORIES "${install_dir}/include"
        )
