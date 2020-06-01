/**************************************************
 * File name: epoll_event.cc
 * Description: A c++ file including main program
 * Author: Zeyuan Qiu
 * Version: 2.0
 * Date: 2020/05/15
**************************************************/

#include <arpa/inet.h>
#include <assert.h>
#include <fstream>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "server.h"
#include "epoll_event.h"

using namespace std;

int main(int argc, char *argv[])
{
    // 判断用户输入参数数量是否正确
    if (argc <= 2)
    {
        printf("usage: %s ip_address port_number \n", basename(argv[0]));
        return 1;
    }

    // 修改，重定向输出
    // freopen("http.log", "w+", stdout);

    const char *ip = argv[1];
    int port = atoi(argv[2]);

    // 服务器端屏蔽SIGPIPE信号
    signal(SIGPIPE, SIG_IGN);

    // 创建专用socket地址
    struct sockaddr_in address;
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    inet_pton(AF_INET, ip, &address.sin_addr);
    address.sin_port = htons(port);

    // 创建socket
    int listenfd = socket(PF_INET, SOCK_STREAM, 0);
    assert(listenfd >= 0);

    // 使用SO_REUSEADDR选项实现TIME_WAIT状态下已绑定的socket地址可以立即被重用
    int reuse = 1;
    setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
    // 命名socket，与具体的地址address绑定
    int ret = bind(listenfd, (struct sockaddr *)&address, sizeof(address));
    assert(ret != -1);
    // 监听socket，backlog参数为监听队列长度
    ret = listen(listenfd, 50);
    assert(ret != -1);

    // 创建single_connection类型指针，用于创建一个用于接受连接的结构体，该结构体包含fd、state、读写事件结构体、请求数据指针等
    single_connection *conn = NULL;
    // 创建ep文件描述符，唯一标识内核中事件表
    Epoll_Event_Op::epoll_init_event(conn);
    // 为single_connection类型结构体及其内部元素分配空间及初始化
    Server::initConnection(conn);
    if (conn == NULL)
    {
        // printf("[ERROR] Failed to create new listen connection \n");
        return -1;
    }

    // 将listenfd赋值给conn->fd，由该conn负责进行监听网络初始请求数据
    conn->fd = listenfd;
    // 该监听连接的读事件回调函数为accept_conn
    conn->read_event->handler = Server::accept_conn;
    // 向epoll内核事件表添加事件，事件类型为可读事件
    Epoll_Event_Op::epoll_add_event(conn, EPOLLIN | EPOLLERR);

    // 设置该监听socket非阻塞
    Server::setnonblocking(listenfd);

    // 此模块为HTTP GET的测试模块
    // data_map["aa"] = 11;
    // data_map["bb"] = 22;
    // data_map["cc"] = 33;

    // 从文件读取map数据
    // printf("------ READ FROM THE FILE START ------ \n");
    char seq[FILE_SEQ_LEN];
    memset(seq, '\0', FILE_SEQ_LEN);
    // 从存储文件读取数据
    ifstream ifs;
    int seq_len;
    ifs.open("map.txt");
    ifs.seekg(0, ios::end);
    // 文件内容长度
    seq_len = ifs.tellg();
    // 回到文件起始
    ifs.seekg(0, ios::beg);
    ifs.read(seq, seq_len);

    ifs.close();
    // printf("[INFO] Read from the storage file: \n");
    // printf("%s", seq);
    // printf("[INFO] Read from the storage file \n");

    // 将seq解析到map中
    string key;
    int value;
    // 临时长度
    int temp_len;
    // 创建一个string用于将字符数组转换为字符串便于操作
    string seq_str = seq;
    int start = 0;
    for (int i = 0; i < seq_len; i++)
    {
        if (seq[i] == ' ')
        {
            temp_len = i - start;
            key = seq_str.substr(start, temp_len);
            start = i + 1;
        }
        if (seq[i] == '\n')
        {
            temp_len = i - start;
            value = atoi(seq_str.substr(start, temp_len).c_str());
            start = i + 1;
            // 向map插入一个键值对
            Server::data_map.insert(make_pair(key, value));
        }
    }

    // printf("------ READ FROM THE FILE END ------ \n");

    // 进入事件循环函数，等待事件到达
    Epoll_Event_Op::epoll_process_events();
    return 0;
}