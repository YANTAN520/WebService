/*
**实现一个存储超时的的双向链表，链表元素包括
**某一个http连接的socket信息和http指针。还有时间设置
**因为64位时间，超时需要193年左右，所以不用考虑超时
*/

#ifndef __TIMER_H__
#define __TIMER_H__

#include "../sys.h"


/*判断是否已经插入*/
#define IS_INSERT_LIST(elem) ((elem->pre != nullptr) && (elem->next != nullptr))

#define list_entry(type, address, elem) \
                (type*)((char *)(address) - (size_t)(&((type *)0)->elem))


struct TimeListElem
{
    uint64_t target_time; //超时时间
    struct TimeListElem* next;
    struct TimeListElem* pre;

    TimeListElem(){
        target_time = 0;
        next = nullptr;
        pre = nullptr;
    }
    TimeListElem(uint64_t value)
    {
        target_time = value;
        next = nullptr;
        pre = nullptr;
    }
};


struct TimeList
{
    TimeListElem head; /*不能定义为指针*/
    TimeListElem tail;

    TimeList(){
        head.next = &tail;
        tail.pre = &head;
    }
};

class Timer
{
private:
    struct TimeList list;
    /*定时器超时信号*/
    pthread_cond_t time_out_cond;
    /*定时时间*/
    uint64_t step;
    
/*
**进行插入和删除的操作，插入全部选择头插法，因为后插入的时间一定
**大于前面插入的
*/
public:
    struct TimeListElem* get_head(){ return &(list.head); };
    struct TimeListElem* get_tail(){ return &(list.tail); };
    pthread_cond_t* get_time_out_cond(){ return &time_out_cond;}
    void insert_list_fast(TimeListElem* elem);
    void insert_list_slow(TimeListElem* elem);
    void delete_list(TimeListElem* elem);
    void time_out_deal();
    void insert_list_normal(TimeListElem* elem);
    void set_step(uint64_t value){ step = value; }
    uint64_t get_step(){ return step; }
    Timer()
    {
        pthread_cond_init(&time_out_cond, NULL);
        step = 5000;
    }

    ~Timer()
    {
        pthread_cond_destroy(&time_out_cond);
    }
};

extern Timer timer; /*维护整个定时器*/

uint64_t GetTickCount();

#endif