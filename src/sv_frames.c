#include "sv_injector.h"
#include "sv_frames.h"
#include "sv_config.h"
#include "sv_packet.h"
#include "sv_xdp.h"




int32_t copySvpBuffer(SVPublisher svp, uint8_t *dest)
{
	struct fSVPublisher {
		uint8_t* buffer;
		uint16_t appId;
		void* ethernetSocket;
		int lengthField;
		int payloadStart;
		int payloadLength;
		int asduCount;
		void* asduList;
	};
	struct fSVPublisher *self = (struct fSVPublisher *) svp;

	int32_t bufLen = self->payloadStart + self->payloadLength;
	memcpy(dest, self->buffer, bufLen);

	return bufLen;
}

void printSvpBuffer(int32_t sv, int32_t smp, uint8_t *b, uint32_t blen)
{
	debug("············ sv %d smp %d len %d ············\n", sv, smp, blen);
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
		debug("%s\n", line);
	}
}


int32_t sv_smppoint(double rms, double degrees, double fact, int32_t point)
{
	double radians = 2 * M_PI * degrees / 360.0 + 2 * M_PI * point / 80.0;
	return (int32_t) (rms * sqrt(2) * sin(radians) * fact);
}


void sv_frame_smpcnt(uint8_t *frame, uint16_t smp)
{
	*(uint16_t*)(frame+39+frame[36]) = htons(smp);
}

int32_t sv_prepare(struct sSvOpt *opt, void *skt_info, uint32_t sv)
{
	struct sSvConf *conf = &opt->sv_conf[sv];
	struct sSvPktSkt *pkt_skt_info;
	struct sSvXdpSkt *xdp_skt_info;
	uint8_t *samples;
	if (opt->mode = 'P') {
		pkt_skt_info = (struct sSvPktSkt *)skt_info;
		samples = pkt_skt_info->pkt_area;
	} else /*if (opt->mode = 'X')*/ {
		xdp_skt_info = (struct sSvXdpSkt *)skt_info;
		samples = xdp_skt_info->umem_area;
	}

	CommParameters params = {
		.vlanPriority = conf->vlanPrio,
		.vlanId = conf->vlanId,
		.appId = conf->appId,
		.dstAddress[0] = conf->mac[0],
		.dstAddress[1] = conf->mac[1],
		.dstAddress[2] = conf->mac[2],
		.dstAddress[3] = conf->mac[3],
		.dstAddress[4] = conf->mac[4],
		.dstAddress[5] = conf->mac[5]
	};

	SVPublisher svp = SVPublisher_create(&params, opt->iface);
	if (!svp) {
		printf("error: SVPublisher_create, run as root? \n");
		return -1;
	}

	SVPublisher_ASDU asdu = SVPublisher_addASDU(svp, conf->svId, strlen(conf->datSet) ? conf->datSet : NULL, conf->confRev);
	
	int32_t amp1 = SVPublisher_ASDU_addINT32(asdu);
	int32_t amp1q = SVPublisher_ASDU_addQuality(asdu);
	int32_t amp2 = SVPublisher_ASDU_addINT32(asdu);
	int32_t amp2q = SVPublisher_ASDU_addQuality(asdu);
	int32_t amp3 = SVPublisher_ASDU_addINT32(asdu);
	int32_t amp3q = SVPublisher_ASDU_addQuality(asdu);
	int32_t amp4 = SVPublisher_ASDU_addINT32(asdu);
	int32_t amp4q = SVPublisher_ASDU_addQuality(asdu);
	int32_t vol1 = SVPublisher_ASDU_addINT32(asdu);
	int32_t vol1q = SVPublisher_ASDU_addQuality(asdu);
	int32_t vol2 = SVPublisher_ASDU_addINT32(asdu);
	int32_t vol2q = SVPublisher_ASDU_addQuality(asdu);
	int32_t vol3 = SVPublisher_ASDU_addINT32(asdu);
	int32_t vol3q = SVPublisher_ASDU_addQuality(asdu);
	int32_t vol4 = SVPublisher_ASDU_addINT32(asdu);
	int32_t vol4q = SVPublisher_ASDU_addQuality(asdu);

	SVPublisher_setupComplete(svp);

	for (uint16_t smp = 0; smp < SAMPLEWRAP; smp++) {
		SVPublisher_ASDU_setSmpCnt(asdu, smp);
		uint8_t point = smp % 80;

		// update currents
		int32_t currentA = sv_smppoint(conf->ia_mag, conf->ia_ang, 1000, point);
 		SVPublisher_ASDU_setINT32(asdu, amp1, currentA);
		SVPublisher_ASDU_setQuality(asdu, amp1q, conf->ia_q);
		int32_t currentB = sv_smppoint(conf->ib_mag, conf->ib_ang, 1000, point);
 		SVPublisher_ASDU_setINT32(asdu, amp2, currentB);
		SVPublisher_ASDU_setQuality(asdu, amp2q, conf->ib_q);
		int32_t currentC = sv_smppoint(conf->ic_mag, conf->ic_ang, 1000, point);
		SVPublisher_ASDU_setINT32(asdu, amp3, currentC);
		SVPublisher_ASDU_setQuality(asdu, amp3q, conf->ic_q);
		if (conf->in_q & QUALITY_DERIVED) {
			SVPublisher_ASDU_setINT32(asdu, amp4, currentA + currentB + currentC);
		} else {
			int32_t currentN = sv_smppoint(conf->in_mag, conf->in_ang, 1000, point);
			SVPublisher_ASDU_setINT32(asdu, amp4, currentN);
		}
		SVPublisher_ASDU_setQuality(asdu, amp4q, conf->in_q);

		// update voltages
		int32_t voltageA = sv_smppoint(conf->va_mag, conf->va_ang, 100, point);
 		SVPublisher_ASDU_setINT32(asdu, vol1, voltageA);
		SVPublisher_ASDU_setQuality(asdu, vol1q, conf->va_q);
		int32_t voltageB = sv_smppoint(conf->vb_mag, conf->vb_ang, 100, point);
 		SVPublisher_ASDU_setINT32(asdu, vol2, voltageB);
		SVPublisher_ASDU_setQuality(asdu, vol3q, conf->vb_q);
		int32_t voltageC = sv_smppoint(conf->vc_mag, conf->vc_ang, 100, point);
		SVPublisher_ASDU_setINT32(asdu, vol3, voltageC);
		SVPublisher_ASDU_setQuality(asdu, vol3q, conf->vc_q);
		if (conf->vn_q & QUALITY_DERIVED) {
			SVPublisher_ASDU_setINT32(asdu, vol4, voltageA + voltageB + voltageC);
		} else {
			int32_t voltageN = sv_smppoint(conf->vn_mag, conf->vn_ang, 100, point);
			SVPublisher_ASDU_setINT32(asdu, vol4, voltageN);
		}
		SVPublisher_ASDU_setQuality(asdu, vol4q, conf->vn_q);

		// copy packet
		uint8_t *sample_ptr = samples + smp * opt->sv_num * PACKETSIZE + sv * PACKETSIZE;// uint8_t sv_samples[SAMPLEWRAP][sv_num][PACKETSIZE]
		int32_t bufLen = copySvpBuffer(svp, sample_ptr);
		if (bufLen > PACKETSIZE) {
			printf("error: packet size (%d) too big\n", bufLen);
			return -1;
		}
#if 0
		printSvpBuffer(sv, smp, buffer, bufLen);
#endif

		if (opt->mode = 'P') {
			// prepare sendmmsg info
			pkt_skt_info->aux[smp].address[sv].sll_family = AF_PACKET;
			pkt_skt_info->aux[smp].address[sv].sll_protocol = htons(0x88ba);
			pkt_skt_info->aux[smp].address[sv].sll_ifindex = opt->ifindex;
			pkt_skt_info->aux[smp].address[sv].sll_halen = ETH_ALEN;
			pkt_skt_info->aux[smp].address[sv].sll_addr[0] = conf->mac[0];
			pkt_skt_info->aux[smp].address[sv].sll_addr[1] = conf->mac[1];
			pkt_skt_info->aux[smp].address[sv].sll_addr[2] = conf->mac[2];
			pkt_skt_info->aux[smp].address[sv].sll_addr[3] = conf->mac[3];
			pkt_skt_info->aux[smp].address[sv].sll_addr[4] = conf->mac[4];
			pkt_skt_info->aux[smp].address[sv].sll_addr[5] = conf->mac[5];
			pkt_skt_info->aux[smp].address[sv].sll_hatype = 0; // not needed
			pkt_skt_info->aux[smp].address[sv].sll_pkttype = 0; // not needed
			pkt_skt_info->aux[smp].msgvec[sv].msg_hdr.msg_name = &pkt_skt_info->aux[smp].address[sv];
			pkt_skt_info->aux[smp].msgvec[sv].msg_hdr.msg_namelen = sizeof(struct sockaddr_ll);
			pkt_skt_info->aux[smp].msgvec[sv].msg_hdr.msg_iov = &pkt_skt_info->aux[smp].iov[sv];
			pkt_skt_info->aux[smp].msgvec[sv].msg_hdr.msg_iovlen = 1;
			pkt_skt_info->aux[smp].msgvec[sv].msg_hdr.msg_control = NULL;
			pkt_skt_info->aux[smp].msgvec[sv].msg_hdr.msg_controllen = 0;
			pkt_skt_info->aux[smp].iov[sv].iov_base = sample_ptr;
			pkt_skt_info->aux[smp].iov[sv].iov_len = bufLen;
		} else /*if (opt->mode = 'X')*/ {
			
		}
	}

	SVPublisher_destroy(svp);
	return 0;
}