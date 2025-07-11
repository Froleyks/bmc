include(ExternalProject)
set(AIGER_TOOLS aigtocnf aigsim aigfuzz aigdd aigtoaig)
set(patch "${CMAKE_CURRENT_LIST_DIR}/static.patch")

ExternalProject_Add(
  aiger_tools
  GIT_REPOSITORY https://github.com/arminbiere/aiger
  BUILD_IN_SOURCE 1
  UPDATE_COMMAND ""
  CONFIGURE_COMMAND /bin/sh -c "env CFLAGS='-O3 -DNDEBUG -static' ./configure.sh"
  BUILD_COMMAND make -j ${AIGER_TOOLS}
  INSTALL_COMMAND cp ${AIGER_TOOLS} ${CMAKE_CURRENT_BINARY_DIR} && make clean)

foreach(tool IN LISTS AIGER_TOOLS)
  install(PROGRAMS ${CMAKE_CURRENT_BINARY_DIR}/${tool} TYPE BIN)
endforeach()
