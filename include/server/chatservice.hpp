#ifndef CHATSERVICE_H
#define CHATSERVICE_H

#include<functional>
#include<mutex>
#include<unordered_map>
#include<muduo/net/TcpConnection.h>
using namespace std;
using namespace muduo;
using namespace muduo::net;

#include"redis.hpp"
#include"usermodel.hpp"
#include"user.hpp"

#include"offlinemessagemodel.hpp"
#include"friendmodel.hpp"
#include"groupmodel.hpp"
#include"json.hpp"
using json=nlohmann::json;

using MsgHandler=std::function<void(const TcpConnectionPtr&conn,json&js,Timestamp)>;
//聊天服务器业务类  单例模式
class ChatService
{
public:
    ChatService(const ChatService&)=delete;
    ChatService& operator=(const ChatService&)=delete;
    //获取单例对象的接口
    static ChatService* instance();


    //处理登录业务
    void login(const TcpConnectionPtr&conn,json&js,Timestamp time);
    //处理注册业务
    void reg(const TcpConnectionPtr&conn,json&js,Timestamp time);
    //一对一聊天业务
    void oneChat(const TcpConnectionPtr&conn,json&js,Timestamp time);
    //添加好友业务
    void addFriend(const TcpConnectionPtr&conn,json&js,Timestamp time);

    //创建群聊的业务
    void creatGroup(const TcpConnectionPtr&conn,json&js,Timestamp time);
    //加入群组业务
    void addGroup(const TcpConnectionPtr&conn,json&js,Timestamp time);
    //群组聊天业务
    void groupChat(const TcpConnectionPtr&conn,json&js,Timestamp time);

    //获得消息对应的处理器
    MsgHandler getHandler(int msgid);
    //处理客户端异常退出
    void clientCloseException(const TcpConnectionPtr&conn);
    //服务器异常退出,业务重置方法
    void reset();
    //处理客户端注销业务
    void logOut(const TcpConnectionPtr&conn,json&js,Timestamp time);
    //redis回调函数
    void handleRedisSubscribeMessage(int,string);
private:
    ChatService();
    //存储消息id和其对应的业务处理方法
    unordered_map<int,MsgHandler> _msgHandlerMap;
    //存储在线用户的通信链接
    unordered_map<int,TcpConnectionPtr> _userconnMap;
    //定义互斥锁,保护_userconnMap的安全
    mutex _connMutex;
    //数据操作类
    UserModel _userModel;
    OfflineMsgModel _offlineMsgModel;
    friendModel _friendModel;
    GroupModel _groupModel;

    Redis _redis;//redis操作对象
};

#endif