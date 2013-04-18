#ifndef _QOF_DYNTMI_H_
#define _QOF_DYNTMI_H_

#include <yaf/autoinc.h>

void qfDynTmiOpen(const char *filename);

void qfDynTmiFlow(uint64_t ms, uint64_t fid, gboolean rev);

void qfDynTmiDynamics(uint32_t seq, uint32_t fan, uint32_t flight,
                      uint32_t iat, uint32_t rttm, uint32_t rttc,
                      uint32_t rtx, uint32_t reo);

void qfDynTmiClose();

#endif