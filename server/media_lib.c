#include <stdio.h>
#include "media_lib.h"
#include <sys/types.h>
#include <dirent.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#define MEDIA_PATH  "/home/yueli/media/"
#define PATH_LEN    512
#define DESCR_LEN   128

struct chn_st {
    chnid_t chnid;
    char chn_dir_path[PATH_LEN];
    char *descr;
    int fd;                     // 是否打开mp3文件
    char mp3_list[100][PATH_LEN]; // 该频道所有mp3文件路径
    int mp3_count;              // mp3 文件数量
    int current_mp3_index;      // 当前正在播放的 mp3 文件索引
};

struct chn_st chan[MAX_CHN_NR];

static int count = 0;

int mlib_get_chn_list(struct mlib_st **mlib, int *nr) {
    DIR *media_dir, *chn_dir;
//    struct dirent *chn;
    struct dirent **chnlist, *file;
    int chn_num;
    char chn_path[PATH_LEN];
    int HaveDescr, HaveMp3;
    struct mlib_st *chn_info;
    char buf[DESCR_LEN];
    int descr_fd;
    char descr_path[PATH_LEN];

    //打开媒体库
    media_dir = opendir(MEDIA_PATH);
    if(media_dir == NULL) {
        perror("opendir()");
        return -1;
    }

    //读取媒体库
    chn_num = scandir(MEDIA_PATH, &chnlist, NULL, alphasort);
    if(chn_num == -1) {
        perror("scandir()");
        closedir(media_dir);
        return -1;
    }

    //打开频道
    for(int i = 0; i < chn_num; i++) {
        HaveDescr = 0;
        HaveMp3 = 0;

        if(chnlist[i]->d_name[0] == '.')
            continue;
        memset(chn_path, 0, PATH_LEN);
        sprintf(chn_path, "%s%s", MEDIA_PATH, chnlist[i]->d_name);

        //判断频道中是否包含descr.txt 和歌曲
        chn_dir = opendir(chn_path);
        while((file = readdir(chn_dir)) != NULL) {
            if(strstr(file->d_name, ".mp3"))
                HaveMp3 = 1;
            if(strstr(file->d_name, "descr.txt"))
                HaveDescr = 1;
            if(HaveDescr && HaveMp3)
                break;
        }
        closedir(chn_dir);

        //频道中包含descr.txt 和歌曲
        if(HaveDescr && HaveMp3) {
//            printf("%s\n", chn_path);
            
            chan[count].chnid = count +1;
            strcpy(chan[count].chn_dir_path, chn_path);
//            chn_info[count].chnid = count + 1;

            //打开descr.txt
            sprintf(descr_path, "%s/descr.txt", chn_path);
            descr_fd = open(descr_path, O_RDONLY);
            if(descr_fd == -1) {
                perror("open()");
                return -1;
            }

            //读取descr.txt
            memset(buf, 0, DESCR_LEN);
            ssize_t cnt = read(descr_fd, buf, DESCR_LEN);
            if(cnt == -1) {
                perror("read()");
                close(descr_fd);
                return -1;
            }
            close(descr_fd);
            
            chan[count].descr = strdup(buf);
//            chn_info[count].descr = strdup(buf);
            if(chan[count].descr == NULL) {
                perror("strdup()");
                return -1;
            }

            chn_dir = opendir(chn_path);
            chan[count].mp3_count = 0;
            chan[count].current_mp3_index = 0;
            chan[count].fd = -1;

            while ((file = readdir(chn_dir)) != NULL) {
                if (strstr(file->d_name, ".mp3")) {
                    snprintf(chan[count].mp3_list[chan[count].mp3_count],
                            PATH_LEN, "%s/%s", chn_path, file->d_name);
                    chan[count].mp3_count++;
                }
            }
            closedir(chn_dir);

            count++;
        }
        
        free(chnlist[i]);
    }
    free(chnlist);

    chn_info = malloc(sizeof(struct mlib_st) * count);
    if(chn_info == NULL) {
        perror("malloc()");
        return -1;
    }
    for(int i = 0; i < count; i++) {
        chn_info[i].chnid = chan[i].chnid;
        chn_info[i].descr = strdup(chan[i].descr);
    }
/*
    while((chn = readdir(media_dir)) != NULL) {
        if(chn->d_name[0] == '.')
            continue;

        memset(chn_path, 0, PATH_LEN);
        sprintf(chn_path, "%s%s", MEDIA_PATH, chn->d_name);
        printf("%s\n", chn_path);
    }

    if(errno != 0) {
        perror("readdir()");
        closedir(media_dir);
        return -1;
    }
*/
    *mlib = chn_info;
    *nr = count;

    closedir(media_dir);
    return 0;
}

ssize_t mlib_read_chn_data(chnid_t chnid, void *buf, size_t size) {
    chnid_t i;
    char mp3_path[PATH_LEN];
    ssize_t cnt;

    //查找指定频道
    for(i = 0; i < count; i++) {
        //找到了
        if(chan[i].chnid == chnid) {
            while(1) {
                //打开mp3文件
                if(chan[i].fd == -1) {
                    memset(mp3_path, 0, PATH_LEN);
                    strcpy(mp3_path, chan[i].mp3_list[chan[i].current_mp3_index]);
                    chan[i].fd = open(mp3_path, O_RDONLY);
                    if(chan[i].fd == -1) {
                        perror("open()");
                        return -1;
                    }
                }

                //读取mp3文件
                cnt = read(chan[i].fd, buf, size);
                //读完了
                if(cnt == 0) {
                    close(chan[i].fd);
                    chan[i].fd = -1;
                    chan[i].current_mp3_index = (chan[i].current_mp3_index + 1) % chan[i].mp3_count;
                }else if(cnt == -1) {
                    perror("read()");
                    close(chan[i].fd);
                    chan[i].fd = -1;
                    return -1;
                }else {
                    return cnt;
                }
            }
        }
    }

    return -1;
}

