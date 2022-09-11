#ifndef __THREADPOOL_H__
#define __THREADPOOL_H__
#include "../sys.h"


#define DEFAULT_TIME 10                 /*10s检测一次*/
#define MIN_WAIT_TASK_NUM 10            /*如果queue_size > MIN_WAIT_TASK_NUM 添加新的线程到线程池*/ 
#define DEFAULT_THREAD_VARY 10          /*每次创建和销毁线程的个数*/
#define true 1
#define false 0

typedef struct{
    void *(*function)(void *);  //回调函数参数
    void* arg;                  //函数参数
}threadpool_task_t;

class threadpool_t{
public:
    threadpool_t(int min_thr_num, int max_thr_num, int queue_max_size);
    int threadpool_add(void*(*function)(void *arg), void *arg);
    int threadpool_create(void);
    int threadpool_destroy(void);
    int threadpool_free(void);
    int threadpool_all_threadnum();
    int threadpool_busy_threadnum();
    friend void* threadpool_thread(void* args);
    friend void* adjust_thread(void* arg);
    friend int is_thread_alive(pthread_t tid);

    ~threadpool_t();
private:
    pthread_mutex_t lock;               /* 用于锁住本结构体 */    
    pthread_mutex_t thread_counter;     /* 记录忙状态线程个数de琐 -- busy_thr_num */
 
    pthread_cond_t queue_not_full;      /* 当任务队列满时，添加任务的线程阻塞，等待此条件变量 */
    pthread_cond_t queue_not_empty;     /* 任务队列里不为空时，通知等待任务的线程 */
 
    pthread_t *threads;                 /* 存放线程池中每个线程的tid。数组 */
    pthread_t adjust_tid;               /* 存管理线程tid */
    threadpool_task_t *task_queue;      /* 任务队列(数组首地址) */
 
    int min_thr_num;                    /* 线程池最小线程数 */
    int max_thr_num;                    /* 线程池最大线程数 */
    int live_thr_num;                   /* 当前存活线程个数 */
    int busy_thr_num;                   /* 忙状态线程个数 */
    int wait_exit_thr_num;              /* 要销毁的线程个数 */
 
    int queue_front;                    /* task_queue队头下标 */
    int queue_rear;                     /* task_queue队尾下标 */
    int queue_size;                     /* task_queue队中实际任务数 */
    int queue_max_size;                 /* task_queue队列可容纳任务数上限 */
 
    int shutdown;                       /* 标志位，线程池使用状态，true或false */
};

void* threadpool_thread(void* args);
void* adjust_thread(void* arg);
int is_thread_alive(pthread_t tid);
#endif