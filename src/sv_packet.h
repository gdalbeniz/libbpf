#ifndef __SV_PACKET__
#define __SV_PACKET__

#include "sv_config.h"


struct sSvPktSkt {
	int32_t socket;
	struct {
		struct sockaddr_ll address[MAX_STREAMS];
		struct mmsghdr msgvec[MAX_STREAMS];
		struct iovec iov[MAX_STREAMS];
	} aux[SAMPLEWRAP];

	uint32_t sv_num;
	uint32_t pkt_sz;
	uint32_t pkt_num;
	uint8_t *pkt_area;

	//stats
	int32_t sleeptimes[16];
	uint32_t tx_npkts;
};

void *pkt_skt_stats(void *arg);
struct sSvPktSkt *sv_pkt_conf_skt(struct sSvOpt *opt);
int32_t sv_pkt_send(struct sSvPktSkt *pkt_skt, uint32_t smp);

#endif
