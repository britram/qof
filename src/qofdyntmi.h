#ifndef _QOF_DYNTMI_H_
#define _QOF_DYNTMI_H_

#include <yaf/autoinc.h>

void qfDynTmiOpen(const char *filename);

void qfDynTmiWrite(uint64_t fid, gboolean rev, uint32_t seq,
                   uint32_t fan, uint32_t iat,
                   uint32_t rttm, uint32_t rttc);

void qfDynTmiClose();

#endif