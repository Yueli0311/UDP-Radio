#ifndef __PROTO_H__
#define __PROTO_H__

#include <stdint.h>

#define GROUP_ADDR  "226.2.3.4"
#define RCV_PORT    5367
#define CHN_MIN_ID  1
#define MAX_CHN_NR  200
#define CHN_MAX_ID  ((CHN_MIN_ID) + (MAX_CHN_NR) - 1)
#define MAX_MSG     1024
#define CHN_LIST_ID 0

typedef uint8_t     chnid_t;
typedef uint16_t    len_t;

//单个频道
struct list_entry_st {
    chnid_t chnid;  /*CHN_MIN_ID ~ CHN_MAX_ID*/
    len_t len;      /*频道包长度*/
    char descr[1];  /*频道描述*/     
}__attribute__((packed));

//频道列表
struct chn_list_st {
    chnid_t chnid;  /*CHN_LIST_ID*/
    struct list_entry_st list[1];
}__attribute__((packed));

//频道数据
struct chn_data_st {
    chnid_t chnid;  /*CHN_MIN_ID ~ CHN_MAX_ID*/
    //char data[MAX_MSG];
    char data[1];
}__attribute__((packed));

//客户端接收到的数据
union msg_st {
    chnid_t chnid;
    struct chn_list_st chn_list;
    struct chn_data_st chn_data;
};



#endif
