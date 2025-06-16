#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <string.h>
#include "../proto/proto.h"
#include "media_lib.h"
#include "thrpool.h"
#include "tbf.h"

int udp_socket;
struct sockaddr_in raddr;

struct chn_list {
    int num;
    struct mlib_st *chan;
};

void *SendChnList(void *arg) {
    struct chn_list_st *chnlist;
    struct list_entry_st *entry;
    struct chn_list *list = (struct chn_list *)arg;
    int len, descr_len, chn_len;

    chnlist = malloc(MAX_MSG);
    if(chnlist == NULL) {
        perror("malloc()");
        return NULL;
    }

    chnlist->chnid  = CHN_LIST_ID;
    len = sizeof(chnid_t);

    entry = chnlist->list;  // 第一个 entry 的地址（相当于指针偏移）

    for(int i = 0; i < list->num; i++) {
        descr_len = strlen(list->chan[i].descr) + 1;
        chn_len = sizeof(struct list_entry_st) + descr_len -1; // 减1是因为 struct 里已有 char descr[1]
        
        if(len + chn_len > MAX_MSG)
            break;
        
        entry->chnid = list->chan[i].chnid;
        entry->len = chn_len; 
        strcpy(entry->descr, list->chan[i].descr);

        entry = (struct list_entry_st *)((char *)entry + chn_len);
        len += chn_len;
    }

    while(1) {
        if(sendto(udp_socket, chnlist, len, 0, (struct sockaddr *)&raddr, sizeof(raddr)) == -1) {
            perror("sendto()");
            break;
        }else {
//            printf("发送了%d字节\n", len);
        }
        
        sleep(1);
    }

    free(chnlist);
    return NULL;
    /*
    for(int i = 0; i < list->num; i++) {
        chn_len = sizeof(chnid_t) + sizeof(len_t) + sizeof((list->chan)[i].descr) + 1;
        entry[i] = malloc(chn_len);
        entry[i]->chnid = (list->chan)[i].chnid;
        strcpy(entry[i]->descr, (list->chan)[i].descr);
        entry[i]->len = chn_len;
    }

    memcpy(chnlist, entry, sizeof(entry));
*/
}

void *SendChnData(void *arg) {
    struct chn_data_st *chn_data;
    ssize_t cnt;

    chn_data = malloc(MAX_MSG);
    if(chn_data == NULL) {
        perror("malloc()");
        return NULL;
    }

    chn_data->chnid = *(chnid_t *)arg;


    int tbf_id = tbf_init(64*1024, 128*1024);
    if(tbf_id == -1) {
        perror("tbf_init");
        exit(1);
    }

    while(1) {

        tbf_fetch_token(tbf_id, MAX_MSG);

        cnt = mlib_read_chn_data(chn_data->chnid, chn_data->data, MAX_MSG - sizeof(chnid_t));
        if(cnt == -1) {
            fprintf(stderr, "发送频道%d错误\n", chn_data->chnid);
            return NULL;
        }

        if(sendto(udp_socket, chn_data, cnt+sizeof(chnid_t), 0, (struct sockaddr *)&raddr, sizeof(raddr)) == -1) {
            perror("sendto()");
            return NULL;
        }
        printf("发送了%ld字节到频道%d\n", cnt+sizeof(chnid_t), chn_data->chnid);
    }


    tbf_destroy(tbf_id);


    free(chn_data);
    return NULL;
}


int main() {
    struct ip_mreqn imr;
    struct chn_list *mlib;
    thr_pool_t *mypool;
    task_t send_chnlist;
    task_t send_chndata[MAX_CHN_NR];
    chnid_t chnid_array[MAX_CHN_NR];

    //开启组播功能
    udp_socket = socket(AF_INET, SOCK_DGRAM, 0);
    if(udp_socket == -1) {
        perror("socket()");
        return -1;
    }

    inet_aton(GROUP_ADDR, &imr.imr_multiaddr);
    inet_aton("0.0.0.0", &imr.imr_address);
    imr.imr_ifindex = if_nametoindex("ens33");
    if(setsockopt(udp_socket, IPPROTO_IP, IP_MULTICAST_IF, &imr, sizeof(imr)) == -1) {
        perror("setsockopt()");
        return -1;
    }

    raddr.sin_family = AF_INET;
    inet_aton(GROUP_ADDR, &raddr.sin_addr);
    raddr.sin_port = htons(RCV_PORT);

    mlib = malloc(sizeof(struct chn_list));
    if(mlib == NULL) {
        perror("malloc()");
        return -1;
    }
    //获取频道列表
    if(mlib_get_chn_list(&mlib->chan, &mlib->num) == -1) {
        fprintf(stderr, "获取频道列表失败\n");
        return -1;
    }

    // for(int i = 0; i < mlib.num; i++) {
    //     printf("%d: %s\n", (mlib.chan[i]).chnid, (mlib.chan[i]).descr);
    // }
    
    //初始化线程池
    thr_pool_init(&mypool, MAX_CHN_NR + 1, 5, MAX_CHN_NR + 1);

    //添加发送频道列表任务
    send_chnlist.task_p = SendChnList;
    send_chnlist.arg = mlib;
    thr_add_task(mypool, &send_chnlist);

    //添加发送频道数据的任务
    for(int i = 0; i < mlib->num; i++) {
        chnid_array[i] = mlib->chan[i].chnid;
        send_chndata[i].task_p = SendChnData;
        send_chndata[i].arg = &chnid_array[i];
        thr_add_task(mypool, &send_chndata[i]);
    }

    while(1)
        pause();

    close(udp_socket);
    return 0;
}