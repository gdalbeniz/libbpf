#include "sv_injector.h"
#include "sv_packet.h"
#include "sv_frames.h"

struct sSvPktSkt *sv_pkt_conf_skt(struct sSvOpt *opt)
{
	struct sSvPktSkt *pski = calloc(1, sizeof(struct sSvPktSkt));
	if (!pski) {
		exit(-1);//exit_with_error(errno);
	}

	pski->socket = socket(AF_PACKET, SOCK_RAW, 0);
	pski->sv_num = opt->sv_num;
	pski->pkt_sz = PACKETSIZE;
	pski->pkt_num = SAMPLEWRAP * opt->sv_num;
	pski->pkt_area = (uint8_t *) calloc(pski->pkt_num, pski->pkt_sz); // uint8_t sv_samples[SAMPLEWRAP][sv_num][PACKETSIZE]

	// prepare packets
	for (uint32_t sv = 0; sv < opt->sv_num; sv++) {
		int32_t ret = sv_prepare(opt, pski, sv);
		if (ret < 0) {
			printf("error: sv_prepare failed (%d)\n", ret);
			return -1;
		}
	}
	printf("creating %d packets, frame size %d B, total size %d kB\n",
		pski->pkt_num, pski->pkt_sz, pski->pkt_num * pski->pkt_sz / 1024);

	return pski;
}

int32_t sv_pkt_send(struct sSvPktSkt *pkt_skt, uint32_t smp)
{
	// send all sv for sample
	int res = sendmmsg(pkt_skt->socket, pkt_skt->aux[smp].msgvec, pkt_skt->sv_num, 0);
	if (res == -1) {
		printf("sendmsg returned -1, errno = %d\n", errno);
		return -1;
	}
	
	pkt_skt->tx_npkts += pkt_skt->sv_num;
	return res;
}

void *pkt_skt_stats(void *arg)
{
	struct sSvPktSkt *pkt_skt = (struct sSvPktSkt *) arg;

	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);

	uint32_t prev_tx_npkts = pkt_skt->tx_npkts;
	for (;;) {
		clock_addinterval(&ts, 1000000000);
		clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &ts, NULL);

		printf("throughput: %d kpps ; sleep times: %d, %d, %d, %d, %d, %d, %d, %d, %d, %d us\n",
				(pkt_skt->tx_npkts - prev_tx_npkts)/1000,
				pkt_skt->sleeptimes[0], pkt_skt->sleeptimes[1], pkt_skt->sleeptimes[2], pkt_skt->sleeptimes[3], pkt_skt->sleeptimes[4],
				pkt_skt->sleeptimes[5], pkt_skt->sleeptimes[6], pkt_skt->sleeptimes[7], pkt_skt->sleeptimes[8], pkt_skt->sleeptimes[9]);
		prev_tx_npkts = pkt_skt->tx_npkts;
	}
	return NULL;
}

