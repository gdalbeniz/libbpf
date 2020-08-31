/* Copyright (C) Oier Garcia de Albeniz Lopez - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 * Written by Oier Garcia de Albeniz Lopez <g.albeniz@gmail.com>, September 2020
 */
#ifndef __SV_PACKET__
#define __SV_PACKET__

#include "sv_config.h"


struct sSvPktSkt {
	int32_t socket;
	struct sockaddr_ll address[MAX_STREAMS];
	struct {
		struct mmsghdr msgvec[MAX_STREAMS];
		struct iovec iov[MAX_STREAMS];
	} aux[80];

	uint32_t pkt_sz;
	uint32_t pkt_num;
	uint8_t *pkt_area;

	//stats
	int32_t sleeptimes[16];
	uint32_t tx_npkts;

	struct sSvOpt *opt;
};

void *pkt_skt_stats(void *arg);
struct sSvPktSkt *sv_pkt_conf_skt(struct sSvOpt *opt);
void sv_pkt_send(struct sSvPktSkt *pkt_skt, uint32_t smp, uint32_t cnt);
void sv_pkt_send_all(struct sSvPktSkt *pkt_skt);
void sv_pkt_store_frame(struct sSvPktSkt *pkt_skt, uint8_t *buffer, uint32_t bufLen, uint32_t sv, uint32_t smp);

#endif
