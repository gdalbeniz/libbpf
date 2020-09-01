
cmake_minimum_required (VERSION 3.16)

set(SVINJECTOR_VERSION_MAJOR "0")
set(SVINJECTOR_VERSION_MINOR "99")
set(SVINJECTOR_VERSION ${SVINJECTOR_VERSION_MAJOR}.${SVINJECTOR_VERSION_MINOR})

add_compile_definitions(SVINJECTOR_VERSION="${SVINJECTOR_VERSION}")

project(sv_injector VERSION ${SVINJECTOR_VERSION} LANGUAGES C)

find_path(IEC61850_INCLUDE_DIR sv_publisher.h PATH_SUFFIXES libiec61850 REQUIRED)
find_library(IEC61850_LIBRARY libiec61850.a REQUIRED)
find_library(IEC61850HAL_LIBRARY libhal.a REQUIRED)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/include/uapi
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/inih
    ${CMAKE_CURRENT_SOURCE_DIR}/libbpf
    ${IEC61850_INCLUDE_DIR}
)

set(libbpf_SRCS
   libbpf/bpf.c
   libbpf/netlink.c
   libbpf/nlattr.c
   libbpf/xsk.c
   libbpf/libbpf_print.c
)

set(inih_SRCS
   inih/ini.c
)

set(sv_injector_SRCS
   src/sv_config.c
   src/sv_frames.c
   src/sv_packet.c
   src/sv_xdp.c
   src/sv_injector.c
)

add_executable(sv_injector
  ${sv_injector_SRCS}
  ${inih_SRCS}
  ${libbpf_SRCS}
)

target_link_libraries(sv_injector
  PRIVATE m
  PRIVATE ${IEC61850_LIBRARY}
  PRIVATE ${IEC61850HAL_LIBRARY}
  PRIVATE Threads::Threads
)

install (TARGETS sv_injector RUNTIME DESTINATION ${BINDIR} COMPONENT Applications)
install (FILES test.ini DESTINATION etc COMPONENT Applications)