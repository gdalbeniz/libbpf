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
};
struct sSvXdpSkt *sv_xdp_skt;

#endif
