/***********************************************************************************
 * File name: epoll_event.h
 * Description: A head file related to class Epoll_Event_Op and struct single_event
 * Author: Zeyuan Qiu
 * Version: 2.0
 * Date: 2020/05/15
***********************************************************************************/

#ifndef EPOLL_EVENT_H
#define EPOLL_EVENT_H
#include <netinet/in.h>
#include <sys/epoll.h>
#include "server.h"
/* 最大事件数量 */
#define MAX_EVENT_NUMBER 10000
/* 用户请求初始内存分配大小 */
#define QUERY_INIT_LEN 1024
/* 用户请求可用内存大小 */
#define REMAIN_BUFFER 512
/* 以下为处理机状态 */
#define ACCEPT 1
#define READ 2
#define QUERY_LINE 4
#define QUERY_HEAD 8
#define QUERY_BODY 16
#define SEND_DATA 32

struct single_connection;
/* 事件回调函数指针 */
typedef void (*event_handler_ptr)(single_connection *conn);

/* 每个事件由single_event结构体表示 */
struct single_event
{
    /* 为1时表示此事件是可写的，通常情况下表示对应的TCP连接状态可写，即连接处于可以发送网络包的状态 */
    unsigned write : 1;
    /* 为1时表示此事件可以建立新的连接，在listening动态数组中，每一个监听对象对应的读事件中的accept标志位是1 */
    unsigned accept : 1;
    /* 为1时表示当前事件是活跃的，该状态对应事件驱动模块 */
    unsigned active : 1;
    /* 为1时表示当前处理的字符流已结束 */
    unsigned eof : 1;
    /* 为1时表示事件处理过程中出现错误 */
    unsigned error : 1;
    /* 为1时表示当前事件已经关闭 */
    unsigned closed : 1;
    /* 事件处理方法，每个连接模块都需要重新实现它 */
    event_handler_ptr handler;
};

class Epoll_Event_Op
{
public:
    /* epoll对象描述符（额外描述符），每个进程只有一个，唯一标识内核中事件表 */
    static int ep;

    /* 创建事件组，用于从内核事件表获取就绪事件，现将其放在外部，以防占用系统空间 */
    static epoll_event event_list[MAX_EVENT_NUMBER];

    /* 创建ep文件描述符，唯一标识内核中事件表 */
    static void epoll_init_event(single_connection *&conn);
    /* 向epoll内核事件表添加事件，conn已设置回调函数和fd */
    static void epoll_add_event(single_connection *conn, int flag);
    /* 修改事件，event已经设置回调函数和fd */
    static void epoll_mod_event(single_connection *conn, int flag);
    /* 删除该描述符上的所有事件，若只删除读事件或写事件，则把相应的事件设置为空函数 */
    static void epoll_del_event(single_connection *conn);
    /* 事件循环函数 */
    static int epoll_process_events();
};

#endif