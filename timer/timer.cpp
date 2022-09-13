

#include "timer.h"
/*
**每10s发生一次超时时间，并且超时后释放超时信号量，触发线程池
**里面任务
*/
// #include "./http/myhttp.h"
// #include "../lib.h"


 

Timer timer;


uint64_t GetTickCount()
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);

    return (ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
}

void Timer::insert_list_fast(TimeListElem* elem)
{
    /*直接头插法*/
    TimeListElem* cur = get_head();
    elem->next = cur->next;
    elem->next->pre = elem;
    elem->pre = cur;
    cur->next = elem;
    return;
}

void Timer::insert_list_slow(TimeListElem* elem)
{
    TimeListElem* cur = get_head()->next;
    while(cur != (get_tail()))
    {
        /*超时时间大的放在头部*/
        if(cur->target_time > elem->target_time)
        {
            cur = cur->next;
            continue;
        }
        break;
    }
    /*此时elem插在cur前面*/
    cur->pre->next = elem;
    elem->pre = cur->pre;
    elem->next = cur;
    cur->pre = elem;
}

void Timer::delete_list(TimeListElem* elem)
{
    /*判断元素是否存在链表中*/
    if(elem->pre && elem->next)
    {
        elem->pre->next->next = elem->next;
        elem->next->pre = elem->pre;
    }
    return;
}


void Timer::insert_list_normal(TimeListElem* elem)
{
    bool res = IS_INSERT_LIST(elem);
    if(res)
    {
        delete_list(elem);
    }
    elem->target_time = GetTickCount() + step;
    insert_list_fast(elem);
    return;
}




// int main()
// {
//     Timer timer;
//     TimeListElem* cur, *buf;
//     for(int i=0; i<10; ++i)
//     {
//         cur = new TimeListElem(GetTickCount());
//         // cur->target_time = i;
//         timer.insert_list_fast(cur);
//     }
//     cur = timer.get_head()->next;
//     while(cur != (timer.get_tail()))
//     {
//         printf("cur->target_time: %d\t\n", (int)(cur->target_time));
//         buf = cur;
//         cur = cur->next;
//         timer.delete_list(buf);
//         delete buf;
//     }

//     return 1;
// }