#include "sv_injector.h"





void getSvpBuffer(SVPublisher svp, void **buffer, uint32_t *bufLen)
{
	struct fSVPublisher {
		uint8_t* buffer;
		uint16_t appId;
		void* ethernetSocket;
		int lengthField; /* can probably be removed since packets have fixed size! */
		int payloadStart;
		int payloadLength; /* length of payload buffer */
		int asduCount; /* number of ASDUs in the APDU */
		void* asduList;
	};
	struct fSVPublisher *self = (struct fSVPublisher *) svp;
    *buffer = self->buffer;
    *bufLen = self->payloadStart + self->payloadLength;
}


int32_t sv_prepare(uint8_t *samples, struct sSvConf *conf, uint32_t sv)
{
	CommParameters params;
	params.vlanPriority = conf[sv].vlanPrio;
	params.vlanId = conf[sv].vlanId;
	params.appId = conf[sv].appId;
	params.dstAddress[0] = conf[sv].mac[0];
	params.dstAddress[1] = conf[sv].mac[1];
	params.dstAddress[2] = conf[sv].mac[2];
	params.dstAddress[3] = conf[sv].mac[3];
	params.dstAddress[4] = conf[sv].mac[4];
	params.dstAddress[5] = conf[sv].mac[5];

	SVPublisher svp = SVPublisher_create(&params, sv_opt.iface);
	if (!svp) {
		printf("error: SVPublisher_create, run as root? \n");
		return -1;
	}

	SVPublisher_ASDU asdu = SVPublisher_addASDU(svp, conf[sv].svId, strlen(conf[sv].datSet) ? conf[sv].datSet : NULL, conf[sv].confRev);
	
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

	//SVPublisher_ASDU_setSmpCntWrap(asdu, SAMPLEWRAP);//?
	SVPublisher_setupComplete(svp);

	for (uint16_t smp = 0; smp < SAMPLEWRAP; smp++) {
		SVPublisher_ASDU_setSmpCnt(asdu, smp);
		uint8_t point = smp % 80;

		// update currents
		int32_t currentA = sv_smppoint(conf[sv].ia_mag, conf[sv].ia_ang, 1000, point);
 		SVPublisher_ASDU_setINT32(asdu, amp1, currentA);
		SVPublisher_ASDU_setQuality(asdu, amp1q, reverse16(conf[sv].ia_q));
		int32_t currentB = sv_smppoint(conf[sv].ib_mag, conf[sv].ib_ang, 1000, point);
 		SVPublisher_ASDU_setINT32(asdu, amp2, currentB);
		SVPublisher_ASDU_setQuality(asdu, amp2q, reverse16(conf[sv].ib_q));
		int32_t currentC = sv_smppoint(conf[sv].ic_mag, conf[sv].ic_ang, 1000, point);
		SVPublisher_ASDU_setINT32(asdu, amp3, currentC);
		SVPublisher_ASDU_setQuality(asdu, amp3q, reverse16(conf[sv].ic_q));
		if (conf[sv].in_q & QUALITY_DERIVED) {
			SVPublisher_ASDU_setINT32(asdu, amp4, currentA + currentB + currentC);
		} else {
			int32_t currentN = sv_smppoint(conf[sv].in_mag, conf[sv].in_ang, 1000, point);
			SVPublisher_ASDU_setINT32(asdu, amp4, currentN);
		}
		SVPublisher_ASDU_setQuality(asdu, amp4q, reverse16(conf[sv].in_q));

		// update voltages
		int32_t voltageA = sv_smppoint(conf[sv].va_mag, conf[sv].va_ang, 100, point);
 		SVPublisher_ASDU_setINT32(asdu, vol1, voltageA);
		SVPublisher_ASDU_setQuality(asdu, vol1q, reverse16(conf[sv].va_q));
		int32_t voltageB = sv_smppoint(conf[sv].vb_mag, conf[sv].vb_ang, 100, point);
 		SVPublisher_ASDU_setINT32(asdu, vol2, voltageB);
		SVPublisher_ASDU_setQuality(asdu, vol3q, reverse16(conf[sv].vb_q));
		int32_t voltageC = sv_smppoint(conf[sv].vc_mag, conf[sv].vc_ang, 100, point);
		SVPublisher_ASDU_setINT32(asdu, vol3, voltageC);
		SVPublisher_ASDU_setQuality(asdu, vol3q, reverse16(conf[sv].vc_q));
		if (conf[sv].vn_q & QUALITY_DERIVED) {
			SVPublisher_ASDU_setINT32(asdu, vol4, voltageA + voltageB + voltageC);
		} else {
			int32_t voltageN = sv_smppoint(conf[sv].vn_mag, conf[sv].vn_ang, 100, point);
			SVPublisher_ASDU_setINT32(asdu, vol4, voltageN);
		}
		SVPublisher_ASDU_setQuality(asdu, vol4q, reverse16(conf[sv].vn_q));

		// copy packet
		uint8_t *buffer;
		uint32_t bufLen;
		getSvpBuffer(svp, &buffer, &bufLen);
		if (bufLen > PACKETSIZE) {
			printf("error: packet size (%d) too big\n", bufLen);
			return -1;
		}
		uint8_t *sample_ptr = samples + smp * sv_opt.sv_num * PACKETSIZE + sv * PACKETSIZE;// uint8_t sv_samples[SAMPLEWRAP][sv_num][PACKETSIZE]
		memcpy(sample_ptr, buffer, bufLen);

		// prepare sendmmsg info
		sv_socket.samp[smp].address[sv].sll_family = AF_PACKET;
		sv_socket.samp[smp].address[sv].sll_protocol = htons(0x88ba);
		sv_socket.samp[smp].address[sv].sll_ifindex = sv_opt.ifindex;
		sv_socket.samp[smp].address[sv].sll_halen = ETH_ALEN;
		sv_socket.samp[smp].address[sv].sll_addr[0] = conf[sv].mac[0];
		sv_socket.samp[smp].address[sv].sll_addr[1] = conf[sv].mac[1];
		sv_socket.samp[smp].address[sv].sll_addr[2] = conf[sv].mac[2];
		sv_socket.samp[smp].address[sv].sll_addr[3] = conf[sv].mac[3];
		sv_socket.samp[smp].address[sv].sll_addr[4] = conf[sv].mac[4];
		sv_socket.samp[smp].address[sv].sll_addr[5] = conf[sv].mac[5];
		sv_socket.samp[smp].address[sv].sll_hatype = 0; // not needed
		sv_socket.samp[smp].address[sv].sll_pkttype = 0; // not needed
		sv_socket.samp[smp].msgvec[sv].msg_hdr.msg_name = &sv_socket.samp[smp].address[sv];
		sv_socket.samp[smp].msgvec[sv].msg_hdr.msg_namelen = sizeof(struct sockaddr_ll);
		sv_socket.samp[smp].msgvec[sv].msg_hdr.msg_iov = &sv_socket.samp[smp].iov[sv];
		sv_socket.samp[smp].msgvec[sv].msg_hdr.msg_iovlen = 1;
		sv_socket.samp[smp].msgvec[sv].msg_hdr.msg_control = NULL;
		sv_socket.samp[smp].msgvec[sv].msg_hdr.msg_controllen = 0;
		sv_socket.samp[smp].iov[sv].iov_base = sample_ptr;
		sv_socket.samp[smp].iov[sv].iov_len = bufLen;
	}

	SVPublisher_destroy(svp);
	return 0;
}