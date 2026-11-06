# libssh2 (git master) as an ESP-IDF component, using the mbedTLS 4 / PSA backend.
# Sources come from the git submodule ./libssh2 (tracks master, run
# `git submodule update --init --remote` to update); can be overridden with the
# LIBSSH2_DIR environment variable.
if(DEFINED ENV{LIBSSH2_DIR})
    set(LIBSSH2_DIR "$ENV{LIBSSH2_DIR}")
else()
    set(LIBSSH2_DIR "${CMAKE_CURRENT_LIST_DIR}/libssh2")
endif()

if(NOT EXISTS "${LIBSSH2_DIR}/include/libssh2.h")
    message(FATAL_ERROR "libssh2 sources not found in ${LIBSSH2_DIR}, run: git submodule update --init")
endif()

# every crypto backend source is guarded by its LIBSSH2_<backend> define
file(GLOB libssh2_srcs "${LIBSSH2_DIR}/src/*.c")

idf_component_register(SRCS ${libssh2_srcs}
                       INCLUDE_DIRS "include" "${LIBSSH2_DIR}/include"
                       PRIV_INCLUDE_DIRS "${LIBSSH2_DIR}/src"
                       REQUIRES mbedtls lwip)

target_compile_definitions(${COMPONENT_LIB} PRIVATE HAVE_CONFIG_H LIBSSH2_MBEDTLS)
target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-maybe-uninitialized -Wno-unused-function)
