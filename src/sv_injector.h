#ifndef __SV_INJECTOR__
#define __SV_INJECTOR__


#define _GNU_SOURCE

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <errno.h>
#include <locale.h>
#include <signal.h>
#include <stdbool.h>
#include <poll.h>
#include <pthread.h>
#include <getopt.h>

#include <asm/barrier.h>
#include <libgen.h>
#include <linux/bpf.h>
#include <linux/compiler.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <sys/socket.h>
#include <net/if.h>
#include <linux/if_packet.h>
#include <linux/if_ether.h>
#include <linux/if_xdp.h>
#include <linux/if_link.h>
#include <net/ethernet.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <arpa/inet.h>
#include <sched.h>

#include "sv_publisher.h"
#include "hal_ethernet.h"
#include "hal_thread.h"

#include "xsk.h"
#include "bpf.h"


void debug(const char *fmt, ...);
//void __exit_with_error(int error, const char *file, const char *func, int line);
//#define exit_with_error(error) __exit_with_error(error, __FILE__, __func__, __LINE__)

#endif