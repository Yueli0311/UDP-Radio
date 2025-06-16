#ifndef __TBF_H__
#define __TBF_H__

#define TBF_MAX	1024

// 定义类型
typedef struct tbf_st {
	int token; // 至少有两个线程访问,第一个管理令牌桶库的线程没秒累加，另一个使用令牌桶
	pthread_mutex_t mutex;
	pthread_cond_t cond;
	int cps;
	int burst;
}tbf_t;

// 初始化令牌桶
/*
*mytbf:开辟的令牌桶结构的地址
cps:令牌桶的速率
burst:令牌桶的上限
 */
int tbf_init(int cps, int burst);

/*
 	取令牌
 */
int tbf_fetch_token(int tbf_pos, int ntokens);

/*
 	销毁
 */
void tbf_destroy(int tbf_pos);


#endif

