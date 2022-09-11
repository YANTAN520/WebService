#include "../lib.h"

void *test(void* arg)
{
    sleep(0.1);
    return NULL;
}
threadpool_t::threadpool_t(int min_thr_num, int max_thr_num, int queue_max_size)
{
    //初始化线程池
    this->min_thr_num = min_thr_num;
    this->max_thr_num = max_thr_num;
    this->live_thr_num = min_thr_num;
    this->busy_thr_num = 0;
    this->wait_exit_thr_num = 0;

    this->queue_front = 0;
    this->queue_rear = 0;
    this->queue_size = 0;
    this->queue_max_size = queue_max_size;
    this->shutdown = false;                 //不关闭线程池

    if(pthread_mutex_init(&(this->lock), NULL) != 0 ||
        pthread_mutex_init(&(this->thread_counter), NULL) != 0 ||
        pthread_cond_init(&(this->queue_not_full), NULL) != 0 ||
        pthread_cond_init(&(this->queue_not_empty), NULL) != 0)
    {
        perror("lock or singal init error");
    }

    threads = (pthread_t*)malloc(sizeof(pthread_t) * max_thr_num);
    task_queue = (threadpool_task_t*)malloc(sizeof(threadpool_task_t) * queue_max_size);
#ifdef DEBUGER
    printf("线程池参数初始化成功\n");
#endif

}
//创建线程池
 int threadpool_t::threadpool_create(void)
 {
    if(threads == NULL || task_queue == NULL)
    {
        perror("malloc error");
    }
    memset(threads, 0, sizeof(pthread_t)*max_thr_num);
    pthread_mutex_lock(&lock);
    for(int i=0; i<min_thr_num; ++i)
    {
        pthread_create(threads+i, NULL, threadpool_thread, this);
    }
    pthread_create(&adjust_tid, NULL, adjust_thread, this);
#ifdef DEBUGER
    printf("线程池创建成功\n");
#endif
    pthread_mutex_unlock(&lock);
 }
//像线程池添加一个工作任务

int threadpool_t::threadpool_add(void*(*function)(void *arg), void *arg)
{
    pthread_mutex_lock(&lock);
    while(queue_size == queue_max_size && !shutdown)
    {
        pthread_cond_wait(&queue_not_full, &lock);
    }

    if(shutdown)
    {
        //不能放入任务了
        pthread_cond_broadcast(&queue_not_empty);
        pthread_mutex_unlock(&lock);
        return 0;
    }
    //将任务放入任务队列
    task_queue[queue_rear].arg = NULL;
    task_queue[queue_rear].function = function;
    task_queue[queue_rear].arg;
    queue_rear = (queue_rear + 1) % queue_max_size;
    queue_size++;
    pthread_cond_signal(&queue_not_empty);
    pthread_mutex_unlock(&lock);
    return 0;
}
//工作线程

void* threadpool_thread(void* arg) 
{
    //任务队列中有任务，就在工作线程中运行任务，其他时候通过
    //循环阻塞和查询
    // threadpool_t *pool = (threadpool_t*) threadpool;
    threadpool_t* pool = (threadpool_t*)arg;
    threadpool_task_t task;

    while(true)
    {
        pthread_mutex_lock(&(pool->lock));
        //没有任务且没有shutdowm，阻塞
        while((pool->queue_size == 0) && (!(pool->shutdown)))
        {
            pthread_cond_wait(&(pool->queue_not_empty), &(pool->lock));

            if(pool->wait_exit_thr_num > 0)
            {
                pool->wait_exit_thr_num--;
            
            //如果存活线程大于最小线程，此线程可以销毁
                if(pool->live_thr_num > pool->min_thr_num)
                {
                    pool->live_thr_num--;
                    pthread_mutex_unlock(&(pool->lock));
                    pthread_exit(NULL);
                }
            }
        }

        if(pool->shutdown)
        {
            //立刻结束此线程
            pthread_mutex_unlock(&(pool->lock));
            pthread_detach(pthread_self());
            pthread_exit(NULL);
        }

        //此时表示有未执行的任务，开始执行任务
        //任务从队列取出
        task.function = pool->task_queue[pool->queue_front].function;
        task.arg = pool->task_queue[pool->queue_front].arg;

        pool->queue_front = (pool->queue_front + 1) % (pool->queue_max_size);
        pool->queue_size--;
        pthread_cond_broadcast(&(pool->queue_not_full));

        pthread_mutex_unlock(&(pool->lock));
        //取出任务后，开始执行任务并更新线程池
        pthread_mutex_lock(&(pool->thread_counter));
        pool->busy_thr_num++;
        pthread_mutex_unlock(&(pool->thread_counter));

        (*(task.function))((void *)task.arg);

        //任务运行结束
        pthread_mutex_lock(&(pool->thread_counter));
        pool->busy_thr_num--;
        pthread_mutex_unlock(&(pool->thread_counter));
    }
    pthread_exit(NULL);
}

//管理线程
void* adjust_thread(void* arg)
{
    //发生shutdown时任务结束
    int i = 0;
    threadpool_t* pool = (threadpool_t*)arg;
    while(!(pool->shutdown)){
        sleep(DEFAULT_TIME);
        //获取此时队列信息
        pthread_mutex_lock(&(pool->lock));
        int _live_thr_num = pool->live_thr_num;
        int _queue_size = pool->queue_size;
        pthread_mutex_unlock(&(pool->lock));

        pthread_mutex_lock(&(pool->thread_counter));
        int _busy_thr_num = pool->busy_thr_num;
        pthread_mutex_unlock(&(pool->thread_counter));

        //当任务数大于线程数，创建新线程
        if(_queue_size >= MIN_WAIT_TASK_NUM && _live_thr_num < (pool->max_thr_num))
        {
            pthread_mutex_lock(&(pool->lock));
            int add = 0;

            for(i = 0; i < (pool->max_thr_num) && add < DEFAULT_THREAD_VARY
                    && pool->live_thr_num < pool->max_thr_num; ++i)
                    {
                        if((pool->threads[i]) == 0 || !is_thread_alive(pool->threads[i]))
                        {
                            pthread_create((pool->threads)+i, NULL, threadpool_thread, (void *)pool);
                            add++;
                            (pool->live_thr_num)++;
                        }
                    }
            pthread_mutex_unlock(&(pool->lock));
        }

        //销毁空闲线程，算法：忙碌线程x2小于存活的线程，且存活线程大于最小线程数

        if(_live_thr_num > 2*_busy_thr_num && _live_thr_num > (pool->min_thr_num))
        {
            pthread_mutex_lock(&(pool->lock));
            pool->wait_exit_thr_num = DEFAULT_THREAD_VARY;
            pthread_mutex_unlock(&(pool->lock));

            for(i = 0; i<DEFAULT_THREAD_VARY; ++i)
            {
                pthread_cond_signal(&(pool->queue_not_full));
            }

        }
    }
    return NULL;
}

int threadpool_t::threadpool_destroy(void)
{
    int i;
    shutdown = true;

    pthread_join(adjust_tid, NULL);
    for(i = 0; i < live_thr_num; ++i)
    {
        pthread_cond_broadcast(&queue_not_empty);//启动全部阻塞线程
    }
    for(i = 0; i<live_thr_num; ++i)
    {
        pthread_join(threads[i], NULL);
    }
    return 0;
}

int threadpool_t::threadpool_free(void)
{
    if(task_queue) free(task_queue);
    if(threads) free(threads);
    pthread_mutex_lock(&lock);
    pthread_mutex_destroy(&lock);
    pthread_mutex_lock(&thread_counter);
    pthread_mutex_destroy(&thread_counter);
    pthread_cond_destroy(&queue_not_full);
    pthread_cond_destroy(&queue_not_empty);

    return 1;
}

int threadpool_t::threadpool_all_threadnum()
{
    pthread_mutex_lock(&lock);
    int ret = live_thr_num;
    pthread_mutex_unlock(&lock);
    return ret;
}

int threadpool_t::threadpool_busy_threadnum()
{
    pthread_mutex_unlock(&thread_counter);
    int ret = busy_thr_num;
    pthread_mutex_unlock(&thread_counter);
    return ret;
}

int is_thread_alive(pthread_t tid)
{
    int kill_rc = pthread_kill(tid, 0);
    if(kill_rc == ESRCH)
        return false;
    return true;
}

//析构函数，释放内存
threadpool_t::~threadpool_t(void)
{
    // threadpool_free();
}

