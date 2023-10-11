#include "bootpack.h"
struct TIMERCTL timerctl;

void init_pit(void) 
{ 
    int i; 
    struct TIMER *t; 
    io_out8(PIT_CTRL, 0x34); 
    io_out8(PIT_CNT0, 0x9c); 
    io_out8(PIT_CNT0, 0x2e); 
    timerctl.count = 0; 
    for (i = 0; i < MAX_TIMER; i++) { 
        timerctl.timers0[i].flags = 0; /* 没有使用 */ 
    } 
    t = timer_alloc(); /* 取得一个 */ 
    t->timeout = 0xffffffff; 
    t->flags = TIMER_FLAGS_USING; 
    t->next = 0; /* 末尾 */ 
    timerctl.t0 = t; /* 因为现在只有哨兵，所以他就在最前面*/ 
    timerctl.next = 0xffffffff; /* 因为只有哨兵，所以下一个超时时刻就是哨兵的时刻 */ 
    timerctl.using = 1; 
    return; 
}

struct TIMER *timer_alloc(void) 
{ 
    int i; 
    for (i = 0; i < MAX_TIMER; i++) { 
        if (timerctl.timers0[i].flags == 0) { 
            timerctl.timers0[i].flags = TIMER_FLAGS_ALLOC; 
            return &timerctl.timers0[i]; 
        } 
    } 
    return 0; /* 没找到 */ 
}

void timer_free(struct TIMER *timer) 
{ 
    timer->flags = 0; /* 未使用 */ 
    return; 
} 
 
void timer_init(struct TIMER *timer, struct FIFO32 *fifo, int data) 
{ 
    timer->fifo = fifo; 
    timer->data = data; 
    return; 
} 
 
// void timer_settime(struct TIMER *timer, unsigned int timeout) 
// { 
//     int e, i, j;
//     timer->timeout = timeout + timerctl.count;

//     timer->flags = TIMER_FLAGS_USING; 

//     e = io_load_eflags(); 
//     io_cli();
//     for(i = 0; i < timerctl.using; i++){
//         if(timerctl.timers[i]->next->timeout >= timer->timeout){
//             break;
//         }
//     }
//     // for( j = timerctl.using; j > i; j--){
//     //     timerctl.timers[j] = timerctl.timers[j - 1];
//     // }
//     struct TIMER * tempTimer = timerctl.timers[i]->next;
//     timerctl.timers[i]->next = timer;
//     timer->next = tempTimer;

//     timerctl.using++;
//     // timerctl.timers[i] = timer;
//     timerctl.next = timerctl.timers[0]->timeout;
//     io_store_eflags(e);
//     return; 
// } 

void timer_settime(struct TIMER *timer, unsigned int timeout) 
{ 
    int e; 
    struct TIMER *t, *s; 
    timer->timeout = timeout + timerctl.count; 
    timer->flags = TIMER_FLAGS_USING; 
    e = io_load_eflags(); 
    io_cli(); 
    timerctl.using++; 
    t = timerctl.t0; 
    if (timer->timeout <= t->timeout) { 
        /* 插入最前面的情况 */ 
        timerctl.t0 = timer; 
        timer->next = t; /* 下面是设定t  */ 
        timerctl.next = timer->timeout; 
        io_store_eflags(e); 
        return; 
    } 
    /* 搜寻插入位置 */ 
    for (;;) { 
        s = t; 
        t = t->next; 
        if (timer->timeout <= t->timeout) { 
            /* 插入s和t之间的情况 */ 
            s->next = timer; /* s下一个是timer */ 
 
            timer->next = t; /* timer的下一个是t */ 
            io_store_eflags(e); 
            return; 
        } 
    } 
}
 
void inthandler20(int *esp) 
{ 
    int i, j; 
    io_out8(PIC0_OCW2, 0x60);   /*  把IRQ-00信号接收结束的信息通知给PIC*/ 
    timerctl.count++; 
    if (timerctl.next > timerctl.count) { 
        return; /* 还不到下一个时刻，所以结束*/ 
    }

    // timerctl.next = 0xffffffff;

    struct TIMER *timer = timerctl.t0;
    for (;;) { 
        /* timers的定时器都处于动作中，所以不确认flags */ 
        if (timer->timeout > timerctl.count) { 
            break; 
        } 
        /*
        对于每一个定时器，如果它的timeout（预定时刻）小于等于当前计数器的值，就说明它已经超时了，
        需要将它的flags设置为TIMER_FLAGS_ALLOC（已经分配）并将它的数据放入FIFO缓冲区中，通知应用程序
        */
        /* 超时*/ 
        timer->flags = TIMER_FLAGS_ALLOC; 
        // fifo8_put(timerctl.timers[i]->fifo, timerctl.timers[i]->data);
        fifo32_put(timer->fifo, timer->data); /* 这里！ */ 
        timer = timer->next; /* 下一定时器的地址赋给timer */
    } 
    /* 正好有i个定时器超时了。其余的进行移位。 */ 
    // timerctl.using -= i; 
    // for (j = 0; j < timerctl.using; j++) { 
    //     timerctl.timers[j] = timerctl.timers[i + j]; 
    // } 
    timerctl.t0 = timer;
timerctl.next = timer->timeout;
    // if (timerctl.using > 0) { 
    //     timerctl.next = timerctl.t0->timeout; 
    // } else { 
    //     timerctl.next = 0xffffffff; 保留了尾结点不需要设置最大值
    // }
    return; 
}
