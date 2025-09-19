
include(GNUInstallDirs)

add_library(jsnpg_static STATIC jsnpg.c)
set_target_properties(jsnpg_static PROPERTIES
        VERSION ${PROJECT_VERSION}
        PUBLIC_HEADER include/jsnpg.h)

add_library(jsnpg SHARED jsnpg.c)
set_target_properties(jsnpg PROPERTIES 
        VERSION ${PROJECT_VERSION}
        SOVERSION ${PROJECT_VERSION_MAJOR}
        PUBLIC_HEADER include/jsnpg.h)

configure_file(jsnpg.pc.in jsnpg.pc @ONLY)

set(PUB_HDRS, include/def_gen_macros.h include/undef_gen_macros.h)

install(TARGETS jsnpg jsnpg_static
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/jsnpg)
install(FILES ${PUB_HDRS}
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/jsnpg)
install(FILES ${CMAKE_BINARY_DIR}/src/jsnpg.pc
    DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/pkgconfig)
