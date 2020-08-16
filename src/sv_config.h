#ifndef __SV_CONFIG__
#define __SV_CONFIG__


#include "sv_injector.h"

#define MAXLEN 128
#define SAMPLEWRAP 4000
#define PACKETSIZE 256

#define SV_ALLOC_SZ 50
#define SV_MAX 256

struct sSvConf {
    char section[MAXLEN];
    uint8_t mac[6];
    uint8_t vlanPrio;
    uint16_t vlanId;
    uint16_t appId;
    char svId[MAXLEN];
    char datSet[MAXLEN];
    uint32_t confRev;
    double ia_ang;
    double ia_mag;
    uint16_t ia_q;
    double ib_mag;
    double ib_ang;
    uint16_t ib_q;
    double ic_mag;
    double ic_ang;
    uint16_t ic_q;
    double in_mag;
    double in_ang;
    uint16_t in_q;
    double va_mag;
    double va_ang;
    uint16_t va_q;
    double vb_mag;
    double vb_ang;
    uint16_t vb_q;
    double vc_mag;
    double vc_ang;
    uint16_t vc_q;
    double vn_mag;
    double vn_ang;
    uint16_t vn_q;
};

struct sSvOpt {
    int test;
    uint32_t xdp_flags;// = XDP_FLAGS_UPDATE_IF_NOEXIST;
    const char *iface;// = null;
    int ifindex;
    char mode;// = 'P';
    int poll;
    int interval;// = 1;
    uint32_t xdp_bind_flags;// = XDP_USE_NEED_WAKEUP;
    uint32_t umem_flags;
    int unaligned_chunks;
    int mmap_flags;
    int xsk_frame_size;// = XSK_UMEM__DEFAULT_FRAME_SIZE;
    int timeout;// = 1000;
    bool need_wakeup;// = true;
    bool debug;// = true;
    const char *cfg_file;// = null;
    uint32_t sv_limit;

    struct sSvConf def_conf;
    struct sSvConf *sv_conf;
    uint32_t sv_num;
    uint32_t sv_alloc;
};
extern struct sSvOpt sv_opt;

struct sSvSocket {
	int32_t socket;
	struct {
		struct sockaddr_ll address[SV_MAX];
		struct mmsghdr msgvec[SV_MAX];
		struct iovec iov[SV_MAX];
	} samp[SAMPLEWRAP];
};
extern struct sSvSocket sv_socket;


void parse_command_line(int argc, char **argv, struct sSvOpt *opt);
void parse_cfg_file(struct sSvOpt *opt);
void usage(const char *prog);

void printSvOpt(struct sSvOpt *opt);

#endif