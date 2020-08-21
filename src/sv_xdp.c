#include "sv_injector.h"
#include "sv_xdp.h"



struct sSvXdpSkt *sv_xdp_conf_skt(struct sSvOpt *opt, uint32_t num_frames, uint32_t frame_size)
{
	int ret;

	struct sSvXdpSkt *xski = calloc(1, sizeof(struct sSvXdpSkt));
	if (!xski) {
		exit(EXIT_FAILURE);//exit_with_error(errno);
	}

	// reserve memory for the umem. Use hugepages if unaligned chunk mode
	xski->umem_area = mmap(NULL, num_frames * frame_size, PROT_READ | PROT_WRITE,
						MAP_PRIVATE | MAP_ANONYMOUS | opt->mmap_flags, -1, 0);
	if (xski->umem_area == MAP_FAILED) {
		printf("error: mmap failed\n");
		exit(EXIT_FAILURE);
	}

	// configure umem
	struct xsk_umem_config ucfg = {
		.fill_size = XSK_RING_PROD__DEFAULT_NUM_DESCS,
		.comp_size = XSK_RING_CONS__DEFAULT_NUM_DESCS,
		.frame_size = frame_size,
		.frame_headroom = XSK_UMEM__DEFAULT_FRAME_HEADROOM,
		.flags = opt->umem_flags
	};
	ret = xsk_umem__create(&xski->umem, xski->umem_area, num_frames * frame_size, &xski->fq, &xski->cq, &ucfg);
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



	// sv_prepare
	// 	printf("creating %d packets, frame size %d B, total size %d kB\n", sv_pkt_skt.pkt_num, sv_pkt_skt.pkt_sz, sv_pkt_skt.pkt_num * sv_pkt_skt.pkt_sz / 1024);


	return xski;
}

int32_t sv_xdp_send(struct sSvXdpSkt *pkt_skt, uint32_t smp)
{
	return 0;
}

void *xdp_skt_stats(void *arg)
{
	struct sSvXdpSkt *xdp_skt = (struct sSvXdpSkt *) arg;

	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);

	uint32_t prev_tx_npkts = xdp_skt->tx_npkts;
	for (;;) {
		clock_addinterval(&ts, 1000000000);
		clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &ts, NULL);

		printf("throughput: %d kpps ; sleep times: %d, %d, %d, %d, %d, %d, %d, %d, %d, %d us\n",
				(xdp_skt->tx_npkts - prev_tx_npkts)/1000,
				xdp_skt->sleeptimes[0], xdp_skt->sleeptimes[1], xdp_skt->sleeptimes[2], xdp_skt->sleeptimes[3], xdp_skt->sleeptimes[4],
				xdp_skt->sleeptimes[5], xdp_skt->sleeptimes[6], xdp_skt->sleeptimes[7], xdp_skt->sleeptimes[8], xdp_skt->sleeptimes[9]);
		prev_tx_npkts = xdp_skt->tx_npkts;
	}
	return NULL;
}


