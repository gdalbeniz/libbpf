/* Copyright (C) Oier Garcia de Albeniz Lopez - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 * Written by Oier Garcia de Albeniz Lopez <g.albeniz@gmail.com>, September 2020
 */
#include "sv_injector.h"
#include "sv_frames.h"
#include "sv_xdp.h"



struct sSvXdpSkt *sv_xdp_conf_skt(struct sSvOpt *opt)
{
	int ret;

	struct sSvXdpSkt *xski = calloc(1, sizeof(struct sSvXdpSkt));
	if (!xski) {
		exit(EXIT_FAILURE);//exit_with_error(errno);
	}
	xski->opt = opt;
	xski->pkt_sz = opt->xsk_frame_size;
	xski->pkt_num = 80 * opt->sv_num;

	// reserve memory for the umem. Use hugepages if unaligned chunk mode
	xski->umem_area = mmap(NULL, xski->pkt_num * xski->pkt_sz, PROT_READ | PROT_WRITE,
						MAP_PRIVATE | MAP_ANONYMOUS | opt->mmap_flags, -1, 0);
	if (xski->umem_area == MAP_FAILED) {
		printf("error: mmap failed\n");
		exit(EXIT_FAILURE);
	}

	// configure umem
	struct xsk_umem_config ucfg = {
		.fill_size = XSK_RING_PROD__DEFAULT_NUM_DESCS,
		.comp_size = XSK_RING_CONS__DEFAULT_NUM_DESCS,
		.frame_size = xski->pkt_sz,
		.frame_headroom = XSK_UMEM__DEFAULT_FRAME_HEADROOM,
		.flags = opt->umem_flags
	};
	ret = xsk_umem__create(&xski->umem, xski->umem_area, xski->pkt_num * xski->pkt_sz,
							&xski->fq, &xski->cq, &ucfg);
	if (ret) {
		printf("error: xsk_umem__create failed\n");
		exit(EXIT_FAILURE);//exit_with_error(-ret);
	}

	// configure socket
	struct xsk_socket_config scfg = {
		.rx_size = XSK_RING_CONS__DEFAULT_NUM_DESCS,
		.tx_size = XSK_RING_PROD__DEFAULT_NUM_DESCS,
		.libbpf_flags = 0,
		.xdp_flags = opt->xdp_flags,
		.bind_flags = opt->xdp_bind_flags
	};	
	ret = xsk_socket__create(&xski->xsk, opt->iface, 0, xski->umem, &xski->rx, &xski->tx, &scfg);
	if (ret) {
		printf("error: xsk_socket__create failed\n");
		exit(EXIT_FAILURE);//exit_with_error(-ret);
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


	// prepare packets
	for (uint32_t sv = 0; sv < opt->sv_num; sv++) {
		int32_t ret = sv_frame_prepare(opt, xski, sv);
		if (ret < 0) {
			printf("error: sv_frame_prepare failed (%d)\n", ret);
			return -1;
		}
	}
	printf("creating %d packets, frame size %d B, total size %d kB\n",
		xski->pkt_num, xski->pkt_sz, xski->pkt_num * xski->pkt_sz / 1024);

	return xski;
}

static inline
uint8_t* sv_xdp_get_ptr(struct sSvXdpSkt *xdp_skt, uint32_t sv, uint32_t smp)
{
	// uint8_t umem_area[80][sv_num][pkt_sz]
	return xdp_skt->umem_area + (smp * xdp_skt->opt->sv_num + sv) * xdp_skt->pkt_sz;
}

void sv_xdp_store_frame(struct sSvXdpSkt *xdp_skt, uint8_t *buffer, uint32_t bufLen, uint32_t sv, uint32_t smp)
{
	uint8_t *sample_ptr = sv_xdp_get_ptr(xdp_skt, sv, smp);
	memcpy(sample_ptr, buffer, bufLen);
	xdp_skt->bufLen = bufLen;
}




void sv_xdp_send(struct sSvXdpSkt *xski, uint32_t *frame_nb, uint32_t batch_sz)
{
	uint32_t idx;
	if (batch_sz > 0) {
		int32_t ntx = xsk_ring_prod__reserve(&xski->tx, batch_sz, &idx);
		if (ntx > 0) {
			for (uint32_t i = 0; i < ntx; i++) {
				xsk_ring_prod__tx_desc(&xski->tx, idx + i)->addr = (*frame_nb + i) * xski->pkt_sz;
				xsk_ring_prod__tx_desc(&xski->tx, idx + i)->len = xski->bufLen;
				// uint8_t *sample_ptr = sv_xdp_get_ptr(xski, sv, smp % 80);
				// sv_frame_smp_upd(sample_ptr, (uint16_t)smp);

				// xsk_ring_prod__tx_desc(&xski->tx, idx + sv)->addr = sample_ptr;
				// xsk_ring_prod__tx_desc(&xski->tx, idx + sv)->len = xski->bufLen;
			}

			xsk_ring_prod__submit(&xski->tx, ntx);
			xski->outstanding_tx += ntx;
			*frame_nb += ntx;
			*frame_nb %= xski->pkt_num;
			//printf("xsk_ring_prod__reserve ret %d\n", ret);
		}
	}

	if (1/*!opt->need_wakeup*/ || xsk_ring_prod__needs_wakeup(&xski->tx)) {
		//kick tx
		int ret = sendto(xsk_socket__fd(xski->xsk), NULL, 0, MSG_DONTWAIT, NULL, 0);
		if (ret < 0 && errno != ENOBUFS && errno != EAGAIN && errno != EBUSY) {
			exit(-1);//exit_with_error(errno);
		}
	}

	uint32_t rcvd = xsk_ring_cons__peek(&xski->cq, 256, &idx);
	if (rcvd > 0) {
		xsk_ring_cons__release(&xski->cq, rcvd);
		xski->outstanding_tx -= rcvd;
		xski->tx_npkts += rcvd;
	}
}

void sv_xdp_send_all(struct sSvXdpSkt *xski)
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);

	uint32_t frame_nb = 0;

	for (;;) {
		sv_xdp_send(xski, &frame_nb, 256);
	}
}


void *xdp_skt_stats(void *arg)
{
	struct sSvXdpSkt *xdp_skt = (struct sSvXdpSkt *) arg;

	struct timespec ts;
	clock_gettime(CLOCK_REALTIME, &ts);

	uint32_t to = ts.tv_sec;
	if (xdp_skt->opt->timeout > 0 && xdp_skt->opt->timeout <= 600 && ts.tv_sec <= 0x60000000) {
		to = ts.tv_sec + xdp_skt->opt->timeout;
	}

	for (;;) {
		uint32_t prev_tx_npkts = xdp_skt->tx_npkts;

		if (++ts.tv_sec > to) exit(0);
		clock_nanosleep(CLOCK_REALTIME, TIMER_ABSTIME, &ts, NULL);

		printf("xdp throughput: %d pps ; outstanding: %d ; \n",
				xdp_skt->tx_npkts - prev_tx_npkts, xdp_skt->outstanding_tx);
	}
	return NULL;
}
