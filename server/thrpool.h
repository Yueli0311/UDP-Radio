#ifndef __THR_POOL_H__
#define __THR_POOL_H__

#include <pthread.h>
#include "queue.h"

//工作线程
enum thr_state {THR_BUSY, THR_FREE};
typedef struct {
    pthread_t tid;
    enum thr_state state;
}thr_job_t;

//任务
typedef struct {
    void *(*task_p)(void *s);    //不限制任务函数的类型
    void *arg;  //任务函数的参数
}task_t;

typedef struct {
    //管理者线程
    pthread_t admin_thr_id;
    int free_thr_cnt;   //空闲线程数量
    int busy_thr_cnt;   //工作线程数量
    pthread_mutex_t busy_mutex;
    pthread_cond_t busy_cond;
    int exit_thrs;  //由管理者线程管理，工作线程如果发现exit_thrs>0,优先中止
    int shutdown;   //为1则池关闭

    //工作线程
    int max_threads;    //最大线程数量
    int min_free_threads;  //至少保证空闲线程个数
    thr_job_t *job_thrs;

    //任务队列
    queue_t *task_queue;    //队列存储的数据类型为task_t
    pthread_mutex_t queue_mut;  //队列锁
    pthread_cond_t cond_not_empty;  //①管理者线程发通知有两种情况：1.池关闭 2.需要中止一部分工作线程 ②放任务接口发送
    pthread_cond_t cond_not_full;

}thr_pool_t;

//初始化
int thr_pool_init(thr_pool_t **mypool, int max_threads, int min_free_threads, int queue_size);

//添加任务
int thr_add_task(thr_pool_t *mypool, const task_t *t);

//销毁
void thr_pool_destroy(thr_pool_t **mypool);

#endif