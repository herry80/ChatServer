#include"chatservice.hpp"
#include"public.hpp"
#include<muduo/base/Logging.h>
using namespace muduo;
//获取单例对象的接口
ChatService* ChatService::instance()
{
    static ChatService service;
    return &service;
}
//处理登录业务
void ChatService::login(const TcpConnectionPtr&conn,json&js,Timestamp time)
{
    //LOG_INFO<<"do login service!!!";
    //获取id和password
    int id=js["id"];
    string pwd=js["password"];

    User user=_userModel.query(id);
    if(user.getId()==id&&user.getPassword()==pwd){
        if(user.getState()=="online")
        {
            //当前用户已登录
            json response;
            response["msgid"] = LOGIN_MSG_ACK;
            response["errno"] = 2;
            response["errmsg"]="this accout is using,input another!!";
            conn->send(response.dump());
        }
        else{
            //登录成功，记录用户连接信息
            {
                lock_guard<mutex> mt(_connMutex);
                _userconnMap.insert({id,conn});
            }
            //id用户登录成功后，向redis订阅channel
            _redis.subscribe(id);
            // 登录成功 还要把状态信息改变 offline->online
            user.setState("online");
            _userModel.updateState(user);//修改状态
            json response;
            response["msgid"] = LOGIN_MSG_ACK;
            response["errno"] = 0; // 0代表没错
            response["id"] = user.getId();
            response["name"] = user.getName();
            //检查是否有离线消息
            auto msg=_offlineMsgModel.query(id);
            if(!msg.empty())
            {
                response["offlinemsg"]=msg;
                _offlineMsgModel.remove(id);//删掉离线消息
            }
            
            //查询该用户的好友信息并返回
            auto userVec=_friendModel.query(id);
            if(!userVec.empty())//有好友
            {
                vector<string> vec;
                for(auto &user:userVec)
                {
                    json js;
                    js["id"]=user.getId();
                    js["name"]=user.getName();
                    js["state"]=user.getState();
                    vec.push_back(js.dump());
                }
                response["friends"]=vec;
            }
            //查询该用户的群组信息并返回
            auto groupvec=_groupModel.queryGroups(id);
            if(!groupvec.empty())
            {
                vector<string> vec;
                for(auto& group:groupvec)
                {
                    json js;
                    js["groupid"]=group.getId();
                    js["groupname"]=group.getName();
                    js["groupdesc"]=group.getDesc();
                    if(!group.getUsers().empty()){
                        vector<string> vec2;
                        for (auto &groupuser : group.getUsers())
                        {
                            json grpjs;
                            grpjs["id"] = groupuser.getId();
                            grpjs["name"] = groupuser.getName();
                            grpjs["state"] = groupuser.getState();
                            grpjs["role"] = groupuser.getRole();
                            vec2.push_back(grpjs.dump());
                        }
                        js["users"] = vec2;
                    }
                    vec.push_back(js.dump());
                }
                response["groups"]=vec;
            }
            conn->send(response.dump());
        }
         
    }
    else{
        //登录失败
        json response;
        response["msgid"]=LOGIN_MSG_ACK;
        response["errno"]=1;
        response["errmsg"]="username or password error!!";
        conn->send(response.dump());
    }
    
}
//处理注册业务
void ChatService::reg(const TcpConnectionPtr&conn,json&js,Timestamp time)
{
    //LOG_INFO<<"do reg service!!!";
    string name=js["name"];
    string pwd=js["password"];
    //创建USer对象
    User user;
    user.setName(name);
    user.setPassword(pwd);
    //插入数据库
    bool state=_userModel.insert(user);
    if(state){
        //注册成功
        json response;
        response["msgid"]=REG_MSG_ACK;
        response["errno"]=0;//0代表没错
        response["id"]=user.getId();//用户id带回去
        string resbuf=response.dump();
        conn->send(resbuf);
    }
    else{
        //注册失败
       json response;
        response["msgid"]=REG_MSG_ACK;
        response["errno"]=1;
        string resbuf=response.dump();
        conn->send(resbuf);
    }
}
//注册消息id以及对应的回调操作
ChatService::ChatService()
{
    _msgHandlerMap.insert({LOGIN_MSG,std::bind(&ChatService::login,this,_1,_2,_3)});
    _msgHandlerMap.insert({REG_MSG,std::bind(&ChatService::reg,this,_1,_2,_3)});
    _msgHandlerMap.insert({ONE_CHAT_MSG,std::bind(&ChatService::oneChat,this,_1,_2,_3)});
    _msgHandlerMap.insert({ADD_FRIEND_MSG,std::bind(&ChatService::addFriend,this,_1,_2,_3)});
    _msgHandlerMap.insert({CREATE_GROUP_MSG,std::bind(&ChatService::creatGroup,this,_1,_2,_3)});
    _msgHandlerMap.insert({ADD_GROUP_MSG,std::bind(&ChatService::addGroup,this,_1,_2,_3)});
    _msgHandlerMap.insert({GROUP_CHAT_MSG,std::bind(&ChatService::groupChat,this,_1,_2,_3)});
    _msgHandlerMap.insert({LOGOUT_MSG,std::bind(&ChatService::logOut,this,_1,_2,_3)});

    if(_redis.connect())
    {
        _redis.init_notify_handler(bind(&ChatService::handleRedisSubscribeMessage,this,_1,_2));
    }
}
//获得消息对应的处理器
MsgHandler ChatService::getHandler(int msgid)
{
    auto it=_msgHandlerMap.find(msgid);
    if(it==_msgHandlerMap.end())//没找到
    {
        //返回一个默认处理器 空操作
        return [=](const TcpConnectionPtr&conn,json&js,Timestamp time){
            LOG_ERROR<<"msgid:"<<msgid<<"can not find Handler!";
        };
    }
    return _msgHandlerMap[msgid];
}
//服务器异常退出
void ChatService::reset()
{
    //重置用户的状态信息，offline->online
    _userModel.resetState();
}
//处理客户端异常退出
void ChatService::clientCloseException(const TcpConnectionPtr &conn)
{
    //从_userconnMap表中删除信息
    User user;
    {
        lock_guard<mutex> mt(_connMutex);
        for (auto it = _userconnMap.begin(); it != _userconnMap.end(); ++it)
        {
            if (it->second == conn)
            {
                user.setId(it->first);
                _userconnMap.erase(it);
                break;
            }
        }
    }
    //在redis中取消订阅通道
    _redis.unsubscribe(user.getId());
    //更新用户状态
    if(user.getId()!=-1){
        user.setState("offline");
        _userModel.updateState(user);
    }
}

//一对一聊天业务  //规定客户端json格式为：{msgid:5,id:1,name:"zhangfei"，toid:3,msg:"xxxxxx",time:....}
void ChatService::oneChat(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    cout<<"你好中古"<<endl;
    int toid=js["toid"].get<int>();
    {
        lock_guard<mutex> lock(_connMutex);
        auto it = _userconnMap.find(toid);
        if (it != _userconnMap.end())
        {
             //在线.转发消息
             it->second->send(js.dump());
             return;
        }
    }
    //查询toid是否在线,在其他服务器上
    User user=_userModel.query(toid);
    if(user.getState()=="online"){
        _redis.publish(toid,js.dump());
        return;
    }
    // 不在线 存储离线消息
    _offlineMsgModel.insert(toid,js.dump());
}
//添加好友业务  规定消息格式 {msgid:6,id:1,frendid:2}
void ChatService::addFriend(const TcpConnectionPtr&conn,json&js,Timestamp time)
{
    int userid=js["id"].get<int>();
    int friendid=js["friendid"].get<int>();

    //可以判断该用户存不存在
    //存储好友信息
    _friendModel.insert(userid,friendid);
}


//创建群聊的业务  {msgid:7,id:1,groupname:"",groupdesc:work}
void ChatService::creatGroup(const TcpConnectionPtr&conn,json&js,Timestamp time)
{
    int userid=js["id"].get<int>();
    string name=js["groupname"];
    string desc=js["groupdesc"];

    //存储创建的群组信息
    Group group(-1,name,desc);
    if(_groupModel.createGroup(group))
    {
        //存储创建人信息
        _groupModel.addGroup(userid,group.getId(),"creator");
    }
}
 //加入群组业务 {msgid:8,id:1,groupid:1}
void ChatService::addGroup(const TcpConnectionPtr&conn,json&js,Timestamp time)
{
    int userid=js["id"].get<int>();
    int groupid=js["groupid"].get<int>();
    _groupModel.addGroup(userid,groupid,"normal");
}
//群组聊天业务 {msgid:9,:"id":1,"groupid":1,"msg":"hello world!"};
void ChatService::groupChat(const TcpConnectionPtr&conn,json&js,Timestamp time)
{
    int userid=js["id"].get<int>();
    int groupid=js["groupid"].get<int>();
    vector<int> useridVec=_groupModel.queryGroupUser(userid,groupid);
    lock_guard<mutex> lock(_connMutex);
    for(int id:useridVec)
    {
        auto it=_userconnMap.find(id);
        if(it!=_userconnMap.end())
        {
            //转发消息
            it->second->send(js.dump());
        }
        else{
            User user=_userModel.query(id);
            if(user.getState()=="online"){
                _redis.publish(id,js.dump());
            }
            else{
                //存储离线消息
                _offlineMsgModel.insert(id,js.dump());
            }
        }
    }
}
//处理客户端注销业务
void ChatService::logOut(const TcpConnectionPtr&conn,json&js,Timestamp time)
{
    int id=js["id"].get<int>();
     //从_userconnMap表中删除信息
    {
        lock_guard<mutex> mt(_connMutex);
        auto it=_userconnMap.find(id);
        if(it!=_userconnMap.end()){
            _userconnMap.erase(it);
        }
    }
    //在redis中取消订阅通道
    _redis.unsubscribe(id);
    //更新用户状态
    User user(id,"","","offline");
     _userModel.updateState(user);
}
//从redis消息队列中获得订阅的消息
void ChatService::handleRedisSubscribeMessage(int userid,string msg)
{
    lock_guard<mutex> lock(_connMutex);
    auto it=_userconnMap.find(userid);
    if(it!=_userconnMap.end())
    {
        it->second->send(msg);
        return;
    }
    //存储用户离线消息
    _offlineMsgModel.insert(userid,msg);
}