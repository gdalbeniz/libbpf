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

	uint32_t sv_num;
	uint32_t pkt_sz;
	uint32_t pkt_num;
	uint32_t bufLen;//hack, len of 1 packet
	uint32_t err_tx;

	uint32_t tx_npkts;
	uint32_t prev_tx_npkts;
	uint32_t outstanding_tx;
	uint32_t prog_id;
	int32_t sleeptimes[16];
};

void *xdp_skt_stats(void *arg);
struct sSvXdpSkt *sv_xdp_conf_skt(struct sSvOpt *opt);
void sv_xdp_send(struct sSvXdpSkt *xski, uint32_t *frame_nb, uint32_t batch_sz);
int32_t sv_xdp_send2(struct sSvXdpSkt *pkt_skt, uint32_t smp);
void sv_xdp_send_all(struct sSvXdpSkt *pkt_skt);
void sv_xdp_store_frame(struct sSvXdpSkt *pkt_skt, uint8_t *buffer, uint32_t bufLen, uint32_t sv, uint32_t smp);


#endif
