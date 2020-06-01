/*******************************************************************
 * File name: epoll_event.cc
 * Description: A c++ file realizing method in class Epoll_Event_Op
 * Author: Zeyuan Qiu
 * Version: 2.0
 * Date: 2020/05/15
*******************************************************************/

#include "epoll_event.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

/* Epoll_Event_Op静态成员ep初始化 */
int Epoll_Event_Op::ep = -1;
/* Epoll_Event_Op静态成员event_list初始化 */
epoll_event Epoll_Event_Op::event_list[MAX_EVENT_NUMBER];

/* 创建ep文件描述符，唯一标识内核中事件表 */
void Epoll_Event_Op::epoll_init_event(single_connection *&conn)
{
    ep = epoll_create(1024);
}

/* 向epoll内核事件表添加事件，conn已设置回调函数和fd */
void Epoll_Event_Op::epoll_add_event(single_connection *conn, int flag)
{
    epoll_event ee;
    int fd = conn->fd;
    /* 不使用epoll_data_t的fd成员，使用epoll_data_t的ptr成员，在ptr指向的用户数据conn中包含fd */
    ee.data.ptr = (void *)conn;
    ee.events = flag;
    /* EPOLL_CTL_ADD参数表示往事件表中注册fd上的事件 */
    epoll_ctl(ep, EPOLL_CTL_ADD, fd, &ee);
}

/* 修改事件，event已经设置回调函数和fd */
void Epoll_Event_Op::epoll_mod_event(single_connection *conn, int flag)
{
    epoll_event ee;
    int fd = conn->fd;
    ee.data.ptr = (void *)conn;
    ee.events = flag;
    epoll_ctl(ep, EPOLL_CTL_MOD, fd, &ee);
}

/* 删除该描述符上的所有事件，若只删除读事件或写事件，则把相应的事件设置为空函数 */
void Epoll_Event_Op::epoll_del_event(single_connection *conn)
{
    /* EPOLL_CTL_DEL参数表示删除fd上的注册事件，最后一个参数为0即可 */
    epoll_ctl(ep, EPOLL_CTL_DEL, conn->fd, 0);
}

/* 事件循环函数 */
int Epoll_Event_Op::epoll_process_events()
{
    while (true)
    {
        /* epoll_wait等待文件描述符组上的事件，，最后一个参数timeout设置为-1为阻塞，直到某个事件发生 */
        /* 优化：第4个参数由-1更为10， 即由阻塞设置成非阻塞（等待10ms）*/
        int number = epoll_wait(ep, event_list, MAX_EVENT_NUMBER - 1, 10);

        /* 输出就绪事件数以及当前进程pid */
        // printf("[Info] Ready fd quantity: %d \n", number);
        // printf("[Info] Current process id: %d \n", getpid());

        /* 遍历number个就绪事件的文件描述符 */
        for (int i = 0; i < number; i++)
        {
            /* 创建single_connection类型的指针，指向epoll_data_t的ptr成员包含的用户数据，即conn */
            single_connection *conn = (single_connection *)(event_list[i].data.ptr);
            /* 当前触发的fd */
            int socket = conn->fd;
            /* 当前触发的fd为读事件 */
            if (event_list[i].events & EPOLLIN)
            {
                /* 执行读事件回调函数 */
                conn->read_event->handler(conn);
            }
            /* 当前触发的fd为写事件 */
            else if (event_list[i].events & EPOLLOUT)
            {
                /* 执行写事件回调函数 */
                conn->write_event->handler(conn);
            }
            /* 服务器采发生错误 */
            else if (event_list[i].events & EPOLLERR)
            {
                printf("[ERROR] EPOLLERR occurred \n");
            }
        }
    }
    return 0;
}