/************************************************************
 * File name: server.cc
 * Description: A c++ file realizing method in class Server
 * Author: Zeyuan Qiu
 * Version: 2.0
 * Date: 2020/05/15
************************************************************/

#include <arpa/inet.h>
#include <assert.h>
#include <stdio.h>
#include <libgen.h>
#include <stdlib.h>
#include <signal.h>
#include <netinet/in.h>
#include <string.h>

#include <sys/socket.h>
#include <sys/epoll.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/sendfile.h>
#include <errno.h>
#include <fstream>
#include <netinet/tcp.h>
#include "server.h"
#include "epoll_event.h"

using namespace std;

map<string, int> Server::data_map;
vector<string> Server::query_list;
vector<string> Server::post_list;

/* 设置文件描述符fd为非阻塞 */
int Server::setnonblocking(int fd)
{
    int old_option = fcntl(fd, F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_option);
    return old_option;
}

/* 为single_connection类型结构体分配空间及初始化 */
void Server::initConnection(single_connection *&conn)
{
    printf("------ INIT CONNECTION ------ \n");

    /* conn指针分配空间，因此参数为引用类型 */
    conn = (single_connection *)malloc(sizeof(single_connection));
    /* 用户请求数据空间大小初始化为QUERY_INIT_LEN */
    conn->querybuf = (char *)malloc(QUERY_INIT_LEN);
    if (!conn->querybuf)
    {
        printf("[ERROR] Web_connection_t struct space mallocation failed \n");
        return;
    }
    /* 为回调函数结构体分配空间 */
    conn->read_event = (single_event *)malloc(sizeof(single_event));
    conn->write_event = (single_event *)malloc(sizeof(single_event));
    /* 设置当前状态为接收连接 */
    conn->state = ACCEPT;
    /* 设置读取用户请求数据的指针初始偏移量为0 */
    conn->query_start_index = 0;
    /* 设置用户请求数据末端偏移量为0 */
    conn->query_end_index = 0;
    /* 设置用户请求数据可用空间大小为QUERY_INIT_LEN */
    conn->query_remain_len = QUERY_INIT_LEN;

    /* 初始化回调函数结构体空间 */
    memset(conn->read_event, '\0', sizeof(conn->read_event));
    memset(conn->write_event, '\0', sizeof(conn->write_event));
    /* 初始化请求行、请求头部空间 */
    memset(conn->method, '\0', sizeof(conn->method));
    memset(conn->uri, '\0', sizeof(conn->uri));
    memset(conn->version, '\0', sizeof(conn->version));
    memset(conn->host, '\0', sizeof(conn->host));
    memset(conn->proxy_connection, '\0', sizeof(conn->proxy_connection));
    memset(conn->upgrade_insecure_requests, '\0', sizeof(conn->upgrade_insecure_requests));
    memset(conn->user_agent, '\0', sizeof(conn->user_agent));
    memset(conn->accept, '\0', sizeof(conn->accept));
    memset(conn->accept_encoding, '\0', sizeof(conn->accept_encoding));
    memset(conn->accept_language, '\0', sizeof(conn->accept_language));
    memset(conn->connection, '\0', sizeof(conn->connection));
    memset(conn->content_length, '\0', sizeof(conn->content_length));
}

/* 负责监听的连接的读事件回调函数 */
void Server::accept_conn(single_connection *conn)
{
    printf("------ ACCEPT CONNECTION ------ \n");

    /* 创建客户端socket地址 */
    struct sockaddr *client_address;
    socklen_t client_addrlength = sizeof(client_address);

    /* 循环接收连接 */
    int connfd = -1;
    while (connfd = accept(conn->fd, (struct sockaddr *)&client_address, &client_addrlength))
    {
        if (connfd < 0)
        {
            printf("[WARN] Accept no connection \n");
            return;
        }

        /* 使用SO_REUSEADDR选项实现TIME_WAIT状态下已绑定的socket地址可以立即被重用 */
        int reuse = 1;
        setsockopt(connfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
        /* 创建一个新的连接结构体，用于和指定客户端保持连接 */
        single_connection *new_conn = NULL;
        /* 为single_connection类型结构体分配空间及初始化 */
        initConnection(new_conn);
        if (new_conn == NULL)
        {
            printf("[ERROR] Failed to create new connection \n");
            return;
        }
        /* 将connfd赋值给new_conn->fd，由该new_conn负责进行与指定客户端的通信 */
        new_conn->fd = connfd;
        /* 设置连接状态为READ */
        new_conn->state = READ;
        /* 设置渎事件回调函数为read_request */
        new_conn->read_event->handler = read_request;
        /* 向epoll内核事件表添加事件，epoll事件类型为EPOLLIN */
        Epoll_Event_Op::epoll_add_event(new_conn, EPOLLIN | EPOLLERR);
        /* 设置该连接为非阻塞 */
        setnonblocking(connfd);
    }

    printf("------ ACCEPT CONNECTION END------ \n");
}

/* 负责与客户端通信的连接的渎事件的回调函数 */
void Server::read_request(single_connection *conn)
{
    printf("------ READ REQUEST FROM CLIENT BEGIN ------ \n");

    int len, fd = conn->fd;
    conn->data_len = 0;
    /* 循环读取fd中的数据 */
    while (true)
    {
        /* 根据需要扩大缓冲区空间 */
        enlarge_buffer(*conn);
        /* 读取fd上的数据 */
        len = recv(fd, conn->querybuf + conn->query_end_index, conn->query_remain_len, 0);
        if (len < 0)
        {
            /* 对于非阻塞IO，以下条件成立表示数据已经完全读取完毕，此后epoll就能再次触发sockfd上的EPOLLIN事件，以驱动下一次操作 */
            if ((errno == EAGAIN) || (errno == EWOULDBLOCK))
            {
                printf("[INFO] Read client request data completed \n");
                break;
            }
        }
        else if (len > 0)
        {
            /* 更新用户请求数据末端指针及剩余可用空间 */
            conn->query_end_index += len;
            conn->query_remain_len -= len;
            conn->data_len += len;
        }
        /* 通信对方已经关闭连接 */
        else if (len == 0)
        {
            printf("[WARN] Connection is closed by client \n");
            /* 删除连接中描述符上所有的事件 */
            Epoll_Event_Op::epoll_del_event(conn);
            /* 关闭连接，释放其结构体空间 */
            close_conn(conn);
            return;
        }
    }

    printf("[INFO] Data from client \n");
    printf("%s \n", conn->querybuf);
    printf("------ READ REQUEST FROM CLIENT END ------ \n");

    /* 解析请求行 */
    process_request_line(conn);
}

/* 解析请求行 */
void Server::process_request_line(single_connection *conn)
{
    printf("------ REQUEST LINE PARSING START ------ \n");
    /* 连接状态为处理请求行 */
    conn->state = QUERY_LINE;

    /* 检验第一个空格的位置，用以定位请求方法 */
    char *ptr = strpbrk(conn->querybuf + conn->query_start_index, " ");
    if (!ptr)
    {
        printf("[ERROR] Request method parsing failed \n");
        return;
    }
    /* 请求方法长度 */
    int len = ptr - conn->querybuf - conn->query_start_index;
    /* 将请求方法复制到连接结构体中的method元素中 */
    strncpy(conn->method, conn->querybuf + conn->query_start_index, len);
    printf("[Info] Request method: %s \n", conn->method);
    /* 重新定位指针偏移量 */
    conn->query_start_index += (len + 1);

    /* 根据请求方法进行相应操作，当为GET时，将请求uri中的用户查询键分离出来后加入query_list中 */
    if (strcmp(conn->method, "GET") == 0)
    {
        /* key_len为各查询键的长度 */
        int key_len;
        /* 用户查询键临时变量 */
        char temp_key[256];
        memset(temp_key, '\0', sizeof(temp_key));
        /* 临时变量str类型 */
        string temp_key_str;
        /* 设置指针保存uri初始位置指针 */
        char *key_search_begin = NULL;
        /* findand为查询到的“&”的位置 */
        char *find_and = NULL;
        /* 清空query_list数组 */
        query_list.clear();
        /* 将uri初始位置指针赋值给key_search_begin */
        key_search_begin = ptr;
        /* 检验第二个空格的位置，用以定位请求uri */
        ptr = strpbrk(conn->querybuf + conn->query_start_index, " ");
        if (!ptr)
        {
            printf("[ERROR] Request uri parsing failed\n");
            return;
        }
        /* 目标资源uri长度 */
        len = ptr - conn->querybuf - conn->query_start_index;
        /* 将用户输入的查询串提取出键，放入query_list数组中 */
        while (true)
        {
            /* 定位“=”的位置 */
            key_search_begin = strpbrk(key_search_begin + 1, "=");
            /* 若该次查询到的等于号为空或其在ptr指针后，则退出循环 */
            if ((key_search_begin == NULL) || (ptr - key_search_begin <= 0))
            {
                /* 输出获得用户查询串中键的数量 */
                printf("[INFO] Get %d key(s) from client \n", query_list.size());
                break;
            }
            else
            {
                /* 定位“&”的位置 */
                find_and = strpbrk(key_search_begin, "&");
                /* 若该次查询到的“&”为空或其在ptr指针之后，则将其设为ptr */
                if ((find_and == NULL) || (ptr - find_and == 0))
                {
                    find_and = ptr;
                }
                /* 键长 */
                key_len = find_and - key_search_begin - 1;
                /* 拷贝键到temp_key */
                strncpy(temp_key, key_search_begin + 1, key_len);
                temp_key_str = temp_key;
                /* 向query_list添加键 */
                query_list.push_back(temp_key_str);
            }
        }
        /* 输出用户查询串中键的具体信息 */
        for (int i = 0; i < query_list.size(); i++)
        {
            printf("[INFO] The key %d is %s \n", i + 1, query_list[i].c_str());
        }
        conn->query_start_index += (len + 1);
    }
    /* 请求方法为非GET，此处为POST */
    else
    {
        ptr = strpbrk(conn->querybuf + conn->query_start_index, " ");
        if (!ptr)
        {
            printf("[ERROR] Request uri parsing failed \n");
            return;
        }
        /* 目标资源uri长度 */
        len = ptr - conn->querybuf - conn->query_start_index;
        strncpy(conn->uri, conn->querybuf + conn->query_start_index, len);

        printf("[INFO] Request uri: %s \n", conn->uri);

        conn->query_start_index += (len + 1);
    }

    /* 检验\n字符的位置，该行末尾为\r\n */
    ptr = strpbrk(conn->querybuf, "\n");
    if (!ptr)
    {
        printf("[ERROR] Request version parsing failed \n");
        return;
    }
    /* 客户端wget程序使用的HTTP版本号 */
    len = ptr - conn->querybuf - conn->query_start_index;
    /* 最后一个参数需要在原长度-1，即除去\r */
    strncpy(conn->version, conn->querybuf + conn->query_start_index, len - 1);

    printf("[INFO] Request version: %s \n", conn->version);

    conn->query_start_index += (len + 1);

    printf("------ REQUEST LINE PARSING END ------ \n");

    /* 解析请求头部 */
    process_head(conn);
}

/* 解析请求头部 */
void Server::process_head(single_connection *conn)
{
    printf("------ REQUEST HEAD PARSING START ------ \n");

    /* 更新连接状态为QUERY_HEAD */
    conn->state = QUERY_HEAD;
    char *end_line;
    int len;

    while (true)
    {
        end_line = strpbrk(conn->querybuf + conn->query_start_index, "\n");
        /* 获取该行字符数 */
        len = end_line - conn->querybuf - conn->query_start_index;
        if (len == 1)
        {
            /* 解析完毕 */
            printf("------ REQUEST HEAD PARSING END ------ \n");
            conn->query_start_index += (len + 1);
            break;
        }
        else
        {
            /* 解析请求头部是否包含Host，下同 */
            if (strncasecmp(conn->querybuf + conn->query_start_index, "Host:", 5) == 0)
            {
                strncpy(conn->host, conn->querybuf + conn->query_start_index + 6, len - 7);
                printf("[INFO] Request host: %s \n", conn->host);
            }
            else if (strncasecmp(conn->querybuf + conn->query_start_index, "Proxy-Connection:", 17) == 0)
            {
                strncpy(conn->proxy_connection, conn->querybuf + conn->query_start_index + 18, len - 19);
                printf("[INFO] Request proxy_connection: %s \n", conn->proxy_connection);
            }
            else if (strncasecmp(conn->querybuf + conn->query_start_index, "Upgrade-Insecure-Requests:", 26) == 0)
            {
                strncpy(conn->upgrade_insecure_requests, conn->querybuf + conn->query_start_index + 27, len - 28);
                printf("[INFO] Request upgrade_insecure_requests: %s \n", conn->upgrade_insecure_requests);
            }
            else if (strncasecmp(conn->querybuf + conn->query_start_index, "User-Agent:", 11) == 0)
            {
                strncpy(conn->user_agent, conn->querybuf + conn->query_start_index + 12, len - 13);
                printf("[INFO] Request user_agent: %s \n", conn->user_agent);
            }
            else if (strncasecmp(conn->querybuf + conn->query_start_index, "Accept:", 7) == 0)
            {
                strncpy(conn->accept, conn->querybuf + conn->query_start_index + 8, len - 9);
                printf("[INFO] Request accept: %s \n", conn->accept);
            }
            else if (strncasecmp(conn->querybuf + conn->query_start_index, "Accept-Encoding:", 16) == 0)
            {
                strncpy(conn->accept_encoding, conn->querybuf + conn->query_start_index + 17, len - 18);
                printf("[INFO] Request accept_encoding: %s \n", conn->accept_encoding);
            }
            else if (strncasecmp(conn->querybuf + conn->query_start_index, "Accept-Language:", 16) == 0)
            {
                strncpy(conn->accept_language, conn->querybuf + conn->query_start_index + 17, len - 18);
                printf("[INFO] Request accept_language: %s \n", conn->accept_language);
            }
            else if (strncasecmp(conn->querybuf + conn->query_start_index, "connection:", 11) == 0)
            {
                strncpy(conn->connection, conn->querybuf + conn->query_start_index + 12, len - 13);
                printf("[INFO] Request connection: %s \n", conn->connection);
            }
            else if (strncasecmp(conn->querybuf + conn->query_start_index, "Content-Length:", 15) == 0)
            {
                strncpy(conn->content_length, conn->querybuf + conn->query_start_index + 16, len - 17);
                printf("[INFO] Request content-length: %s \n", conn->content_length);
            }
            else
            {
            }
            conn->query_start_index += (len + 1);
        }
    }

    /* 解析可选消息体 */
    process_body(conn);
}

/* 解析可选消息体 */
void Server::process_body(single_connection *conn)
{
    printf("------ REQUEST BODY PARSING START ------ \n");

    /* 更新连接状态为QUERY_BODY */
    conn->state = QUERY_BODY;

    /* 判断可选消息体是否为空 */
    if (conn->query_start_index == conn->query_end_index)
    {
        printf("[WARN] HTTP request content is empty \n");
    }
    else
    {
        /* 请求方法若为GET则忽略请求体 */
        if (strcmp(conn->method, "GET") == 0)
        {
            printf("[WARN] Skip parsing HTTP request content \n");
        }
        /* 请求方法若为POST则需要解析请求体 */
        else
        {
            /* 清空post_list */
            post_list.clear();
            /* key */
            string key_str;
            char key[32];
            /* value */
            int value_int;
            char value[32];
            /* 键或值的长度 */
            int len;
            /* 可选消息体的末尾地址 */
            char *ptr = conn->querybuf + conn->data_len;
            /* find_equ定位“=”的位置 */
            char *find_equ = conn->querybuf + conn->query_start_index;
            /* find_and定位“&”的位置 */
            char *find_and = conn->querybuf + conn->query_start_index;
            while (true)
            {
                memset(key, '\0', sizeof(key));
                memset(value, '\0', sizeof(value));
                /* 定位“=”的位置 */
                find_equ = strpbrk(find_equ + 1, "=");
                /* 所找寻的“=”超出该行范围，退出循环 */
                if ((find_equ == NULL) || (ptr - find_equ <= 0))
                {
                    break;
                }
                /* 定位“&”的位置 */
                find_and = strpbrk(find_and + 1, "&");
                /* 键的长度 */
                len = find_and - find_equ - 1;
                /* 拷贝key值到key字符数组 */
                strncpy(key, find_equ + 1, len);
                key_str = key;
                /* 将键放入post_list以进行后续输出 */
                post_list.push_back(key_str);
                /* 定位“=”的位置 */
                find_equ = strpbrk(find_equ + 1, "=");
                /* 定位“&”的位置 */
                find_and = strpbrk(find_and + 1, "&");
                /* 所找寻的“=”超出该行范围，退出循环 */
                if ((find_and == NULL) || (ptr - find_and <= 0))
                {
                    find_and = ptr;
                }
                /* 值的长度 */
                len = find_and - find_equ - 1;
                /* 拷贝value值到value字符数组 */
                strncpy(value, find_equ + 1, len);
                /* value由字符数组转换为整形 */
                value_int = atoi(value);

                printf("[INFO] Insert into the map: key = %s, value = %d \n", key, value_int);

                /* 向map中插入键值对 */
                data_map.insert(make_pair(key_str, value_int));
            }
        }
    }

    /* 修改该连接状态为SEND_DATA */
    conn->state = SEND_DATA;
    /* 重置起始偏移量以及末尾指针 */
    conn->query_start_index = conn->query_end_index = 0;
    /* 设置该连接写事件回调函数为send_response */
    conn->write_event->handler = send_response;
    /* 设置该连接读事件回调函数为空 */
    conn->read_event->handler = empty_event_handler;
    /* 修改epoll事件为EPOLLOUT */
    Epoll_Event_Op::epoll_mod_event(conn, EPOLLOUT | EPOLLERR);

    printf("------ REQUEST BODY PARSING END ------ \n");
}

/* 负责与客户端通信的连接的写事件的回调函数 */
void Server::send_response(single_connection *conn)
{
    printf("------ REQUEST RESPONSE SEND START ------ \n");

    /* 若请求方法为POST，则将用户POST的键值对进行整合返回给客户端 */
    /*  POST需要返回数据，这样可以使用wrk进行压测 */
    if (strcmp(conn->method, "POST") == 0)
    {
        /* HTTP应答 */
        char response_res[1024];
        memset(response_res, '\0', sizeof(response_res));
        /* 整合状态行 */
        sprintf(response_res, "%sHTTP/1.1 200 OK\r\n", response_res);
        map<string, int>::iterator it;
         /* 响应主体 */
        char response_body[1024];
        memset(response_body, '\0', sizeof(response_body));
        /* 目标文档的MIME类型为text/html */
        sprintf(response_body, "%s<html><body>The key-value you instered <br>", response_body);
        /* 根据post_list的key，查询对应的value */
        for (int i = 0; i < post_list.size(); i++)
        {
            it = data_map.find(post_list[i]);
            sprintf(response_body, "%skey = %s , value = %d <br>", response_body, it->first.c_str(), it->second);
        }
        sprintf(response_body, "%s</html></body>", response_body);
        /* 响应主体长度 */
        int response_len = strlen(response_body);

        /* 整合应答头部字段，指出目标文档的MIME类型 */
        sprintf(response_res, "%sContent-Length: %d\r\nContent-Type: text/html\r\n", response_res, response_len);
        /* 整合HTTP应答 */
        sprintf(response_res, "%s\r\n%s", response_res, response_body);

        printf("[INFO] The response is formatted \n");
        printf("%s \n", response_res);

        /* 向连接fd发送HTTP应答 */
        send(conn->fd, response_res, sizeof(response_res), 0);

        /* 将map中的键值对写入文件map.txt，应放在POST方法的响应体发送模块后 */
        printf("------ WRITE INTO THE FILE START ------ \n");
        char seq[FILE_SEQ_LEN];
        memset(seq, '\0', FILE_SEQ_LEN);
        for (it = data_map.begin(); it != data_map.end(); it++)
        {
            sprintf(seq, "%s%s %d\n", seq, it->first.c_str(), it->second);
        }

        int seq_len = strlen(seq);
        ofstream ofs;
        ofs.open("map.txt");
        ofs.write(seq, seq_len);
        ofs.close();

        printf("------ WRITE INTO THE FILE END ------ \n");
    }
    /* 若请求方法为GET，则进行键值对整合向客户端发送 */
    else
    {
        /* HTTP应答 */
        char response_res[1024];
        memset(response_res, '\0', sizeof(response_res));
        /* 整合状态行 */
        sprintf(response_res, "%sHTTP/1.1 200 OK\r\n", response_res);
        map<string, int>::iterator it;
        /* 响应主体 */
        char response_body[1024];
        memset(response_body, '\0', sizeof(response_body));
        /* 目标文档的MIME类型为text/html */
        sprintf(response_body, "%s<html><body>", response_body);
        for (int i = 0; i < query_list.size(); i++)
        {
            // 在map查询用户输入的key
            it = data_map.find(query_list[i]);
            // 若该key存在
            if (it != data_map.end())
            {
                sprintf(response_body, "%skey = %s , value = %d <br>", response_body, it->first.c_str(), it->second);
            }
            // 若key不存在
            else
            {
                sprintf(response_body, "%sthe key %s is not existed <br>", response_body, query_list[i].c_str());
            }
        }
        sprintf(response_body, "%s</html></body>", response_body);
        /* 响应主体长度 */
        int response_len = strlen(response_body);

        /* 整合应答头部字段，指出目标文档的MIME类型 */
        sprintf(response_res, "%sContent-Length: %d\r\nContent-Type: text/html\r\n", response_res, response_len);
        /* 整合HTTP应答 */
        sprintf(response_res, "%s\r\n%s", response_res, response_body);

        printf("[INFO] The response is formatted \n");
        printf("%s \n", response_res);

        /* 向连接fd发送HTTP应答 */
        send(conn->fd, response_res, sizeof(response_res), 0);
    }

    /* 删除该事件并关闭连接 */
    Epoll_Event_Op::epoll_del_event(conn);
    close_conn(conn);

    printf("------ REQUEST RESPONSE SEND END ------ \n");
}

/* 根据需要扩大用户请求数据空间 */
void Server::enlarge_buffer(single_connection &conn)
{
    /* 用户请求数据缓冲区剩余空间大小小于REMAIN_BUFFER，则需要在原空间大小基础上增加QUERY_INIT_LEN */
    if (conn.query_remain_len < REMAIN_BUFFER)
    {
        int new_size = strlen(conn.querybuf) + QUERY_INIT_LEN;
        conn.querybuf = (char *)realloc(conn.querybuf, new_size);
        /* 更新剩余空间大小 */
        conn.query_remain_len = new_size - conn.query_end_index;
    }
}

/* 空回调函数 */
void Server::empty_event_handler(single_connection *conn)
{
    printf("[WARN] Fd %d was set to the empty event handler \n", conn->fd);
}

/* 关闭连接，释放其结构体空间 */
void Server::close_conn(single_connection *conn)
{

    /* 统计已经关闭连接的客户端数 */
    static int count = 0;
    count++;
    // printf("[INFO] Close the connection %d \n", count);

    /* 关闭文件描述符 */
    close(conn->fd);
    /* 释放元素空间 */
    free(conn->querybuf);
    free(conn->read_event);
    free(conn->write_event);
    /* 释放结构体空间 */
    free(conn);
}