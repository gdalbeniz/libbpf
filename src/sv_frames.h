/* Copyright (C) Oier Garcia de Albeniz Lopez - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 * Written by Oier Garcia de Albeniz Lopez <g.albeniz@gmail.com>, September 2020
 */
#ifndef __SV_FRAMES__
#define __SV_FRAMES__

#include <stdint.h>
struct sSvOpt;
struct sSvXdpSkt;




int32_t sv_frame_prepare(struct sSvOpt *opt, void *skt_info, uint32_t sv);
void sv_frame_smp_upd(uint8_t *frame, uint16_t smp);


#endif