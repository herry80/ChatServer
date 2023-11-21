#ifndef CHATSERVER_H
#define CHATSERVER_H
#include<muduo/net/TcpServer.h>
#include<muduo/net/EventLoop.h>
using namespace muduo;
using namespace muduo::net;

//聊天服务器主类
class ChatServer
{
    public:
    //构造函数
    ChatServer(EventLoop* loop,
            const InetAddress& listenAddr,
            const string& nameArg);
    //启动函数
    void start();
    private:
    //上报链接相关信息的回调函数
    void onConnection(const TcpConnectionPtr&);
    //上报读写时间相关信息的回调函数
    void onMessage(const TcpConnectionPtr&,
                            Buffer*,
                            Timestamp);
    TcpServer _server;
    EventLoop* _loop;
};

#endif