

INCLUDES := -I. -I./src -I./include -I./include/uapi
ALL_CFLAGS := $(INCLUDES)

# if build fails because it needs reallocarray turn on: ALL_CFLAGS += -DCOMPAT_NEED_REALLOCARRAY

# SHARED_CFLAGS += -fPIC -fvisibility=hidden -DSHARED

CFLAGS ?= -g -O2 -Werror -Wall
ALL_CFLAGS += $(CFLAGS)
# ^was: ALL_CFLAGS += $(CFLAGS) -D_LARGEFILE64_SOURCE -D_FILE_OFFSET_BITS=64

ALL_LDFLAGS += $(LDFLAGS) -pthread
# needed?: ALL_LDFLAGS += -lelf -lz


BUILDDIR ?= ./build
OBJS :=	bpf.o \
		netlink.o \
		nlattr.o \
		xsk.o
# OBJS := bpf.o \
# 		btf.o  \
# 		libbpf.o \
# 		libbpf_errno.o \
# 		netlink.o \
# 		nlattr.o \
# 		str_error.o \
# 		libbpf_probes.o \
# 		bpf_prog_linfo.o \
# 		xsk.o \
# 		btf_dump.o \
# 		hashmap.o \
# 		ringbuf.o

STATIC_OBJS := $(addprefix $(BUILDDIR)/,$(OBJS))


all: xdpsock_sv

$(BUILDDIR):
	mkdir -p $(BUILDDIR)

$(BUILDDIR)/%.o: src/%.c | $(BUILDDIR)
	$(CC) $(ALL_CFLAGS) $(CPPFLAGS) -c $< -o $@

xdpsock_sv: xdpsock_user.c $(STATIC_OBJS)
	$(CC) $(ALL_CFLAGS) $(CPPFLAGS) $(ALL_LDFLAGS) $^ -o $@

clean:
	rm -rf xdpsock_sv *.o $(BUILDDIR)