#include"chatserver.hpp"
#include"chatservice.hpp"
#include"json.hpp"
#include<string>
#include"usermodel.hpp"
#include<functional>
using namespace std;
using namespace std::placeholders;
using json=nlohmann::json;
//ChatServer构造函数
ChatServer::ChatServer(EventLoop* loop,
            const InetAddress& listenAddr,
            const string& nameArg):_server(loop,listenAddr,nameArg),_loop(loop)
            {
                //注册链接回调
                _server.setConnectionCallback(std::bind(&ChatServer::onConnection,this,_1));
                //注册消息回调
                _server.setMessageCallback(std::bind(&ChatServer::onMessage,this,_1,_2,_3));
                //设置线程数量
                _server.setThreadNum(4);
            }
//启动函数
void ChatServer::start()
{
    _server.start();
}

//上报链接相关信息的回调函数
void  ChatServer::onConnection(const TcpConnectionPtr&conn)
{
    //客户端断开链接
    if(!conn->connected())
    {
        ChatService::instance()->clientCloseException(conn);
        conn->shutdown();
    }
}
//上报读写时间相关信息的回调函数
void  ChatServer::onMessage(const TcpConnectionPtr &conn,
               Buffer *buffer,
               Timestamp time)
{
    string recvbuf=buffer->retrieveAllAsString();
    //反序列化字符串
    json js=json::parse(recvbuf);

    //目的：完全解耦网络模块代码和业务模块的代码
    //根据js["mgstype"]获取业务handler，调用相应的回调函数

    auto Handler=ChatService::instance()->getHandler(js["msgid"].get<int>());
    //执行相应的业务处理
    Handler(conn,js,time);//只有两行代码
}