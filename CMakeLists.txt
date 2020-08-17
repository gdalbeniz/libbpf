# XXXXXXX

cmake_minimum_required (VERSION 3.16)

set(LIBBPF_VERSION_MAJOR "0")
set(LIBBPF_VERSION_MINOR "0")
set(LIBBPF_VERSION_REVISION "9")
set(LIBBPF_VERSION ${LIBBPF_VERSION_MAJOR}.${LIBBPF_VERSION_MINOR}.${LIBBPF_VERSION_REVISION})

project(libbpf VERSION ${LIBBPF_VERSION} LANGUAGES C)

find_path(IEC61850_INCLUDE_DIR sv_publisher.h PATH_SUFFIXES libiec61850 REQUIRED)
find_library(IEC61850_LIBRARY libiec61850.a REQUIRED)
find_library(IEC61850HAL_LIBRARY libhal.a REQUIRED)


message("IEC61850_INCLUDE_DIR include dir = ${IEC61850_INCLUDE_DIR}")
message("IEC61850_LIBRARY lib = ${IEC61850_LIBRARY}")
message("IEC61850HAL_LIBRARY lib = ${IEC61850HAL_LIBRARY}")

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/include/uapi
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/ini
    ${CMAKE_CURRENT_SOURCE_DIR}/libbpf
    ${IEC61850_INCLUDE_DIR}
)

set(xdpsock_sv_SRCS
   src/xdpsock_user.c
   src/main.c
)
set(libbpf_SRCS
   libbpf/bpf.c
   libbpf/netlink.c
   libbpf/nlattr.c
   libbpf/xsk.c
   libbpf/libbpf_print.c
)
add_executable(xdpsock_sv
  ${libbpf_SRCS}
  ${xdpsock_sv_SRCS}
)
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)
target_link_libraries(xdpsock_sv PRIVATE Threads::Threads)

set(sv_injector_SRCS
   ini/ini.c
   src/sv_config.c
   src/sv_frames.c
   src/sv_injector.c
)
add_executable(sv_injector
  ${sv_injector_SRCS}
)
target_link_libraries(sv_injector
  PRIVATE m
  PRIVATE ${IEC61850_LIBRARY}
  PRIVATE ${IEC61850HAL_LIBRARY}
)



install (TARGETS xdpsock_sv RUNTIME DESTINATION ${BINDIR} COMPONENT Applications)

install (TARGETS sv_injector RUNTIME DESTINATION ${BINDIR} COMPONENT Applications)
install (FILES test.ini DESTINATION etc COMPONENT Applications)