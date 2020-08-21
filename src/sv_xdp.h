#ifndef __SV_XDP__
#define __SV_XDP__

#include "sv_config.h"


struct sSvXdpSkt {
	struct xsk_socket *xsk;
	struct xsk_ring_cons rx;
	struct xsk_ring_prod tx;
	struct xsk_ring_prod fq;
	struct xsk_ring_cons cq;

	struct xsk_umem *umem;
	uint8_t *umem_area;

	uint32_t tx_npkts;
	uint32_t outstanding_tx;
	uint32_t prog_id;
	int32_t sleeptimes[16];
};

void *xdp_skt_stats(void *arg);
struct sSvXdpSkt *sv_xdp_conf_skt(struct sSvOpt *opt, uint32_t num_frames, uint32_t frame_size);
int32_t sv_xdp_send(struct sSvXdpSkt *pkt_skt, uint32_t smp);
//struct sSvXdpSkt *sv_xdp_fill_frames(struct sSvOpt *opt, uint32_t num_frames, uint32_t frame_size);


#endif
