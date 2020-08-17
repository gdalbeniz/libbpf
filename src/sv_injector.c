#include "sv_injector.h"
#include "sv_config.h"
#include "sv_frames.h"

struct sSvSocket sv_socket = {0};
struct sSvOpt sv_opt = {0};


void debug(const char *fmt, ...)
{
	if (sv_opt.debug) {
		va_list myargs;
    	va_start(myargs, fmt);
		vfprintf(stderr, fmt, myargs);
    	va_end(myargs);
	}
}




#define NEXT_SV 250000
void clock_addinterval(struct timespec *ts, uint64_t ns)
{
	ts->tv_nsec += ns;
	while (ts->tv_nsec >= 1000000000UL) {
		ts->tv_nsec -= 1000000000UL;
		ts->tv_sec += 1;
	}
}
int32_t clock_getdiff_us(struct timespec *tsref)
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return ((tsref->tv_sec - ts.tv_sec)) * 1000000L + ((tsref->tv_nsec - ts.tv_nsec) / 1000L);
}
//??? needed ???
uint64_t clock_gettime_ms()
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return ts.tv_sec * 1000UL + ts.tv_nsec / 1000000UL;
}
//??? needed ???
uint64_t clock_gettime_ns(void)
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return ts.tv_sec * 1000000000UL + ts.tv_nsec;
}






int main(int argc, char* argv[])
{
    // parse cli and cfg
	parse_command_line(argc, argv, &sv_opt);
	parse_cfg_file(&sv_opt);

    struct rlimit r = {RLIM_INFINITY, RLIM_INFINITY};
	if (setrlimit(RLIMIT_MEMLOCK, &r)) {
		fprintf(stderr, "ERROR: setrlimit(RLIMIT_MEMLOCK) \"%s\"\n", strerror(errno));
		exit(EXIT_FAILURE);
	}
	
	uint32_t sv_sample_sz = PACKETSIZE * SAMPLEWRAP * sv_opt.sv_num;
	uint8_t *sv_samples = (uint8_t *) malloc(sv_sample_sz); // uint8_t sv_samples[SAMPLEWRAP][sv_num][PACKETSIZE]
	memset(sv_samples, 0, sv_sample_sz);

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



    
    debug("==========================\n"
		"creating packets (%d bytes) \n", sv_sample_sz);
	debug("==========================\n"
		"estimated output: %d Kpps \n", sv_opt.sv_num * 4);
	debug("==========================\n");
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