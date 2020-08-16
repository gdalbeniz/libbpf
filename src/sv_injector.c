#include "sv_injector.h"



//TODO move
void debug(const char *fmt, ...)
{
	if (sv_opt.debug) {
		va_list myargs;
    	va_start(myargs, fmt);
		vfprintf(stderr, fmt, myargs);
    	va_end(myargs);
	}
}







int32_t sv_smppoint(double rms, double degrees, double fact, int32_t point)
{
	double radians = 2 * M_PI * degrees / 360.0 + 2 * M_PI * point / 80.0;
	return (int32_t) (rms * sqrt(2) * sin(radians) * fact);
}


uint16_t reverse16(uint16_t x)
{
    x = (((x & 0xAAAA) >> 1) | ((x & 0x5555) << 1));
    x = (((x & 0xCCCC) >> 2) | ((x & 0x3333) << 2));
    x = (((x & 0xF0F0) >> 4) | ((x & 0x0F0F) << 4));
    return (x >> 8) | (x << 8);
}



struct sSvSocket sv_socket = {0};




#define NEXT_SV 250000
void clock_addinterval(struct timespec *ts, unsigned long ns)
{
	ts->tv_nsec += ns;
	while (ts->tv_nsec >= 1000000000) {
		ts->tv_nsec -= 1000000000;
		ts->tv_sec += 1;
	}
}
uint64_t clock_gettime_ms()
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return ((uint64_t) ts.tv_sec) * 1000 + (ts.tv_nsec / 1000000);
}
int32_t clock_getdiff_us(struct timespec *tsref)
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return ((tsref->tv_sec - ts.tv_sec)) * 1000000 + ((tsref->tv_nsec - ts.tv_nsec) / 1000);
}


struct sSvOpt sv_opt = {0};

int main(int argc, char* argv[])
{
    // parse cli and cfg
	parse_command_line(argc, argv, &sv_opt);
	parse_cfg_file(&sv_opt);

	// print conf
	printSvOpt(&sv_opt);
	

	debug("==========================\n");
	if (sv_opt.sv_num > SV_MAX) {
		printf("error: too many sv (%d > %d)\n", sv_opt.sv_num, SV_MAX);
		return -1;
	} else if ((sv_opt.sv_num > sv_opt.sv_limit) && (sv_opt.sv_limit > 0)) {
		debug("Succesfully parsed %d SV streams. Limited to %d \n", sv_opt.sv_num, sv_opt.sv_limit);
		sv_opt.sv_num = sv_opt.sv_limit;
	} else {
		debug("Succesfully parsed %d SV streams \n", sv_opt.sv_num);
	}
	
	uint32_t sv_sample_sz = PACKETSIZE * SAMPLEWRAP * sv_opt.sv_num;
	uint8_t *sv_samples = (uint8_t *) malloc(sv_sample_sz); // uint8_t sv_samples[SAMPLEWRAP][sv_num][PACKETSIZE]
	memset(sv_samples, 0, sv_sample_sz);

	debug("==========================\n"
		"creating packets (%d bytes) \n", sv_sample_sz);
	debug("==========================\n"
		"estimated output: %d Kpps \n", sv_opt.sv_num * 4);
	debug("==========================\n");

	sv_socket.socket = socket(AF_PACKET, SOCK_RAW, 0);
	// int32_t sock_qdisc_bypass = 1;
	// errno = 0;
	// int32_t sock_qdisc_ret = setsockopt(sv_socket.socket, SOL_PACKET, PACKET_QDISC_BYPASS, &sock_qdisc_bypass, sizeof(sock_qdisc_bypass));
	// debug("setsockopt PACKET_QDISC_BYPASS returned %d, errno %d\n", sock_qdisc_ret, errno);


	for (int i = 0; i < sv_opt.sv_num; i++) {
		int ret = sv_prepare(sv_samples, sv_opt.sv_conf, i);
		if (ret < 0) {
			printf("error: sv_prepare failed (%d)\n", ret);
			return -1;
		}
	}


#if 0
	for (int smp = 0; smp < SAMPLEWRAP; smp++) {
		for (int sv = 0; sv < sv_num; sv++) {
			uint8_t *b = sv_samples + smp * sv_num * PACKETSIZE + sv * PACKETSIZE;// uint8_t sv_samples[SAMPLEWRAP][sv_num][PACKETSIZE]
			debug("========= sv %d smp %d =========\n", sv, smp);
			uint8_t blen = sv_len;
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
	}
#endif

	debug("start sending\n");
	int32_t sleeptimes[600] = {0};
	int32_t sleepindex = 0;

	// start sending
	struct timespec tsnext;
	clock_gettime(CLOCK_MONOTONIC, &tsnext);

	bool running = true;
	while (running) {
		for (uint32_t smp = 0; smp < SAMPLEWRAP; smp++) {
			// sleep until next 250us
			clock_addinterval(&tsnext, NEXT_SV);
			if (smp % 400 == 0) {
				sleeptimes[sleepindex++] = clock_getdiff_us(&tsnext);
				if (sleepindex >= sizeof(sleeptimes)/sizeof(int32_t)) {
					for (int i = 0; i < sizeof(sleeptimes)/sizeof(int32_t); i++) {
						debug("diff time [%d] = %d us\n", i, sleeptimes[i]);
					}
					return 0;
				}
			}
			clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &tsnext, NULL);

			// if (smp % 4 == 0) {
			// 	// sync every 1ms, 4 samples
			// 	clock_addinterval(&tsnext, 4*NEXT_SV);
			// 	int32_t diff = clock_getdiff_us(&tsnext);
			// 	if (diff > 10) {
			// 		usleep(diff - 10);
			// 	}
			// 	if (smp % 400 == 0) {
			// 		sleeptimes[sleepindex++] = diff;
			// 		if (sleepindex >= sizeof(sleeptimes)/sizeof(int32_t)) {
			// 			for (int i = 0; i < sizeof(sleeptimes)/sizeof(int32_t); i++) {
			// 				debug("diff time [%d] = %d us\n", i, sleeptimes[i]);
			// 			}
			// 			return 0;
			// 		}
			// 	}
			// }
			// send all sv for sample
			errno = 0;
			int res = sendmmsg(sv_socket.socket, sv_socket.samp[smp].msgvec, sv_opt.sv_num, 0);
			if (res == -1) {
				printf("sendmsg returned -1, errno = %d\n", errno);
				return -1;
			} else {
				//debug("smp %d, sendmsg returned %d\n", smp, res);
			}
		}
	}



	
	return 0;
}