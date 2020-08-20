#include "sv_injector.h"
#include "sv_config.h"
#include "sv_frames.h"
#include "sv_packet.h"
//#include "sv_xdp.h"

struct sSvPktSkt sv_pkt_skt = {0};
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


static void *stats(void *arg)
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


static void int_exit(int sig)
{
	(void)sig;
	printf("exiting\n");
	exit(EXIT_SUCCESS);
}






int main(int argc, char* argv[])
{
	// parse cli and cfg
	parse_command_line(argc, argv, &sv_opt);
	parse_cfg_file(&sv_opt);


	// init packets
	sv_pkt_skt.socket = socket(AF_PACKET, SOCK_RAW, 0);
	sv_pkt_skt.pkt_sz = PACKETSIZE;
	sv_pkt_skt.pkt_num = SAMPLEWRAP * sv_opt.sv_num;
	sv_pkt_skt.pkt_area = (uint8_t *) calloc(sv_pkt_skt.pkt_num, sv_pkt_skt.pkt_sz); // uint8_t sv_samples[SAMPLEWRAP][sv_num][PACKETSIZE]
	for (int i = 0; i < sv_opt.sv_num; i++) {
		int ret = sv_prepare(sv_pkt_skt.pkt_area, sv_opt.sv_conf, i);
		if (ret < 0) {
			printf("error: sv_prepare failed (%d)\n", ret);
			return -1;
		}
	}

	// set up process and stats thread
	struct rlimit r = {RLIM_INFINITY, RLIM_INFINITY};
	if (setrlimit(RLIMIT_MEMLOCK, &r)) {
		fprintf(stderr, "ERROR: setrlimit(RLIMIT_MEMLOCK) \"%s\"\n", strerror(errno));
		exit(EXIT_FAILURE);
	}
	
	signal(SIGINT, int_exit);
	signal(SIGTERM, int_exit);
	signal(SIGABRT, int_exit);

	setlocale(LC_ALL, "");

	pthread_t pt;
	int ret = pthread_create(&pt, NULL, stats, &sv_pkt_skt);
	if (ret) {
		exit(-1);//exit_with_error(ret);
	}
	
	// set process rt prio
	if (sv_opt.rt_prio) {
		struct sched_param param = {
			.sched_priority = sv_opt.rt_prio
		};
		int ret = sched_setscheduler(0, SCHED_FIFO, &param);
		if (ret) {
			printf("error: sched_setscheduler errno %d\n", errno);
			exit(-1);//exit_with_error(ret);
		}
	}
	

	printf("==========================\n");
	printf("creating packets (%d kB) \n", sv_pkt_skt.pkt_num * sv_pkt_skt.pkt_sz / 1024);
	printf("==========================\n");
	printf("estimated throughput: %d Kpps \n", 4 * sv_opt.sv_num);
	printf("==========================\n");
	printf("start sending\n");
	printf("==========================\n");


	// start sending

	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);

	for (int32_t i=0; ; i=0) {
		for (uint32_t smp = 0; smp < SAMPLEWRAP; smp++) {
			// sleep until next 250us
			clock_addinterval(&ts, NEXT_SV);
			if (smp % 400 == 0) {
				sv_pkt_skt.sleeptimes[i++] = clock_getdiff_us(&ts);
			}
			clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &ts, NULL);

			// send all sv for sample
			int res = sendmmsg(sv_pkt_skt.socket, sv_pkt_skt.aux[smp].msgvec, sv_opt.sv_num, 0);
			if (res == -1) {
				printf("sendmsg returned -1, errno = %d\n", errno);
				return -1;
			} else {
				sv_pkt_skt.tx_npkts += sv_opt.sv_num;
			}
		}
	}

	return 0;
}