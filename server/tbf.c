#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <pthread.h>
#include "tbf.h"

static tbf_t *jobs[TBF_MAX];
static pthread_mutex_t mut_job = PTHREAD_MUTEX_INITIALIZER;
static int inited;

static int __get_free_pos(void)
{
	int i;

	for (i = 0; i < TBF_MAX; i++)
		if (!jobs[i])
			return i;
	return -1;
}

static void *thr_job(void *s)
{
	int i;

	while (1) {
		pthread_mutex_lock(&mut_job);
		for (i = 0; i < TBF_MAX; i++) {
			if (jobs[i]) {
				pthread_mutex_lock(&jobs[i]->mutex);
				jobs[i]->token += jobs[i]->cps;
				if (jobs[i]->token > jobs[i]->burst)
					jobs[i]->token = jobs[i]->burst;
				pthread_cond_broadcast(&jobs[i]->cond);
				pthread_mutex_unlock(&jobs[i]->mutex);
			}
		}
		pthread_mutex_unlock(&mut_job);
		sleep(1);
	}
}

static void moduler_init(void)
{
	pthread_t tid;
	int err;

	err = pthread_create(&tid, NULL, thr_job, NULL);
	if (err) {
		fprintf(stderr, "pthread_create():%s\n", strerror(err));
		exit(0);
	}
}

int tbf_init(int cps, int burst)
{
	static tbf_t *me;
	int pos;

	if (0 == inited) {
		moduler_init(); // 并发线程
		inited = 1;
	}

	me = malloc(sizeof(tbf_t));
	if (NULL == me)
		return -1;

	me->token = 0;
	me->cps = cps;
	me->burst = burst;
	pthread_mutex_init(&me->mutex, NULL);
	pthread_cond_init(&me->cond, NULL);

	pthread_mutex_lock(&mut_job);
	pos = __get_free_pos();
	if (-1 == pos) {
		free(me);
		me = NULL;
		return -1;
	}
	jobs[pos] = me;
	pthread_mutex_unlock(&mut_job);

	return pos;
}

int tbf_fetch_token(int tbf_pos, int ntokens)
{
	pthread_mutex_lock(&jobs[tbf_pos]->mutex);
	while (jobs[tbf_pos]->token < ntokens) {
		pthread_cond_wait(&jobs[tbf_pos]->cond, &jobs[tbf_pos]->mutex);
	}
	jobs[tbf_pos]->token -= ntokens;	
	pthread_mutex_unlock(&jobs[tbf_pos]->mutex);
	return 0;
}

void tbf_destroy(int tbf_pos)
{
	// mytbf--->&t 
	pthread_mutex_lock(&mut_job);
	pthread_mutex_destroy(&jobs[tbf_pos]->mutex);
	pthread_cond_destroy(&jobs[tbf_pos]->cond);
	free(jobs[tbf_pos]);
	jobs[tbf_pos] = NULL;
	pthread_mutex_unlock(&mut_job);
}


