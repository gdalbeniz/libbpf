# XXXXXXX

cmake_minimum_required (VERSION 3.2)

set(LIBBPF_VERSION_MAJOR "0")
set(LIBBPF_VERSION_MINOR "0")
set(LIBBPF_VERSION_REVISION "9")
set(LIBBPF_VERSION ${LIBBPF_VERSION_MAJOR}.${LIBBPF_VERSION_MINOR}.${LIBBPF_VERSION_REVISION})

project(libbpf VERSION ${LIBBPF_VERSION} LANGUAGES C)


include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/include/uapi
)

set(xdpsock_sv_SRCS
   xdpsock_user.c
   src/bpf.c
   src/netlink.c
   src/nlattr.c
   src/xsk.c
)

add_executable(xdpsock_sv
  ${xdpsock_sv_SRCS}
)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)
target_link_libraries(xdpsock_sv PRIVATE Threads::Threads)

install (TARGETS xdpsock_sv RUNTIME DESTINATION ${BINDIR} COMPONENT Applications)