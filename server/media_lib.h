#ifndef __MEDIA_LIB_H__
#define __MEDIA_LIB_H__

#include "../proto/proto.h"

struct mlib_st {
    chnid_t chnid;
    char *descr;
};

int mlib_get_chn_list(struct mlib_st **mlib, int *nr);

ssize_t mlib_read_chn_data(chnid_t chnid, void *buf, size_t size);

#endif