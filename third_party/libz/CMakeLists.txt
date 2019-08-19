include(ExternalProject)

ExternalProject_Add(
        libz_project
        PREFIX ${CMAKE_CURRENT_SOURCE_DIR}
        URL http://10.199.1.14:10000/other/zlib-1.2.11.tar.gz
        URL_HASH SHA1=e6d119755acdf9104d7ba236b1242696940ed6dd
        DOWNLOAD_NAME zlib-1.2.11.tar.gz
        CONFIGURE_COMMAND <SOURCE_DIR>/configure
        BUILD_COMMAND make -j 10
        INSTALL_COMMAND mkdir -p <INSTALL_DIR>/lib
                && mkdir -p <INSTALL_DIR>/include
                && cp -av <BINARY_DIR>/libz.a <INSTALL_DIR>/lib
                && cp <SOURCE_DIR>/zlib.h <INSTALL_DIR>/include
                && cp <SOURCE_DIR>/zconf.h <INSTALL_DIR>/include
)

ExternalProject_Get_Property(libz_project install_dir)

add_library(libz STATIC IMPORTED GLOBAL)
add_dependencies(libz libz_project)
set_target_properties(libz PROPERTIES
        IMPORTED_LOCATION "${install_dir}/lib/libz.a"
        INCLUDE_DIRECTORIES "${install_dir}/include"
        )