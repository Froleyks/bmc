include(ExternalProject)
set(CADICAL_GIT_TAG master CACHE STRING "CaDiCaL git commit hash or tag to checkout")

set(CADICAL_DIR ${CMAKE_BINARY_DIR}/cadical-prefix/src/cadical)
set(CADICAL_INCLUDE_DIR ${CADICAL_DIR}/src)
set(CADICAL_LIB ${CADICAL_DIR}/build/libcadical.a)

ExternalProject_Add(
  cadical
  GIT_REPOSITORY https://github.com/arminbiere/cadical
  GIT_TAG ${CADICAL_GIT_TAG}
  CONFIGURE_COMMAND ./configure
  BUILD_COMMAND make -j
  BUILD_IN_SOURCE 1
  UPDATE_COMMAND ""
  INSTALL_COMMAND "")
