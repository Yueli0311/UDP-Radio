#include "thrpool.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <string.h>

static void *admin_thr_job(void *arg);
static void *works_threads(void *arg);

//管理者线程
static void *admin_thr_job(void *arg) {
    /*
        管理者线程
        while(!shutdown) {
            //工作线程有状态变化的时候进行增减线程
            pthread_mutex_lock(&busy_mut);
            alive_cnt = busy_thr_cnt + free_thr_cnt
            while(busy_thr_cnt < alive_cnt && free_thr_cnt <= min_free_threads) {
                //不用调整线程池
                pthread_cond_wait(&busy_cond, &busy_mut);
            }
            //增 busy_thr_cnt == alive_cnt 
            //减 free_thr_cnt > min_free_threads
        }
    */
    thr_pool_t *pool = arg;
    while (!pool->shutdown) {
        pthread_mutex_lock(&pool->busy_mutex);
        int alive_cnt = pool->free_thr_cnt + pool->busy_thr_cnt;

        // 等待线程状态变化或资源变化
        while (!pool->shutdown && (pool->busy_thr_cnt < alive_cnt && pool->free_thr_cnt <= pool->min_free_threads)) {
            pthread_cond_wait(&pool->busy_cond, &pool->busy_mutex);
        }

        if (pool->shutdown) {
            pthread_mutex_unlock(&pool->busy_mutex);
            break;
        }

        // 增加线程：当前线程全部忙碌且未达到最大线程数
        if (pool->busy_thr_cnt == alive_cnt && alive_cnt < pool->max_threads) {
            for (int i = 0; i < pool->max_threads; i++) {
                if (pool->job_thrs[i].tid == 0) {
                    pthread_create(&pool->job_thrs[i].tid, NULL, works_threads, pool);
                    pool->job_thrs[i].state = THR_FREE;
                    pool->free_thr_cnt++;
                    break;
                }
            }
        }

        // 减少线程：空闲线程超过最小空闲数，设置 exit_thrs 请求
        if (pool->free_thr_cnt > pool->min_free_threads) {
            int reduce_cnt = pool->free_thr_cnt - pool->min_free_threads;
            pool->exit_thrs += reduce_cnt;
            while (reduce_cnt--) {
                pthread_cond_signal(&pool->cond_not_empty);  // 唤醒线程让其自退出
            }
        }

        pthread_mutex_unlock(&pool->busy_mutex);
        sleep(1);
    }
    return NULL;
}

//任务线程
static void *works_threads(void *arg) {
    thr_pool_t *pool = arg;
    task_t task;
    while(1) {
        //看看池是否关闭

        //看看exit_thrs > 0 优先中止

        //判断队列是否为空
        //空: 解锁等待 queue_not_emoty:接收到通知:判断池是否关闭，判断exit_thrs是否大于0
        
        //不空:取任务  queue_deq();
        //取到任务 --->解锁 --->调用任务函数 
        pthread_mutex_lock(&pool->queue_mut);

        // 检查是否需要退出线程
        if (pool->exit_thrs > 0) {
            pool->exit_thrs--;
            pool->free_thr_cnt--;

            // 状态设置为FREE
            for (int i = 0; i < pool->max_threads; i++) {
                if (pthread_equal(pool->job_thrs[i].tid, pthread_self())) {
                    pool->job_thrs[i].tid = 0;
                    pool->job_thrs[i].state = THR_FREE;
                    break;
                }
            }
            pthread_mutex_unlock(&pool->queue_mut);
            break;
        }

        // 等待任务到来或池关闭
        while (queue_empty(pool->task_queue) && !pool->shutdown) {
            pthread_cond_wait(&pool->cond_not_empty, &pool->queue_mut);
            // 再次检查退出标志
            if (pool->exit_thrs > 0) {
                pool->exit_thrs--;
                pool->free_thr_cnt--;

                // 状态设置为FREE
                for (int i = 0; i < pool->max_threads; i++) {
                    if (pthread_equal(pool->job_thrs[i].tid, pthread_self())) {
                        pool->job_thrs[i].tid = 0;
                        pool->job_thrs[i].state = THR_FREE;
                        break;
                    }
                }
                pthread_mutex_unlock(&pool->queue_mut);
                return NULL;
            }
        }

        // 池关闭时退出
        if (pool->shutdown) {
            pthread_mutex_unlock(&pool->queue_mut);
            break;
        }

        // 取出任务执行
        queue_deq(pool->task_queue, &task);
        pthread_cond_signal(&pool->cond_not_full);  // 通知任务队列不满
        pool->free_thr_cnt--;
        pool->busy_thr_cnt++;
    
        //状态设置为BUSY
        for (int i = 0; i < pool->max_threads; i++) {
            if (pthread_equal(pool->job_thrs[i].tid, pthread_self())) {
                pool->job_thrs[i].state = THR_BUSY;
                break;
            }
        }

        pthread_mutex_unlock(&pool->queue_mut);

        // 执行任务
        task.task_p(task.arg);

        // 任务执行完成，更新线程状态
        pthread_mutex_lock(&pool->queue_mut);
        pool->busy_thr_cnt--;
        pool->free_thr_cnt++;
        pthread_cond_signal(&pool->busy_cond);

        //状态设置为FREE
        for (int i = 0; i < pool->max_threads; i++) {
            if (pthread_equal(pool->job_thrs[i].tid, pthread_self())) {
                pool->job_thrs[i].state = THR_FREE;
                break;
            }
        }
        pthread_mutex_unlock(&pool->queue_mut);

    }
    return NULL;
}

int thr_pool_init(thr_pool_t **mypool, int max_threads, int min_free_threads, int queue_size) {
    /*
        初始化线程池
        构建工作线程
        构建管理者线程
    */
    *mypool = (thr_pool_t *)malloc(sizeof(thr_pool_t));
    if(*mypool == NULL) {
        perror("malloc()");
        return -1;
    }

    thr_pool_t *pool = *mypool;
   
    pool->max_threads = max_threads;
    pool->min_free_threads = min_free_threads;
    pool->free_thr_cnt = 0;
    pool->busy_thr_cnt = 0;
    pool->exit_thrs = 0;
    pool->shutdown = 0;

    // 初始化同步原语
    pthread_mutex_init(&pool->busy_mutex, NULL);
    pthread_cond_init(&pool->busy_cond, NULL);
    pthread_mutex_init(&pool->queue_mut, NULL);
    pthread_cond_init(&pool->cond_not_empty, NULL);
    pthread_cond_init(&pool->cond_not_full, NULL);

    // 分配工作线程数组
    pool->job_thrs = calloc(max_threads, sizeof(thr_job_t));
    if(!pool->job_thrs) 
        return -1;
    
    queue_init(&pool->task_queue, sizeof(task_t), queue_size);
   
    // 创建初始工作线程
    for (int i = 0; i < min_free_threads; i++) {
        pthread_create(&pool->job_thrs[i].tid, NULL, works_threads, pool);
        pool->job_thrs[i].state = THR_FREE;
        pool->free_thr_cnt++;
    }

    // 创建管理者线程
    pthread_create(&pool->admin_thr_id, NULL, admin_thr_job, pool);
   return 0;
}

int thr_add_task(thr_pool_t *mypool, const task_t *t) {
    pthread_mutex_lock(&mypool->queue_mut);
    while (queue_full(mypool->task_queue)) {
        pthread_cond_wait(&mypool->cond_not_full, &mypool->queue_mut);
    }

    queue_enq(mypool->task_queue, t);
    pthread_cond_signal(&mypool->cond_not_empty);
    pthread_mutex_unlock(&mypool->queue_mut);
    return 0;
}

void thr_pool_destroy(thr_pool_t **mypool) {
    //关闭池
    //发任务队列不为空的信号（cond_not_empty）
    //给所有线程收尸
    //释放开辟的存储空间 销毁队列
    //释放整个线程池结构
    thr_pool_t *pool = *mypool;
    pool->shutdown = 1;
    pthread_cond_broadcast(&pool->cond_not_empty);  // 唤醒所有线程准备退出

    // 等待所有工作线程结束
    for (int i = 0; i < pool->max_threads; i++) {
        if (pool->job_thrs[i].tid != 0) {
            pthread_join(pool->job_thrs[i].tid, NULL);
        }
    }

    // 等待管理者线程结束
    pthread_join(pool->admin_thr_id, NULL);

    // 清理资源
    queue_destroy(&pool->task_queue);
    free(pool->job_thrs);
    pthread_mutex_destroy(&pool->busy_mutex);
    pthread_mutex_destroy(&pool->queue_mut);
    pthread_cond_destroy(&pool->busy_cond);
    pthread_cond_destroy(&pool->cond_not_empty);
    pthread_cond_destroy(&pool->cond_not_full);

    free(pool);
    *mypool = NULL;
}