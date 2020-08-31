/* Copyright (C) Oier Garcia de Albeniz Lopez - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 * Written by Oier Garcia de Albeniz Lopez <g.albeniz@gmail.com>, September 2020
 */
#include "sv_injector.h"
#include "sv_packet.h"
#include "sv_frames.h"

struct sSvPktSkt *sv_pkt_conf_skt(struct sSvOpt *opt)
{
	struct sSvPktSkt *pski = calloc(1, sizeof(struct sSvPktSkt));
	if (!pski) {
		exit(-1);//exit_with_error(errno);
	}

	pski->opt = opt;
	pski->socket = socket(AF_PACKET, SOCK_RAW, 0);
	pski->pkt_sz = PACKETSIZE;
	pski->pkt_num = 80 * opt->sv_num;
	pski->pkt_area = (uint8_t *) calloc(pski->pkt_num, pski->pkt_sz); // uint8_t sv_samples[80][sv_num][PACKETSIZE]

	// prepare packets
	for (uint32_t sv = 0; sv < opt->sv_num; sv++) {
		int32_t ret = sv_frame_prepare(opt, pski, sv);
		if (ret < 0) {
			printf("error: sv_frame_prepare failed (%d)\n", ret);
			return -1;
		}
	}
	printf("creating %d packets, frame size %d B, total size %d kB\n",
		pski->pkt_num, pski->pkt_sz, pski->pkt_num * pski->pkt_sz / 1024);

	return pski;
}

static inline
uint8_t* sv_pkt_get_ptr(struct sSvPktSkt *pkt_skt, uint32_t sv, uint32_t smp)
{
	// uint8_t pkt_area[80][sv_num][pkt_sz]
	return pkt_skt->pkt_area + (smp * pkt_skt->opt->sv_num + sv) * pkt_skt->pkt_sz;
}

void sv_pkt_store_frame(struct sSvPktSkt *pkt_skt, uint8_t *buffer, uint32_t bufLen, uint32_t sv, uint32_t smp)
{
	uint8_t *sample_ptr = sv_pkt_get_ptr(pkt_skt, sv, smp % 80);
	memcpy(sample_ptr, buffer, bufLen);

	pkt_skt->address[sv].sll_family = AF_PACKET;
	pkt_skt->address[sv].sll_protocol = htons(0x88ba);
	pkt_skt->address[sv].sll_ifindex = pkt_skt->opt->ifindex;
	pkt_skt->address[sv].sll_halen = ETH_ALEN;
	pkt_skt->address[sv].sll_addr[0] = buffer[0];
	pkt_skt->address[sv].sll_addr[1] = buffer[1];
	pkt_skt->address[sv].sll_addr[2] = buffer[2];
	pkt_skt->address[sv].sll_addr[3] = buffer[3];
	pkt_skt->address[sv].sll_addr[4] = buffer[4];
	pkt_skt->address[sv].sll_addr[5] = buffer[5];
	pkt_skt->address[sv].sll_hatype = 0; // not needed
	pkt_skt->address[sv].sll_pkttype = 0; // not needed

	pkt_skt->aux[smp].msgvec[sv].msg_hdr.msg_name = &pkt_skt->address[sv];
	pkt_skt->aux[smp].msgvec[sv].msg_hdr.msg_namelen = sizeof(struct sockaddr_ll);
	pkt_skt->aux[smp].msgvec[sv].msg_hdr.msg_iov = &pkt_skt->aux[smp].iov[sv];
	pkt_skt->aux[smp].msgvec[sv].msg_hdr.msg_iovlen = 1;
	pkt_skt->aux[smp].msgvec[sv].msg_hdr.msg_control = NULL;
	pkt_skt->aux[smp].msgvec[sv].msg_hdr.msg_controllen = 0;
	pkt_skt->aux[smp].iov[sv].iov_base = sample_ptr;
	pkt_skt->aux[smp].iov[sv].iov_len = bufLen;
}


void sv_pkt_send(struct sSvPktSkt *pkt_skt, uint32_t smp, uint32_t cnt)
{
	// update sample point for next batch
	for (uint32_t sv = 0; sv < pkt_skt->opt->sv_num; sv++) {
		uint8_t *sample_ptr = sv_pkt_get_ptr(pkt_skt, sv, cnt);
		sv_frame_smp_upd(sample_ptr, (uint16_t)smp);
	}

	// send all sv for sample
	int res = sendmmsg(pkt_skt->socket, pkt_skt->aux[cnt].msgvec, pkt_skt->opt->sv_num, 0);
	if (res == -1) {
		printf("sendmsg returned -1, errno = %d\n", errno);
		exit(-1);
	}

	pkt_skt->tx_npkts += pkt_skt->opt->sv_num;

	return res;
}



void sv_pkt_send_all(struct sSvPktSkt *pktsi)
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);

	for (;;) {
		int32_t islp = 0;
		for (uint32_t smp = 0; smp < SAMPLEWRAP; smp++) {
			// sleep until next 250us
			clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &ts, NULL);
			clock_addinterval(&ts, 250000);

			// send packets
			sv_pkt_send(pktsi, smp, smp%80);
			if (smp % 400 == 0) {
				pktsi->sleeptimes[islp++] = clock_getdiff_us(&ts);
			}
		}
	}
}



void *pkt_skt_stats(void *arg)
{
	struct sSvPktSkt *pkt_skt = (struct sSvPktSkt *) arg;

	struct timespec ts;
	clock_gettime(CLOCK_REALTIME, &ts);

	uint32_t to = ts.tv_sec;
	if (pkt_skt->opt->timeout > 0 && pkt_skt->opt->timeout <= 600 && ts.tv_sec <= 0x60000000) {
		to = ts.tv_sec + pkt_skt->opt->timeout;
	}

	for (;;) {
		uint32_t prev_tx_npkts = pkt_skt->tx_npkts;

		if (++ts.tv_sec > to) exit(0);
		clock_nanosleep(CLOCK_REALTIME, TIMER_ABSTIME, &ts, NULL);

		printf("throughput: %d pps ; sleep times: %d, %d, %d, %d, %d, %d, %d, %d, %d, %d us\n",
				pkt_skt->tx_npkts - prev_tx_npkts,
				pkt_skt->sleeptimes[0], pkt_skt->sleeptimes[1], pkt_skt->sleeptimes[2], pkt_skt->sleeptimes[3], pkt_skt->sleeptimes[4],
				pkt_skt->sleeptimes[5], pkt_skt->sleeptimes[6], pkt_skt->sleeptimes[7], pkt_skt->sleeptimes[8], pkt_skt->sleeptimes[9]);
	}
	return NULL;
}

