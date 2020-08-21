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

//#include "libbpf.h"
//#include "libbpf_internal.h"
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


#define SAMPLEWRAP (4000)
#define PACKET_SIZE (256)
#define MAX_STREAMS (256)
#define NUM_FRAMES (SAMPLEWRAP*PACKET_SIZE*MAX_STREAMS/XSK_UMEM__DEFAULT_FRAME_SIZE)
#define BATCH_SIZE 100

#define DEBUG_HEXDUMP 0
#define MAX_SOCKS 8



unsigned long prev_time;

extern int opt_test;
extern uint32_t opt_xdp_flags;
extern const char *opt_if;
extern int opt_ifindex;
extern int opt_queue;
extern int opt_poll;
extern int opt_interval;
extern uint32_t opt_xdp_bind_flags;
extern uint32_t opt_umem_flags;
extern int opt_unaligned_chunks;
extern int opt_mmap_flags;
extern uint32_t opt_xdp_bind_flags;
extern int opt_xsk_frame_size;
extern int opt_timeout;
extern bool opt_need_wakeup;



struct sv_xdp_socket_info {
	struct xsk_ring_cons rx;
	struct xsk_ring_prod tx;
	struct xsk_ring_prod fq;
	struct xsk_ring_cons cq;
	struct xsk_umem *umem;
	char *umem_area;
	struct xsk_socket *xsk;
	unsigned long rx_npkts;
	unsigned long tx_npkts;
	unsigned long prev_rx_npkts;
	unsigned long prev_tx_npkts;
	uint32_t outstanding_tx;
	uint32_t prog_id;
};
struct sv_xdp_socket_info *xsksi;



unsigned long get_nsecs(void)
{
	struct timespec ts;

	clock_gettime(CLOCK_MONOTONIC, &ts);
	return ts.tv_sec * 1000000000UL + ts.tv_nsec;
}

static void print_benchmark(bool running)
{
	const char *bench_str = "";

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

void dump_stats(void)
{
	unsigned long now = get_nsecs();
	long dt = now - prev_time;
	int i;

	prev_time = now;
	
	char *fmt = "%-15s %'-11.0f %'-11lu\n";
	double rx_pps, tx_pps;

	rx_pps = (xsksi->rx_npkts - xsksi->prev_rx_npkts) *
			1000000000. / dt;
	tx_pps = (xsksi->tx_npkts - xsksi->prev_tx_npkts) *
			1000000000. / dt;

	printf("\n sock%d@", i);
	print_benchmark(false);
	printf("\n");

	printf("%-15s %-11s %-11s %-11.2f\n", "", "pps", "pkts", dt / 1000000000.);
	printf(fmt, "rx", rx_pps, xsksi->rx_npkts);
	printf(fmt, "tx", tx_pps, xsksi->tx_npkts);

	xsksi->prev_rx_npkts = xsksi->rx_npkts;
	xsksi->prev_tx_npkts = xsksi->tx_npkts;
}

void __exit_with_error(int error, const char *file, const char *func, int line)
{
	fprintf(stderr, "%s:%s:%i: errno: %d/\"%s\"\n", file, func,
		line, error, strerror(error));
	dump_stats();
	cleanup_xdp();
	exit(EXIT_FAILURE);
}

#define exit_with_error(error) __exit_with_error(error, __FILE__, __func__, __LINE__)


static void remove_xdp_program(uint32_t prog_id)
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


static void gen_eth_frame(struct sv_xdp_socket_info *xski, uint64_t addr, uint32_t svid, uint32_t smp)
{
	uint8_t *pkt_ptr = xsk_umem__get_data(xski->umem_area, addr);
	memcpy(pkt_ptr, pkt_frames[smp % FRAME_NUM], FRAME_SIZE);
	*(uint16_t*)(pkt_ptr+39+pkt_ptr[36]) = htons((uint16_t)smp);
#if 0
	printf("············ sv %d smp %d len %d ············\n", svid, smp, FRAME_SIZE);
	uint8_t blen = FRAME_SIZE;
	uint8_t *b = pkt_ptr;
	while (blen > 0) {
		char line[128];
		sprintf(line, "%02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x",
				b[0], b[1], b[2], b[3], b[4], b[5], b[6], b[7], b[8], b[9], b[10], b[11], b[12], b[13], b[14], b[15]);
		if (blen >= 16) {
			b += 16;
			blen -=16;
		} else {
			line[3*blen] = '\0';
			blen = 0;
		}
		printf("%s\n", line);
	}
#endif
}

static struct sv_xdp_socket_info *sv_xdp_configure_socket(uint32_t num_frames, uint32_t frame_size)
{
	int ret;

	struct sv_xdp_socket_info *xski = calloc(1, sizeof(struct sv_xdp_socket_info));
	if (!xski) {
		exit_with_error(errno);
	}

	// reserve memory for the umem. Use hugepages if unaligned chunk mode
	xski->umem_area = mmap(NULL, num_frames * frame_size, PROT_READ | PROT_WRITE,
						MAP_PRIVATE | MAP_ANONYMOUS | opt_mmap_flags, -1, 0);
	if (xski->umem_area == MAP_FAILED) {
		printf("ERROR: mmap failed\n");
		exit(EXIT_FAILURE);
	}

	// configure umem
	struct xsk_umem_config ucfg = {
		.fill_size = XSK_RING_PROD__DEFAULT_NUM_DESCS,
		.comp_size = XSK_RING_CONS__DEFAULT_NUM_DESCS,
		.frame_size = frame_size,
		.frame_headroom = XSK_UMEM__DEFAULT_FRAME_HEADROOM,
		.flags = opt_umem_flags
	};
	ret = xsk_umem__create(&xski->umem, xski->umem_area, num_frames * frame_size, &xski->fq, &xski->cq, &ucfg);
	if (ret) {
		exit_with_error(-ret);
	}

	// configure socket
	struct xsk_socket_config scfg = {
		.rx_size = XSK_RING_CONS__DEFAULT_NUM_DESCS,
		.tx_size = XSK_RING_PROD__DEFAULT_NUM_DESCS,
		.libbpf_flags = 0,
		.xdp_flags = opt_xdp_flags,
		.bind_flags = opt_xdp_bind_flags
	};	
	ret = xsk_socket__create(&xski->xsk, opt_if, opt_queue, xski->umem, &xski->rx, &xski->tx, &scfg);
	if (ret) {
		exit_with_error(-ret);
	}

	// ret = bpf_get_link_xdp_id(opt_ifindex, &xski->prog_id, opt_xdp_flags); //needed?
	// if (ret) {
	// 	exit_with_error(-ret);
	// }

	// uint32_t idx;
	// ret = xsk_ring_prod__reserve(&xski->fq, XSK_RING_PROD__DEFAULT_NUM_DESCS, &idx);
	// if (ret != XSK_RING_PROD__DEFAULT_NUM_DESCS) {
	// 	exit_with_error(-ret);
	// }
	// for (int i = 0; i < XSK_RING_PROD__DEFAULT_NUM_DESCS; i++) {
	// 	*xsk_ring_prod__fill_addr(&xski->fq, idx+i) = i * opt_xsk_frame_size;
	// }
	// xsk_ring_prod__submit(&xski->fq, XSK_RING_PROD__DEFAULT_NUM_DESCS);

	return xski;
}




static void tx_only(struct sv_xdp_socket_info *xski, uint32_t *frame_nb)
{
	uint32_t idx;

	int32_t ret = xsk_ring_prod__reserve(&xski->tx, BATCH_SIZE, &idx);
	if (ret  == BATCH_SIZE) {
		unsigned int i;

		for (i = 0; i < BATCH_SIZE; i++) {
			xsk_ring_prod__tx_desc(&xski->tx, idx + i)->addr = (*frame_nb + i) * opt_xsk_frame_size;
			xsk_ring_prod__tx_desc(&xski->tx, idx + i)->len = FRAME_SIZE;
		}

		xsk_ring_prod__submit(&xski->tx, BATCH_SIZE);
		xski->outstanding_tx += BATCH_SIZE;
		*frame_nb += BATCH_SIZE;
		*frame_nb %= NUM_FRAMES;
	} else {
		//printf("xsk_ring_prod__reserve ret %d\n", ret);
	}


	if (!xski->outstanding_tx) {
		return;
	}

	if (!opt_need_wakeup || xsk_ring_prod__needs_wakeup(&xski->tx)) {
		//kick tx
		int ret = sendto(xsk_socket__fd(xski->xsk), NULL, 0, MSG_DONTWAIT, NULL, 0);
		if (ret < 0 && errno != ENOBUFS && errno != EAGAIN && errno != EBUSY) {
			exit_with_error(errno);
		}
	}

	uint32_t rcvd = xsk_ring_cons__peek(&xski->cq, BATCH_SIZE, &idx);
	if (rcvd > 0) {
		xsk_ring_cons__release(&xski->cq, rcvd);
		xski->outstanding_tx -= rcvd;
		xski->tx_npkts += rcvd;
	}
}



void start_xdp(void)
{
	/* Create mmap, umem and sockets */

	xsksi = sv_xdp_configure_socket(NUM_FRAMES, opt_xsk_frame_size);

	for (int i = 0; i < NUM_FRAMES; i++) {
		gen_eth_frame(xsksi, i * opt_xsk_frame_size, 0, i);
	}

	struct pollfd fds;
	uint32_t frame_nb = 0;
	int i, ret;

	memset(&fds, 0, sizeof(fds));
	fds.fd = xsk_socket__fd(xsksi->xsk);
	fds.events = POLLOUT;

	for (;;) {
		if (opt_poll) {
			ret = poll(&fds, 1, 1000);
			if (ret <= 0)
				continue;

			if (!(fds.revents & POLLOUT))
				continue;
		}

		//usleep(1);
		tx_only(xsksi, &frame_nb);
	}
}



void cleanup_xdp(void)
{
    struct xsk_umem *umem = xsksi->umem;
	xsk_socket__delete(xsksi->xsk);
	(void)xsk_umem__delete(umem);
	remove_xdp_program(xsksi->prog_id);
}