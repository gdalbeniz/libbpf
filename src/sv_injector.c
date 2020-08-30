#include "sv_injector.h"
#include "sv_config.h"
#include "sv_frames.h"
#include "sv_packet.h"
#include "sv_xdp.h"

char __debug__ = 0;

void debug(const char *fmt, ...)
{
	if (!__debug__) return;
	va_list myargs;
	va_start(myargs, fmt);
	vfprintf(stderr, fmt, myargs);
	va_end(myargs);
}




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



static void int_exit(int sig)
{
	(void)sig;
	printf("exiting\n");
	exit(EXIT_SUCCESS);
}



int main(int argc, char* argv[])
{
	int32_t ret;
	pthread_t pt;
	struct sSvOpt *opt_info;
	struct sSvPktSkt *pkt_skt_info;
	struct sSvXdpSkt *xdp_skt_info;

	// parse cli and cfg
	opt_info = parse_command_line(argc, argv);
	parse_cfg_file(opt_info);

	// init socket and prepare frames
	if (opt_info->mode == 'P') {
		pkt_skt_info = sv_pkt_conf_skt(opt_info);
	} else /*if (opt_info->mode == 'X')*/ {
		xdp_skt_info = sv_xdp_conf_skt(opt_info);
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

	if (opt_info->mode == 'P') {
		ret = pthread_create(&pt, NULL, pkt_skt_stats, pkt_skt_info);
	} else /*if (opt_info->mode == 'X')*/ {
		ret = pthread_create(&pt, NULL, xdp_skt_stats, xdp_skt_info);
	}
	if (ret) {
		exit(-1);//exit_with_error(ret);
	}
	
	// set process rt prio
	if (opt_info->rt_prio) {
		struct sched_param param = {
			.sched_priority = opt_info->rt_prio
		};
		int ret = sched_setscheduler(0, SCHED_FIFO, &param);
		if (ret) {
			printf("error: sched_setscheduler errno %d\n", errno);
			exit(-1);//exit_with_error(ret);
		}
	}


	printf("==========================\n");
	printf("start sending (estimated throughput: %d Kpps) \n", 4 * opt_info->sv_num);
	printf("==========================\n");

	// send packets
	if (opt_info->mode == 'P') {
		sv_pkt_send_all(pkt_skt_info);
	} else /*if (opt_info->mode == 'X')*/ {
		sv_xdp_send_all(xdp_skt_info);
	}

	return 0;
}