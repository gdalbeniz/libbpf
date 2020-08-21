#include "sv_injector.h"
#include "sv_config.h"
#include "ini.h"


#define MATCH(a,b) (!strcmp(a?a:"", b?b:""))




static uint16_t parse_q(const char *value)
{
	uint16_t q = 0;

	char* token;
	char *delim = ",|";

	for (char *token = strtok(value, delim);
		token != NULL;
		token = strtok(NULL, delim))
	{
		if (MATCH(token, "GOOD")) {
			q |= QUALITY_VALIDITY_GOOD;
		} else if (MATCH(token, "INVALID")) {
			q |= QUALITY_VALIDITY_INVALID;
		} else if (MATCH(token, "QUESTIONABLE")) {
			q |= QUALITY_VALIDITY_QUESTIONABLE;
		} else if (MATCH(token, "TEST")) {
			q |= QUALITY_TEST;
		} else if (MATCH(token, "DERIVED")) {
			q |= QUALITY_DERIVED;
		} else {
			// TODO ADD MORE Q:
			// OVERFLOW, OUT_OF_RANGE, BAD_REFERENCE, OSCILLATORY, FAILURE, OLD_DATA, INCONSISTENT, INACCURATE, SUBSTITUTED, OPERATOR_BLOCKED
			printf("error: unsupported quality '%s'\n", token ? token : "nil");
			return -1;
		}
	}
	return q;
}

static int32_t parse_mac(uint8_t *mac, const char *value, const char *section)
{
	uint8_t i = 0;

	char temp[MAXLEN];
	snprintf(temp, MAXLEN, value, section);


	char* token;
	char *delim = ":-";

	for (char *token = strtok(temp, delim);
		token != NULL;
		token = strtok(NULL, delim))
	{
		uint32_t octet = strtoul(token, NULL, 16);
		if (octet >= 256 || i >= 6) {
			printf("error: parsing mac\n");
			return -1;
		}
		mac[i] = (uint8_t) octet;
		i++;
	}
	return 0;
}



static int ini_parse_handler(void* user, const char* section, const char* key, const char* value, int lineno)
{
	static struct sSvConf *conf = NULL;
	struct sSvOpt *opt = (struct sSvOpt *) user;

	// new section
	if (key == NULL) {
		// sv stream
		//debug("--- new section (%d) = %s\n", opt->sv_num, section);
		if (opt->sv_num >= opt->sv_alloc) {
			// allocate more space
			opt->sv_alloc += 64;
			opt->sv_conf = realloc(opt->sv_conf, opt->sv_alloc * sizeof(struct sSvConf));
			//debug("--- allocate total = %d\n", opt->sv_alloc);
		}
		conf = &opt->sv_conf[opt->sv_num];
		opt->sv_num++;
		// copy from default
		memcpy(conf, &opt->def_conf, sizeof(struct sSvConf));
		snprintf(conf->section, MAXLEN, section);
		return 1; //ok
	}

	// global default section
	if (conf == NULL && *section == 0) {
		//debug("--- no section  -> default\n");
		conf = &opt->def_conf;
	}

	//debug("==> [%s] %s = %s\n", section, key, value);

	// fill key-value
	if (MATCH(key, "mac")) {
		int ret = parse_mac(conf->mac, value, section);
		if (ret) {
			printf("error (line %d): not valid mac\n", lineno);
			return 0; //error
		}
	} else if (MATCH(key, "vlanId")) {
		conf->vlanId = (uint16_t) strtol(value, NULL, 0);
	} else if (MATCH(key, "vlanPrio")) {
		conf->vlanPrio = (uint8_t) strtol(value, NULL, 0);
	} else if (MATCH(key, "appId")) {
		conf->appId = (uint16_t) strtol(value, NULL, 0);
	} else if (MATCH(key, "svId")) {
		snprintf(conf->svId, MAXLEN, value, section);
	} else if (MATCH(key, "datSet")) {
		snprintf(conf->datSet, MAXLEN, value, section);
	} else if (MATCH(key, "confRev")) {
		conf->confRev = (uint32_t) strtol(value, NULL, 0);
	} else if (MATCH(key, "ia_mag")) {
		conf->ia_mag = strtod(value, NULL);
	} else if (MATCH(key, "ia_ang")) {
		conf->ia_ang = strtod(value, NULL);
	} else if (MATCH(key, "ia_q")) {
		conf->ia_q = parse_q(value);
	} else if (MATCH(key, "ib_mag")) {
		conf->ib_mag = strtod(value, NULL);
	} else if (MATCH(key, "ib_ang")) {
		conf->ib_ang = strtod(value, NULL);
	} else if (MATCH(key, "ib_q")) {
		conf->ib_q = parse_q(value);
	} else if (MATCH(key, "ic_mag")) {
		conf->ic_mag = strtod(value, NULL);
	} else if (MATCH(key, "ic_ang")) {
		conf->ic_ang = strtod(value, NULL);
	} else if (MATCH(key, "ic_q")) {
		conf->ic_q = parse_q(value);
	} else if (MATCH(key, "in_mag")) {
		conf->in_mag = strtod(value, NULL);
	} else if (MATCH(key, "in_ang")) {
		conf->in_ang = strtod(value, NULL);
	} else if (MATCH(key, "in_q")) {
		conf->in_q = parse_q(value);
	} else if (MATCH(key, "va_mag")) {
		conf->va_mag = strtod(value, NULL);
	} else if (MATCH(key, "va_ang")) {
		conf->va_ang = strtod(value, NULL);
	} else if (MATCH(key, "va_q")) {
		conf->va_q = parse_q(value);
	} else if (MATCH(key, "vb_mag")) {
		conf->vb_mag = strtod(value, NULL);
	} else if (MATCH(key, "vb_ang")) {
		conf->vb_ang = strtod(value, NULL);
	} else if (MATCH(key, "vb_q")) {
		conf->vb_q = parse_q(value);
	} else if (MATCH(key, "vc_mag")) {
		conf->vc_mag = strtod(value, NULL);
	} else if (MATCH(key, "vc_ang")) {
		conf->vc_ang = strtod(value, NULL);
	} else if (MATCH(key, "vc_q")) {
		conf->vc_q = parse_q(value);
	} else if (MATCH(key, "vn_mag")) {
		conf->vn_mag = strtod(value, NULL);
	} else if (MATCH(key, "vn_ang")) {
		conf->vn_ang = strtod(value, NULL);
	} else if (MATCH(key, "vn_q")) {
		conf->vn_q = parse_q(value);
	} else {
		//TODO
		printf("error (line %d): attribute '%s' unsupported\n", lineno, key);
		return 0; //error
	}

	return 1; //ok
}





void printSvOpt(struct sSvOpt *opt)
{
	if (!opt->debug) return;

	debug("************ options ************\n");
	debug("debug %d, iface %s, ifindex %d, mode %c, cfg %s, sv_limit %d, sv_num %d\n",
		opt->debug, opt->iface, opt->ifindex, opt->mode, opt->cfg_file, opt->sv_limit, opt->sv_num);

	for (int i = 0; i < opt->sv_num; i++) {
		struct sSvConf *conf = opt->sv_conf + i;
		debug("============ [%s] ===========\n", conf->section);
		debug("mac: %02x:%02x:%02x:%02x:%02x:%02x, vlanPrio: %d, vlanId: %d, appId: 0x%04x\n",
			conf->mac[0], conf->mac[1], conf->mac[2], conf->mac[3], conf->mac[4], conf->mac[5], conf->vlanPrio, conf->vlanId, conf->appId);
		debug("svId: %s, datSet: %s, confRev: %d\n", conf->svId, conf->datSet, conf->confRev);
		debug("ia: {%.1f, %.1f, 0x%04x}, ib: {%.1f, %.1f, 0x%04x}, ic: {%.1f, %.1f, 0x%04x}, in: {%.1f, %.1f, 0x%04x}\n",
			conf->ia_mag, conf->ia_ang, conf->ia_q, conf->ib_mag, conf->ib_ang, conf->ib_q,
			conf->ic_mag, conf->ic_ang, conf->ic_q, conf->in_mag, conf->in_ang, conf->in_q);
		debug("va: {%.1f, %.1f, 0x%04x}, vb: {%.1f, %.1f, 0x%04x}, vc: {%.1f, %.1f, 0x%04x}, vn: {%.1f, %.1f, 0x%04x}\n",
			conf->va_mag, conf->va_ang, conf->va_q, conf->vb_mag, conf->vb_ang, conf->vb_q,
			conf->vc_mag, conf->vc_ang, conf->vc_q, conf->vn_mag, conf->vn_ang, conf->vn_q);
	}
}




void parse_cfg_file(struct sSvOpt *opt)
{
	if (opt->cfg_file == NULL) {
		printf("No config file!\n");
		exit(EXIT_FAILURE);
	}

	int ret = ini_parse(opt->cfg_file, ini_parse_handler, opt);
	if (ret < 0) {
		printf("Can't read '%s'!\n", opt->cfg_file);
		exit(EXIT_FAILURE);
	} else if (ret) {
		printf("Bad config file (first error on line %d)!\n", ret);
		exit(EXIT_FAILURE);
	}

	if (opt->sv_num > MAX_STREAMS) {
		printf("error: too many sv (%d > %d)\n",opt->sv_num, MAX_STREAMS);
		exit(EXIT_FAILURE);
	} else if ((opt->sv_num > opt->sv_limit) && (opt->sv_limit > 0)) {
		printf("Succesfully parsed %d SV streams. Limited to %d \n",opt->sv_num,opt->sv_limit);
		opt->sv_num =opt->sv_limit;
	} else {
		printf("Succesfully parsed %d SV streams \n",opt->sv_num);
	}

	printSvOpt(opt);
}








struct option long_options[] = {
	{"pkt-sendmmsg", no_argument, 0, 'P'},
	{"xdp-skb", no_argument, 0, 'S'},
	{"xdp-native", no_argument, 0, 'X'},
	{"interface", required_argument, 0, 'i'},
	{"config", required_argument, 0, 'c'},
	{"limit", required_argument, 0, 'l'},
	{"debug", no_argument, 0, 'd'},
	{"rt-prio", required_argument, 0, 'r'},

	{"poll", no_argument, 0, 'p'},
	{"interval", required_argument, 0, 'n'},
	{"zero-copy", no_argument, 0, 'z'},
	{"no-need-wakeup", no_argument, 0, 'm'},
	{"frame-size", required_argument, 0, 'f'},
	{"unaligned", no_argument, 0, 'u'},
	{0, 0, 0, 0}
};

void usage(const char *prog)
{
	const char *str =
		"  Usage: %s [OPTIONS]\n"
		"  Options:\n"
		"  -P, --pkt-sendmmsg   Use AF_PACKET mode\n"
		"  -S, --xdp-skb        Use AF_XDP skb-mod\n"
		"  -X, --xdp-native     Use AF_XDP native mode\n"
		"  -i, --interface=s    Run on interface 's'\n"
		"  -c, --config=s       Configuration file='s'.\n"
		"  -l, --limit=n        Limit number of streams to n\n"
		"  -d, --debug          Debug\n"
		"  -r, --rt-prio=n      Set RT priority to n\n"

		"  -p, --poll           Use poll syscall\n"
		"  -n, --interval=n     Specify statistics update interval (default 1 sec).\n"
		"  -z, --zero-copy      Force zero-copy mode.\n"
		"  -m, --no-need-wakeup Turn off use of driver need wakeup flag.\n"
		"  -f, --frame-size=n   Set the frame size (must be a power of two in aligned mode, default is %d).\n"
		"  -u, --unaligned      Enable unaligned chunk placement\n"
		"\n";
	fprintf(stderr, str, prog, XSK_UMEM__DEFAULT_FRAME_SIZE);
	exit(EXIT_FAILURE);
}

struct sSvOpt* parse_command_line(int argc, char **argv)
{
	struct sSvOpt *opt = calloc(1, sizeof(struct sSvOpt));
	opt->xdp_flags = XDP_FLAGS_UPDATE_IF_NOEXIST;
	opt->interval = 1;
	opt->xdp_bind_flags = XDP_USE_NEED_WAKEUP | XDP_COPY;
	opt->xsk_frame_size = XSK_UMEM__DEFAULT_FRAME_SIZE;
	opt->need_wakeup = true;
	opt->mode = 'P';

	for (;;) {
		int c = getopt_long(argc, argv, "PSXi:c:l:dr:pn:zmf:u", long_options, NULL);
		if (c == -1) {
			break;
		}

		switch (c) {
		case 'd':
			opt->debug = true;
			break;
		case 'i':
			opt->iface = optarg;
			break;
		case 'p':
			opt->poll = 1;
			break;
		case 'S':
			opt->mode = 'X';
			opt->xdp_flags |= XDP_FLAGS_SKB_MODE;
			break;
		case 'X':
			opt->mode = 'X';
			opt->xdp_flags |= XDP_FLAGS_DRV_MODE;
			break;
		case 'P':
			opt->mode = 'P';
			break;
		case 'n':
			opt->interval = atoi(optarg);
			break;
		case 'l':
			opt->sv_limit = atoi(optarg);
			break;
		case 'r':
			opt->rt_prio = atoi(optarg);
			break;
		case 'z':
			opt->xdp_bind_flags |= XDP_ZEROCOPY;
			opt->xdp_bind_flags &= ~XDP_COPY;
			break;
		case 'c':
			opt->cfg_file = optarg;
			break;
		case 'u':
			opt->umem_flags |= XDP_UMEM_UNALIGNED_CHUNK_FLAG;
			opt->unaligned_chunks = 1;
			opt->mmap_flags = MAP_HUGETLB;
			break;
		// case 'F':
		// 	opt->xdp_flags &= ~XDP_FLAGS_UPDATE_IF_NOEXIST;
		// 	break;
		case 'f':
			opt->xsk_frame_size = atoi(optarg);
		case 'm':
			opt->need_wakeup = false;
			opt->xdp_bind_flags &= ~XDP_USE_NEED_WAKEUP;
			break;
		default:
			usage(basename(argv[0]));
		}
	}

	opt->ifindex = if_nametoindex(opt->iface);
	if (!opt->ifindex) {
		fprintf(stderr, "ERROR: interface \"%s\" does not exist\n", opt->iface);
		usage(basename(argv[0]));
	}

	if ((opt->xsk_frame_size & (opt->xsk_frame_size - 1)) && !opt->unaligned_chunks) {
		fprintf(stderr, "--frame-size=%d is not a power of two\n",
			opt->xsk_frame_size);
		usage(basename(argv[0]));
	}

	return opt;
}
