// SPDX-License-Identifier: GPL-2.0
/* Copyright(c) 2017 - 2018 Intel Corporation. */

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

// #ifndef SOL_XDP
// #define SOL_XDP 283
// #endif

// #ifndef AF_XDP
// #define AF_XDP 44
// #endif

// #ifndef PF_XDP
// #define PF_XDP AF_XDP
// #endif

#define NUM_FRAMES (4000)
#define BATCH_SIZE 128

#define DEBUG_HEXDUMP 0
#define MAX_SOCKS 8



static unsigned long prev_time;

static int opt_test;
static uint32_t opt_xdp_flags = XDP_FLAGS_UPDATE_IF_NOEXIST;
static const char *opt_if = "";
static int opt_ifindex;
static int opt_queue;
static int opt_poll;
static int opt_interval = 1;
static uint32_t opt_xdp_bind_flags = XDP_USE_NEED_WAKEUP;
static uint32_t opt_umem_flags;
static int opt_unaligned_chunks;
static int opt_mmap_flags;
static uint32_t opt_xdp_bind_flags;
static int opt_xsk_frame_size = XSK_UMEM__DEFAULT_FRAME_SIZE;
static int opt_timeout = 1000;
static bool opt_need_wakeup = true;
static uint32_t prog_id;

struct xsk_umem_info {
	struct xsk_ring_prod fq;
	struct xsk_ring_cons cq;
	struct xsk_umem *umem;
	void *buffer;
};

struct xsk_socket_info {
	struct xsk_ring_cons rx;
	struct xsk_ring_prod tx;
	struct xsk_umem_info *umem;
	struct xsk_socket *xsk;
	unsigned long rx_npkts;
	unsigned long tx_npkts;
	unsigned long prev_rx_npkts;
	unsigned long prev_tx_npkts;
	uint32_t outstanding_tx;
};

static int num_socks;
struct xsk_socket_info *xsks[MAX_SOCKS];

static unsigned long get_nsecs(void)
{
	struct timespec ts;

	clock_gettime(CLOCK_MONOTONIC, &ts);
	return ts.tv_sec * 1000000000UL + ts.tv_nsec;
}

static void print_benchmark(bool running)
{
	const char *bench_str = "INVALID";

	printf("%s:%d %s ", opt_if, opt_queue, bench_str);
	if (opt_xdp_flags & XDP_FLAGS_SKB_MODE)
		printf("xdp-skb ");
	else if (opt_xdp_flags & XDP_FLAGS_DRV_MODE)
		printf("xdp-drv ");
	else
		printf("	");

	if (opt_poll)
		printf("poll() ");

	if (running) {
		printf("running...");
		fflush(stdout);
	}
}

static void dump_stats(void)
{
	unsigned long now = get_nsecs();
	long dt = now - prev_time;
	int i;

	prev_time = now;

	for (i = 0; i < num_socks && xsks[i]; i++) {
		char *fmt = "%-15s %'-11.0f %'-11lu\n";
		double rx_pps, tx_pps;

		rx_pps = (xsks[i]->rx_npkts - xsks[i]->prev_rx_npkts) *
			 1000000000. / dt;
		tx_pps = (xsks[i]->tx_npkts - xsks[i]->prev_tx_npkts) *
			 1000000000. / dt;

		printf("\n sock%d@", i);
		print_benchmark(false);
		printf("\n");

		printf("%-15s %-11s %-11s %-11.2f\n", "", "pps", "pkts",
		       dt / 1000000000.);
		printf(fmt, "rx", rx_pps, xsks[i]->rx_npkts);
		printf(fmt, "tx", tx_pps, xsks[i]->tx_npkts);

		xsks[i]->prev_rx_npkts = xsks[i]->rx_npkts;
		xsks[i]->prev_tx_npkts = xsks[i]->tx_npkts;
	}
}

static void *poller(void *arg)
{
	(void)arg;
	for (;;) {
		sleep(opt_interval);
		dump_stats();
	}

	return NULL;
}

static void remove_xdp_program(void)
{
	uint32_t curr_prog_id = 0;

	if (bpf_get_link_xdp_id(opt_ifindex, &curr_prog_id, opt_xdp_flags)) {
		printf("bpf_get_link_xdp_id failed\n");
		exit(EXIT_FAILURE);
	}
	if (prog_id == curr_prog_id)
		bpf_set_link_xdp_fd(opt_ifindex, -1, opt_xdp_flags);
	else if (!curr_prog_id)
		printf("couldn't find a prog id on a given interface\n");
	else
		printf("program on interface changed, not removing\n");
}

static void int_exit(int sig)
{
	struct xsk_umem *umem = xsks[0]->umem->umem;

	(void)sig;

	dump_stats();
	xsk_socket__delete(xsks[0]->xsk);
	(void)xsk_umem__delete(umem);
	remove_xdp_program();

	exit(EXIT_SUCCESS);
}

static void __exit_with_error(int error, const char *file, const char *func,
			      int line)
{
	fprintf(stderr, "%s:%s:%i: errno: %d/\"%s\"\n", file, func,
		line, error, strerror(error));
	dump_stats();
	remove_xdp_program();
	exit(EXIT_FAILURE);
}

#define exit_with_error(error) __exit_with_error(error, __FILE__, __func__, __LINE__)


static size_t gen_eth_frame(struct xsk_umem_info *umem, uint64_t addr, uint32_t svid, uint32_t smp)
{
	void *pkt_ptr = xsk_umem__get_data(umem->buffer, addr);
	memcpy(pkt_ptr, pkt_frames[smp % FRAME_NUM], FRAME_SIZE);
	*(uint16_t*)(pkt_ptr+SMPC_OFFSET) = htons((uint16_t)smp);
	return 0;
}

static struct xsk_umem_info *xsk_configure_umem(void *buffer, uint64_t size)
{
	struct xsk_umem_info *umem;
	struct xsk_umem_config cfg = {
		.fill_size = XSK_RING_PROD__DEFAULT_NUM_DESCS,
		.comp_size = XSK_RING_CONS__DEFAULT_NUM_DESCS,
		.frame_size = opt_xsk_frame_size,
		.frame_headroom = XSK_UMEM__DEFAULT_FRAME_HEADROOM,
		.flags = opt_umem_flags
	};

	int ret;

	umem = calloc(1, sizeof(*umem));
	if (!umem)
		exit_with_error(errno);

	ret = xsk_umem__create(&umem->umem, buffer, size, &umem->fq, &umem->cq, &cfg);

	if (ret)
		exit_with_error(-ret);

	umem->buffer = buffer;
	return umem;
}

static struct xsk_socket_info *xsk_configure_socket(struct xsk_umem_info *umem)
{
	struct xsk_socket_config cfg;
	struct xsk_socket_info *xsk;
	int ret;
	uint32_t idx;
	int i;

	xsk = calloc(1, sizeof(*xsk));
	if (!xsk)
		exit_with_error(errno);

	xsk->umem = umem;
	cfg.rx_size = XSK_RING_CONS__DEFAULT_NUM_DESCS;
	cfg.tx_size = XSK_RING_PROD__DEFAULT_NUM_DESCS;
	cfg.libbpf_flags = 0;
	cfg.xdp_flags = opt_xdp_flags;
	cfg.bind_flags = opt_xdp_bind_flags;
	ret = xsk_socket__create(&xsk->xsk, opt_if, opt_queue, umem->umem,
				 &xsk->rx, &xsk->tx, &cfg);
	if (ret)
		exit_with_error(-ret);

	ret = bpf_get_link_xdp_id(opt_ifindex, &prog_id, opt_xdp_flags);
	if (ret)
		exit_with_error(-ret);

	ret = xsk_ring_prod__reserve(&xsk->umem->fq,
				     XSK_RING_PROD__DEFAULT_NUM_DESCS,
				     &idx);
	if (ret != XSK_RING_PROD__DEFAULT_NUM_DESCS)
		exit_with_error(-ret);
	for (i = 0; i < XSK_RING_PROD__DEFAULT_NUM_DESCS; i++)
		*xsk_ring_prod__fill_addr(&xsk->umem->fq, idx++) =
			i * opt_xsk_frame_size;
	xsk_ring_prod__submit(&xsk->umem->fq,
			      XSK_RING_PROD__DEFAULT_NUM_DESCS);

	return xsk;
}

static struct option long_options[] = {
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

static void usage(const char *prog)
{
	const char *str =
		"  Usage: %s [OPTIONS]\n"
		"  Options:\n"
		"  -t, --test		TEST TEST TEST\n"
		"  -i, --interface=n	Run on interface n\n"
		"  -q, --queue=n	Use queue n (default 0)\n"
		"  -p, --poll		Use poll syscall\n"
		"  -S, --xdp-skb=n	Use XDP skb-mod\n"
		"  -N, --xdp-native=n	Enfore XDP native mode\n"
		"  -n, --interval=n	Specify statistics update interval (default 1 sec).\n"
		"  -z, --zero-copy      Force zero-copy mode.\n"
		"  -c, --copy           Force copy mode.\n"
		"  -m, --no-need-wakeup Turn off use of driver need wakeup flag.\n"
		"  -f, --frame-size=n   Set the frame size (must be a power of two in aligned mode, default is %d).\n"
		"  -u, --unaligned	Enable unaligned chunk placement\n"
		"\n";
	fprintf(stderr, str, prog, XSK_UMEM__DEFAULT_FRAME_SIZE);
	exit(EXIT_FAILURE);
}

static void parse_command_line(int argc, char **argv)
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

static void kick_tx(struct xsk_socket_info *xsk)
{
	int ret;

	ret = sendto(xsk_socket__fd(xsk->xsk), NULL, 0, MSG_DONTWAIT, NULL, 0);
	if (ret >= 0 || errno == ENOBUFS || errno == EAGAIN || errno == EBUSY)
		return;
	exit_with_error(errno);
}


static inline void complete_tx_only(struct xsk_socket_info *xsk)
{
	unsigned int rcvd;
	uint32_t idx;

	if (!xsk->outstanding_tx)
		return;

	if (!opt_need_wakeup || xsk_ring_prod__needs_wakeup(&xsk->tx))
		kick_tx(xsk);

	rcvd = xsk_ring_cons__peek(&xsk->umem->cq, BATCH_SIZE, &idx);
	if (rcvd > 0) {
		xsk_ring_cons__release(&xsk->umem->cq, rcvd);
		xsk->outstanding_tx -= rcvd;
		xsk->tx_npkts += rcvd;
	}
}

static void tx_only(struct xsk_socket_info *xsk, uint32_t frame_nb)
{
	uint32_t idx;

	if (xsk_ring_prod__reserve(&xsk->tx, BATCH_SIZE, &idx) == BATCH_SIZE) {
		unsigned int i;

		for (i = 0; i < BATCH_SIZE; i++) {
			xsk_ring_prod__tx_desc(&xsk->tx, idx + i)->addr	= (frame_nb + i) << XSK_UMEM__DEFAULT_FRAME_SHIFT;
			xsk_ring_prod__tx_desc(&xsk->tx, idx + i)->len = FRAME_SIZE;
		}

		xsk_ring_prod__submit(&xsk->tx, BATCH_SIZE);
		xsk->outstanding_tx += BATCH_SIZE;
		frame_nb += BATCH_SIZE;
		frame_nb %= NUM_FRAMES;
	}

	complete_tx_only(xsk);
}

static void tx_only_all(void)
{
	struct pollfd fds[MAX_SOCKS];
	uint32_t frame_nb[MAX_SOCKS] = {};
	int i, ret;

	memset(fds, 0, sizeof(fds));
	for (i = 0; i < num_socks; i++) {
		fds[0].fd = xsk_socket__fd(xsks[i]->xsk);
		fds[0].events = POLLOUT;
	}

	for (;;) {
		if (opt_poll) {
			ret = poll(fds, num_socks, opt_timeout);
			if (ret <= 0)
				continue;

			if (!(fds[0].revents & POLLOUT))
				continue;
		}

		for (i = 0; i < num_socks; i++)
			tx_only(xsks[i], frame_nb[i]);
	}
}

int main(int argc, char **argv)
{
	struct rlimit r = {RLIM_INFINITY, RLIM_INFINITY};
	struct xsk_umem_info *umem;
	pthread_t pt;
	void *bufs;
	int ret;

	parse_command_line(argc, argv);

	if (setrlimit(RLIMIT_MEMLOCK, &r)) {
		fprintf(stderr, "ERROR: setrlimit(RLIMIT_MEMLOCK) \"%s\"\n",
			strerror(errno));
		exit(EXIT_FAILURE);
	}

	/* Reserve memory for the umem. Use hugepages if unaligned chunk mode */
	bufs = mmap(NULL, NUM_FRAMES * opt_xsk_frame_size,
		    PROT_READ | PROT_WRITE,
		    MAP_PRIVATE | MAP_ANONYMOUS | opt_mmap_flags, -1, 0);
	if (bufs == MAP_FAILED) {
		printf("ERROR: mmap failed\n");
		exit(EXIT_FAILURE);
	}
       /* Create sockets... */
	umem = xsk_configure_umem(bufs, NUM_FRAMES * opt_xsk_frame_size);
	xsks[num_socks++] = xsk_configure_socket(umem);

	{
		int i;

		for (i = 0; i < NUM_FRAMES; i++) {
			(void) gen_eth_frame(umem, i * opt_xsk_frame_size, 0, i);
		}
	}

	signal(SIGINT, int_exit);
	signal(SIGTERM, int_exit);
	signal(SIGABRT, int_exit);

	setlocale(LC_ALL, "");


	if (opt_test) {
		libbpf_print(LIBBPF_INFO, "test\n");
		return 0;
	}



	ret = pthread_create(&pt, NULL, poller, NULL);
	if (ret)
		exit_with_error(ret);

	prev_time = get_nsecs();

	tx_only_all();

	return 0;
}

/* this function is needed to not include libbpf.c tha needs a bunch of other dependencies too */
void libbpf_print(enum libbpf_print_level level, const char *format, ...)
{
	va_list args;
	va_start(args, format);
	vfprintf(stderr, format, args);
	va_end(args);
}
