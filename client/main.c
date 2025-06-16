#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <arpa/inet.h>
#include <net/if.h>
#include "../proto/proto.h"
#include <string.h>

#define BUFSIZE 2048

int main() {
    struct sockaddr_in laddr;
    struct ip_mreqn imr;
    union msg_st *buf;
    struct chn_list_st *chan_list;
    ssize_t msg_len;
    struct list_entry_st *chan;
    struct chn_data_st *chn_data;

    int id;

    int pfd[2] = {};
    pid_t pid;
    int fd;

    if(pipe(pfd) == -1) {
        perror("pipe()");
        return 1;
    }

    pid = fork();
    if(pid == -1){
        perror("fork()");
        return -1;
    }

    if(pid == 0){
        close(pfd[1]);
        dup2(pfd[0], 0);
        //execl("/usr/bin/mplayer", "mplayer", "-", NULL);
        execl("/usr/bin/mplayer", "mplayer", "-cache", "8192", "-cache-min", "5", "-", NULL);
        //execl("/usr/bin/mplayer", "mplayer", "-cache", "8192", "-cache-min", "0.5", "-", NULL);
        perror("execl()");
        exit(1);   
    }
     
    close(pfd[0]);

    int udp_socket = socket(AF_INET, SOCK_DGRAM, 0);
    if(udp_socket == -1){
        perror("socket()");
        return -1;
    }

    //允许一个地址绑定多个客户端
    int val = 1;
    if (setsockopt(udp_socket, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val)) == -1) {
        perror("setsockopt(SO_REUSEADDR)");
        exit(1);
    }

    inet_aton(GROUP_ADDR, &imr.imr_multiaddr);
    inet_aton("0.0.0.0", &imr.imr_address);
    imr.imr_ifindex = if_nametoindex("ens33");

    if ((imr.imr_ifindex = if_nametoindex("ens33")) == 0) {
        perror("if_nametoindex()");
        exit(1);
    }

    if(setsockopt(udp_socket, IPPROTO_IP, IP_ADD_MEMBERSHIP, &imr, sizeof(imr)) == -1){
        perror("setsockopt()");
        close(udp_socket);
        return -1;
    }

    inet_aton("0.0.0.0", &laddr.sin_addr);
    laddr.sin_port = htons(RCV_PORT); 
    laddr.sin_family = AF_INET;
    if(bind(udp_socket, (void *)&laddr, sizeof(laddr)) == -1){
        perror("bind()");
        close(udp_socket);
        return -1;
    }

    buf = malloc(BUFSIZE);
    if(buf == NULL) {
        perror("malloc()");
        return -1;
    }

    while(1){
        msg_len = recvfrom(udp_socket, buf, BUFSIZE, 0, NULL, NULL);
        if(msg_len == -1) {
            perror("recvfrom()");
            return -1;
        }
        
        if(buf->chnid == 0) 
            break;
    }
        
    printf("接收到频道列表:\n");

    chan_list = (struct chn_list_st *)buf;
    int offset = sizeof(chnid_t);
    while (offset < msg_len) {
        chan = (struct list_entry_st *)((char *)chan_list + offset);

        printf("[%d] : %s\n", chan->chnid, chan->descr);
        offset += chan->len;
    }



    // chan_list = (struct chn_list_st *)buf;
    // chan = (struct list_entry_st *)((char *)chan_list + sizeof(chnid_t));
    // while((char *)chan - (char *)chan_list < msg_len) {
    //     printf("[%d] : %s\n", chan->chnid, chan->descr);
    //     chan = (struct list_entry_st *)((char *)chan + chan->len);   
    // }

    printf("请选择频道: ");
    scanf("%d", &id);
       
    
    while (1) {
        msg_len = recvfrom(udp_socket, buf, BUFSIZE, 0, NULL, NULL);
        if (msg_len < (ssize_t)sizeof(chnid_t))
            continue;

        if (buf->chnid == id) {
            ssize_t wlen = write(pfd[1], (char *)buf + sizeof(chnid_t), msg_len - sizeof(chnid_t));
            //printf("写入管道数据长度：%zd\n", wlen);
            printf("写入 %zd 字节到管道\n", wlen);
            //usleep(20000);  // 每 20ms 发送一包，与 server 同步
        
            // // DEBUG: dump 前几个字节
            // for (int i = 0; i < 8 && i < msg_len - sizeof(chnid_t); ++i)
            //     printf("%02X ", ((unsigned char*)buf + sizeof(chnid_t))[i]);
            // printf("\n");
        }
    }

    close(udp_socket);
    return 0;
}