/* bootpackのメイン */

#include "bootpack.h"
#include <stdio.h>

// extern struct FIFO8 keyfifo, mousefifo;

// void enable_mouse(struct MOUSE_DEC *mdec);
// int mouse_decode(struct MOUSE_DEC *mdec, unsigned char dat);
// void init_keyboard(void);
void putfonts8_asc_sht(struct SHEET *sht, int x, int y, int c, int b, char *s, int l)
{
    boxfill8(sht->buf, sht->bxsize, b, x, y, x + l * 8 - 1, y + 15);
    putfonts8_asc(sht->buf, sht->bxsize, x, y, c, s);
    sheet_refresh(sht, x, y, x + l * 8, y + 16);
    return;
}
void make_window8(unsigned char *buf, int xsize, int ysize, char *title)
{
    static char closebtn[14][16] = {
        "OOOOOOOOOOOOOOO@",
        "OQQQQQQQQQQQQQ$@",
        "OQQQQQQQQQQQQQ$@",
        "OQQQ@@QQQQ@@QQ$@",
        "OQQQQ@@QQ@@QQQ$@",
        "OQQQQQ@@@@QQQQ$@",
        "OQQQQQQ@@QQQQQ$@",
        "OQQQQQ@@@@QQQQ$@",
        "OQQQQ@@QQ@@QQQ$@",
        "OQQQ@@QQQQ@@QQ$@",
        "OQQQQQQQQQQQQQ$@",
        "OQQQQQQQQQQQQQ$@",
        "O$$$$$$$$$$$$$$@",
        "@@@@@@@@@@@@@@@@"};
    int x, y;
    char c;
    boxfill8(buf, xsize, COL8_C6C6C6, 0, 0, xsize - 1, 0);
    boxfill8(buf, xsize, COL8_FFFFFF, 1, 1, xsize - 2, 1);
    boxfill8(buf, xsize, COL8_C6C6C6, 0, 0, 0, ysize - 1);
    boxfill8(buf, xsize, COL8_FFFFFF, 1, 1, 1, ysize - 2);
    boxfill8(buf, xsize, COL8_848484, xsize - 2, 1, xsize - 2, ysize - 2);
    boxfill8(buf, xsize, COL8_000000, xsize - 1, 0, xsize - 1, ysize - 1);
    boxfill8(buf, xsize, COL8_C6C6C6, 2, 2, xsize - 3, ysize - 3);
    boxfill8(buf, xsize, COL8_000084, 3, 3, xsize - 4, 20);
    boxfill8(buf, xsize, COL8_848484, 1, ysize - 2, xsize - 2, ysize - 2);
    boxfill8(buf, xsize, COL8_000000, 0, ysize - 1, xsize - 1, ysize - 1);
    putfonts8_asc(buf, xsize, 24, 4, COL8_FFFFFF, title);
    for (y = 0; y < 14; y++)
    {
        for (x = 0; x < 16; x++)
        {
            c = closebtn[y][x];
            if (c == '@')
            {
                c = COL8_000000;
            }
            else if (c == '$')
            {
                c = COL8_848484;
            }
            else if (c == 'Q')
            {
                c = COL8_C6C6C6;
            }
            else
            {
                c = COL8_FFFFFF;
            }
            buf[(5 + y) * xsize + (xsize - 21 + x)] = c;
        }
    }
    return;
}
void HariMain(void)
{
    struct BOOTINFO *binfo = (struct BOOTINFO *)ADR_BOOTINFO;
    // struct FIFO8 timerfifo, timerfifo2, timerfifo3;
    struct FIFO32 fifo;
    char s[40];
    int fifobuf[128];
    char keybuf[32], mousebuf[128], timerbuf[8], timerbuf2[8], timerbuf3[8];
    struct TIMER *timer, *timer2, *timer3;
    int mx, my, i;
    unsigned int memtotal, count = 0; // 图层计数
    struct MOUSE_DEC mdec;
    struct MEMMAN *memman = (struct MEMMAN *)MEMMAN_ADDR;
    struct SHTCTL *shtctl;
    struct SHEET *sht_back, *sht_mouse, *sht_win;
    unsigned char *buf_back, buf_mouse[256], *buf_win;

    init_gdtidt();
    init_pic();
    io_sti(); /* IDT/PIC的初始化已经完成，于是开放CPU的中断 */
    fifo32_init(&fifo, 128, fifobuf);
    init_keyboard(&fifo, 256);
    enable_mouse(&fifo, 512, &mdec);
    // fifo8_init(&keyfifo, 32, keybuf);
    // fifo8_init(&mousefifo, 128, mousebuf);
    io_out8(PIC0_IMR, 0xf9); /* 开放PIC1和键盘中断(11111001) */

    init_pit();                                                        /* 这里！ */
    io_out8(PIC0_IMR, 0xf8); /* PIT和PIC1和键盘设置为许可(11111000) */ /* 这里！ */

    io_out8(PIC1_IMR, 0xef); /* 开放鼠标中断(11101111) */
                             // fifo8_init(&timerfifo, 8, timerbuf);

    timer = timer_alloc();
    timer_init(timer, &fifo, 10);
    timer_settime(timer, 1000);
    // fifo8_init(&timerfifo2, 8, timerbuf2);
    timer2 = timer_alloc();
    timer_init(timer2, &fifo, 3);
    timer_settime(timer2, 300);
    // fifo8_init(&timerfifo3, 8, timerbuf3);
    timer3 = timer_alloc();
    timer_init(timer3, &fifo, 1);
    timer_settime(timer3, 50);

    // init_keyboard();
    // enable_mouse(&mdec);
    memtotal = memtest(0x00400000, 0xbfffffff);
    memman_init(memman);
    memman_free(memman, 0x00001000, 0x0009e000); /* 0x00001000 - 0x0009efff */
    memman_free(memman, 0x00400000, memtotal - 0x00400000);

    init_palette();
    shtctl = shtctl_init(memman, binfo->vram, binfo->scrnx, binfo->scrny);
    sht_back = sheet_alloc(shtctl);
    sht_mouse = sheet_alloc(shtctl);
    sht_win = sheet_alloc(shtctl); /* 这里！ */
    buf_back = (unsigned char *)memman_alloc_4k(memman, binfo->scrnx * binfo->scrny);
    buf_win = (unsigned char *)memman_alloc_4k(memman, 160 * 52); /* 这里！ */

    sheet_setbuf(sht_back, buf_back, binfo->scrnx, binfo->scrny, -1); /* 没有透明色 */
    sheet_setbuf(sht_win, buf_win, 160, 52, -1); /* 没有透明色 */     /* 这里！*/
    sheet_setbuf(sht_mouse, buf_mouse, 16, 16, 99);                   /* 透明色号99 */

    init_screen8(buf_back, binfo->scrnx, binfo->scrny);
    init_mouse_cursor8(buf_mouse, 99);         /* 背景色号99 */
    make_window8(buf_win, 160, 52, "counter"); /* 这里！ */

    sheet_slide(sht_back, 0, 0);
    mx = (binfo->scrnx - 16) / 2; /* 按显示在画面中央来计算坐标 */
    my = (binfo->scrny - 28 - 16) / 2;
    sheet_slide(sht_mouse, mx, my);
    sheet_slide(sht_win, 80, 72); /* 这里！ */

    sheet_updown(sht_back, 0);
    sheet_updown(sht_win, 1); /* 这里！ */
    sheet_updown(sht_mouse, 2);
    sprintf(s, "(%3d, %3d)", mx, my);
    putfonts8_asc(buf_back, binfo->scrnx, 0, 0, COL8_FFFFFF, s);
    sprintf(s, "memory %dMB free : %dKB", memtotal / (1024 * 1024), memman_total(memman) / 1024);
    putfonts8_asc(buf_back, binfo->scrnx, 0, 32, COL8_FFFFFF, s);
    sheet_refresh(sht_back, 0, 0, binfo->scrnx, 48); /* 刷新文字 */

    // fifo8_init(&timerfifo, 8, timerbuf);
    // settimer(1000, &timerfifo, 1);

    for (;;)
    {
        count++; /* 从这里开始 */
        sprintf(s, "%010d", count);
        sprintf(s, "%010d", timerctl.count); /* 这里！ */

        // boxfill8(buf_win, 160, COL8_C6C6C6, 40, 28, 119, 43);
        // putfonts8_asc(buf_win, 160, 40, 28, COL8_000000, s);
        // sheet_refresh(sht_win, 40, 28, 120, 44); /* 到这里结束 */

        putfonts8_asc_sht(sht_win, 40, 28, COL8_000000, COL8_C6C6C6, s, 10);
        io_cli();
        if (fifo32_status(&fifo) == 0)
        {
            io_stihlt();
        }
        else
        {

            i = fifo32_get(&fifo);
            io_sti();
            if (256 <= i && i <= 511)
            { /* 键盘数据*/
                sprintf(s, "%02X", i - 256);
                putfonts8_asc_sht(sht_back, 0, 16, COL8_FFFFFF, COL8_008484, s, 2);
            }
            else if (512 <= i && i <= 767)
            { /* 鼠标数据*/
                if (mouse_decode(&mdec, i - 512) != 0)
                {
                    /* 已经收集了3字节的数据，所以显示出来 */
                    sprintf(s, "[lcr %4d %4d]", mdec.x, mdec.y);
                    if ((mdec.btn & 0x01) != 0)
                    {
                        s[1] = 'L';
                    }
                    if ((mdec.btn & 0x02) != 0)
                    {
                        s[3] = 'R';
                    }
                    if ((mdec.btn & 0x04) != 0)
                    {
                        s[2] = 'C';
                    }
                    putfonts8_asc_sht(sht_back, 32, 16, COL8_FFFFFF, COL8_008484, s, 15);
                    /* 鼠标指针的移动 */
                    mx += mdec.x;

                    my += mdec.y;
                    if (mx < 0)
                    {
                        mx = 0;
                    }
                    if (my < 0)
                    {
                        my = 0;
                    }
                    if (mx > binfo->scrnx - 1)
                    {
                        mx = binfo->scrnx - 1;
                    }

                    if (my > binfo->scrny - 1)
                    {
                        my = binfo->scrny - 1;
                    }
                    sprintf(s, "(%3d, %3d)", mx, my);
                    putfonts8_asc_sht(sht_back, 0, 0, COL8_FFFFFF, COL8_008484, s, 10);
                    sheet_slide(sht_mouse, mx, my);
                }
            }
            else if (i == 10)
            { /* 10秒定时器 */
                putfonts8_asc_sht(sht_back, 0, 64, COL8_FFFFFF, COL8_008484, "10[sec]", 7);
                sprintf(s, "%010d", count);
                putfonts8_asc_sht(sht_win, 40, 28, COL8_000000, COL8_C6C6C6, s, 10);
            }
            else if (i == 3)
            { /* 3秒定时器 */
                putfonts8_asc_sht(sht_back, 0, 80, COL8_FFFFFF, COL8_008484, "3[sec]", 6);
                count = 0; /* 开始测试 */
            }
            else if (i == 1)
            {                                 /* 光标用定时器*/
                timer_init(timer3, &fifo, 0); /* 下面是设定0 */
                boxfill8(buf_back, binfo->scrnx, COL8_FFFFFF, 8, 96, 15, 111);
                timer_settime(timer3, 50);
                sheet_refresh(sht_back, 8, 96, 16, 112);
            }
            else if (i == 0)
            {                                 /* 光标用定时器 */
                timer_init(timer3, &fifo, 1); /* 下面是设定1 */
                boxfill8(buf_back, binfo->scrnx, COL8_008484, 8, 96, 15, 111);
                timer_settime(timer3, 50);
                sheet_refresh(sht_back, 8, 96, 16, 112);
            }
        }
    }
}
