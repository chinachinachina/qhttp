/***********************************************************************************
 * File name: server.h
 * Description: A head file related to class Server and struct single_connection
 * Author: Zeyuan Qiu
 * Version: 2.0
 * Date: 2020/05/15
***********************************************************************************/

#ifndef SERVER_H
#define SERVER_H

#include <map>
#include <vector>
#include <string>
#include "epoll_event.h"
#define FILE_SEQ_LEN 4096

using namespace std;

struct single_event;

/* HTTP连接结构体 */
struct single_connection
{
    /* 连接的fd */
    int fd;
    /* 当前处理到的阶段 */
    int state;
    /* 读事件回调函数 */
    single_event *read_event;
    /* 写事件回调函数 */
    single_event *write_event;
    /* 用户请求数据空间起始指针 */
    char *querybuf;
    /* 用户请求数据的当前指针偏移量 */
    int query_start_index;
    /* 用户请求数据的结束位置（相对于起始位置的偏移量） */
    int query_end_index;
    /* 用户请求数据空间中的可用空间 */
    int query_remain_len;
    /* 用户请求数据长度 */
    int data_len;

    /* HTTP请求行字段 */
    char method[16];
    char uri[128];
    char version[32];

    /* HTTP请求头部字段 */
    char host[128];
    char proxy_connection[64];
    char upgrade_insecure_requests[64];
    char user_agent[64];
    char accept[128];
    char accept_encoding[128];
    char accept_language[128];
    char connection[64];
    char content_length[64];
};

class Server
{
public:
    /* 服务器保存的键值对 */
    static map<string, int> data_map;
    /* 用户查询键的数组（请求方法为GET） */
    static vector<string> query_list;
    /* 用户写入键值对中键数组（请求方法为POST） */
    static vector<string> post_list;

    /* 设置文件描述符fd为非阻塞 */
    static int setnonblocking(int fd);
    /* 为single_connection类型结构体分配空间及初始化 */
    static void initConnection(single_connection *&conn);
    /* 负责监听的连接的读事件回调函数 */
    static void accept_conn(single_connection *conn);
    /* 负责与客户端通信的连接的渎事件的回调函数 */
    static void read_request(single_connection *conn);
    /* 解析请求行 */
    static void process_request_line(single_connection *conn);
    /* 解析请求头部 */
    static void process_head(single_connection *conn);
    /* 解析可选消息体 */
    static void process_body(single_connection *conn);
    /* 负责与客户端通信的连接的写事件的回调函数 */
    static void send_response(single_connection *conn);
    /* 根据需要扩大用户请求数据空间 */
    static void enlarge_buffer(single_connection &conn);
    /* 空回调函数 */
    static void empty_event_handler(single_connection *conn);
    /* 关闭连接，释放其结构体空间 */
    static void close_conn(single_connection *conn);
};

#endif