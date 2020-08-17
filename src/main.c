
#include <asm/barrier.h>
#include <errno.h>
#include <getopt.h>
#include <libgen.h>
#include <linux/bpf.h>
#include <linux/compiler.h>
#include <linux/if_link.h>
#include <linux/if_xdp.h>
#include <linux/if_ether.h>

#include <locale.h>
#include <net/ethernet.h>
#include <net/if.h>
#include <poll.h>
#include <pthread.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include <arpa/inet.h>

#include "libbpf.h"
#include "libbpf_internal.h"
#include "xsk.h"
#include "bpf.h"

#include "sv_frames.h"

void start_xdp(void);
void cleanup_xdp(void);
void dump_stats(void);
void __exit_with_error(int error, const char *file, const char *func, int line);
#define exit_with_error(error) __exit_with_error(error, __FILE__, __func__, __LINE__)


int opt_test;
uint32_t opt_xdp_flags = XDP_FLAGS_UPDATE_IF_NOEXIST;
const char *opt_if = "";
int opt_ifindex;
int opt_queue;
int opt_poll;
int opt_interval = 1;
uint32_t opt_xdp_bind_flags = XDP_USE_NEED_WAKEUP;
uint32_t opt_umem_flags;
int opt_unaligned_chunks;
int opt_mmap_flags;
uint32_t opt_xdp_bind_flags;
int opt_xsk_frame_size = XSK_UMEM__DEFAULT_FRAME_SIZE;
int opt_timeout = 1000;
bool opt_need_wakeup = true;
int opt_debug = 1;


struct option long_options[] = {
	{"test", no_argument, 0, 't'},
	{"interface", required_argument, 0, 'i'},
	{"queue", required_argument, 0, 'q'},
	{"poll", no_argument, 0, 'p'},
	{"xdp-skb", no_argument, 0, 'S'},
	{"xdp-native", no_argument, 0, 'N'},
	{"interval", required_argument, 0, 'n'},
	{"zero-copy", no_argument, 0, 'z'},
	{"copy", no_argument, 0, 'c'},
	{"frame-size", required_argument, 0, 'f'},
	{"no-need-wakeup", no_argument, 0, 'm'},
	{"unaligned", no_argument, 0, 'u'},
	{0, 0, 0, 0}
};

void usage(const char *prog)
{
	const char *str =
		"  Usage: %s [OPTIONS]\n"
		"  Options:\n"
		"  -t, --test           TEST TEST TEST\n"
		"  -i, --interface=n    Run on interface n\n"
		"  -q, --queue=n        Use queue n (default 0)\n"
		"  -p, --poll           Use poll syscall\n"
		"  -S, --xdp-skb=n      Use XDP skb-mod\n"
		"  -N, --xdp-native=n   Enfore XDP native mode\n"
		"  -n, --interval=n     Specify statistics update interval (default 1 sec).\n"
		"  -z, --zero-copy      Force zero-copy mode.\n"
		"  -c, --copy           Force copy mode.\n"
		"  -m, --no-need-wakeup Turn off use of driver need wakeup flag.\n"
		"  -f, --frame-size=n   Set the frame size (must be a power of two in aligned mode, default is %d).\n"
		"  -u, --unaligned      Enable unaligned chunk placement\n"
		"\n";
	fprintf(stderr, str, prog, XSK_UMEM__DEFAULT_FRAME_SIZE);
	exit(EXIT_FAILURE);
}

void parse_command_line(int argc, char **argv)
{
	int option_index, c;

	opterr = 0;

	for (;;) {
		c = getopt_long(argc, argv, "Fti:q:psSNn:czf:mu", long_options, &option_index);
		if (c == -1)
			break;

		switch (c) {
		case 't':
			opt_test = 1;
			break;
		case 'i':
			opt_if = optarg;
			break;
		case 'q':
			opt_queue = atoi(optarg);
			break;
		case 'p':
			opt_poll = 1;
			break;
		case 'S':
			opt_xdp_flags |= XDP_FLAGS_SKB_MODE;
			opt_xdp_bind_flags |= XDP_COPY;
			break;
		case 'N':
			opt_xdp_flags |= XDP_FLAGS_DRV_MODE;
			break;
		case 'n':
			opt_interval = atoi(optarg);
			break;
		case 'z':
			opt_xdp_bind_flags |= XDP_ZEROCOPY;
			break;
		case 'c':
			opt_xdp_bind_flags |= XDP_COPY;
			break;
		case 'u':
			opt_umem_flags |= XDP_UMEM_UNALIGNED_CHUNK_FLAG;
			opt_unaligned_chunks = 1;
			opt_mmap_flags = MAP_HUGETLB;
			break;
		case 'F':
			opt_xdp_flags &= ~XDP_FLAGS_UPDATE_IF_NOEXIST;
			break;
		case 'f':
			opt_xsk_frame_size = atoi(optarg);
		case 'm':
			opt_need_wakeup = false;
			opt_xdp_bind_flags &= ~XDP_USE_NEED_WAKEUP;
			break;

		default:
			usage(basename(argv[0]));
		}
	}

	opt_ifindex = if_nametoindex(opt_if);
	if (!opt_ifindex) {
		fprintf(stderr, "ERROR: interface \"%s\" does not exist\n",
			opt_if);
		usage(basename(argv[0]));
	}

	if ((opt_xsk_frame_size & (opt_xsk_frame_size - 1)) &&
	    !opt_unaligned_chunks) {
		fprintf(stderr, "--frame-size=%d is not a power of two\n",
			opt_xsk_frame_size);
		usage(basename(argv[0]));
	}
}

unsigned long get_nsecs(void);
extern unsigned long prev_time;

static void *poller(void *arg)
{
	(void)arg;
	prev_time = get_nsecs();
	for (;;) {
		sleep(opt_interval);
		dump_stats();
	}

	return NULL;
}

static void int_exit(int sig)
{
	(void)sig;
	dump_stats();
	cleanup_xdp();
	exit(EXIT_SUCCESS);
}





int main(int argc, char **argv)
{

	parse_command_line(argc, argv);


	

	signal(SIGINT, int_exit);
	signal(SIGTERM, int_exit);
	signal(SIGABRT, int_exit);

	setlocale(LC_ALL, "");

	pthread_t pt;
	int ret = pthread_create(&pt, NULL, poller, NULL);
	if (ret)
		exit_with_error(ret);

    start_xdp();

	return 0;
}