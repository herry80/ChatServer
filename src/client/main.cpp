#include<iostream>
#include<thread>
#include<string>
#include<vector>
#include<chrono>
#include<ctime>
#include"json.hpp"
using json=nlohmann::json;
using namespace std;

#include<unistd.h>
#include<sys/socket.h>
#include<sys/types.h>
#include<netinet/in.h>
#include<arpa/inet.h>

#include"public.hpp"
#include"group.hpp"
#include"user.hpp"

//记录当前系统登录的用户信息
User g_currentUser;
//记录当前登录用户的好友列表
vector<User> g_currentUserFriendList;
//记录当前登录用户的群组列表信息
vector<Group> g_currentUserGroupList;

//控制聊天页面程序
bool isMainMenuRuning=false;

//显示当前登录成功用户的基本信息
void showCurrentUserData();
//接收线程
void readTaskHandler(int clientfd);
//获取系统时间(聊天信息需要添加时间信息)
string getCurrentTime();
//主聊天页面程序
void mainMenu(int clientfd);

//客户端实现，main线程用作发送线程，子线程用作接收线程
int main(int argc,char**argv)
{
    if(argc<3){
        cerr<<"command invalid example: ./chatclient 127.0.0.1 6000"<<endl;
        exit(-1);
    }
    //解析通过命令行参数传递的ip和port
    char *ip=argv[1];
    uint16_t port=atoi(argv[2]);
    //创建client端的socket
    int clientfd=socket(AF_INET,SOCK_STREAM,0);
    if(-1==clientfd){
        cerr<<"socket create error"<<endl;
        exit(-1);
    }
    //填写client需要链接的server信息ip+port
    sockaddr_in server;
    memset(&server,0,sizeof(sockaddr_in));
    server.sin_family=AF_INET;
    server.sin_port=htons(port);
    server.sin_addr.s_addr=inet_addr(ip);

    //client与server进行连接
    if(-1==connect(clientfd,(sockaddr*)&server,sizeof(sockaddr_in)))
    {
        cerr<<"connect server error"<<endl;
        close(clientfd);
        exit(-1);
    }
    //main线程用于接收用户输入，负责发送数据
    for(;;)
    {
        //显示首页菜单
        cout<<"==========================="<<endl;
        cout<<"1. login"<<endl;
        cout<<"2. regiser"<<endl;
        cout<<"3. quit"<<endl;
        cout<<"==========================="<<endl;
        cout<<"choice:";
        int choice=0;
        cin>>choice;
        //cin.get();//读掉残留的回车
        switch(choice)
        {
            case 1://login业务
            {
                int id=0;
                char pwd[50]={0};
                cout<<"userid:";
                cin>>id;
                cin.get();
                cout<<"userpassword:";
                cin.getline(pwd,50);

                json js;
                js["msgid"]=LOGIN_MSG;
                js["id"]=id;
                js["password"]=pwd;
                string request=js.dump();
                int len=send(clientfd,request.c_str(),strlen(request.c_str())+1,0);
                if(len==-1){
                    cerr<<"send login msg error:"<<request<<endl;
                }
                else{
                    char buffer[1024]={0};
                    len=recv(clientfd,buffer,1024,0);
                    if(len==-1){
                        cerr<<"recv reg response error!"<<endl;
                    }
                    else{
                        json responsejs=json::parse(buffer);
                        if(0!=responsejs["errno"].get<int>())
                        {
                            cerr<<js["errmsg"]<<endl;
                        }
                        else{
                            //登录成功
                            g_currentUser.setId(responsejs["id"].get<int>());
                            g_currentUser.setName(responsejs["name"]);
                            //记录当前用户的好友列表
                            if(responsejs.contains("friends"))
                            {
                                //初始化一下g_currentUserFriendList
                                g_currentUserFriendList.clear();
                                vector<string> vec=responsejs["friends"];
                                for(string &str:vec)
                                {
                                    json js=json::parse(str);
                                    User user;
                                    user.setId(js["id"].get<int>());
                                    user.setName(js["name"]);
                                    user.setState(js["state"]);
                                    g_currentUserFriendList.push_back(user);
                                }
                            }
                            //记录当前用户的群组列表信息
                            if(responsejs.contains("groups"))
                            {
                                //初始化一下g_currentUserGroupList
                                g_currentUserGroupList.clear();
                                vector<string> vec=responsejs["groups"];
                                for(string &groupstr:vec)
                                {
                                    json grpjs=json::parse(groupstr);
                                    Group group;
                                    group.setId(grpjs["groupid"].get<int>());
                                    group.setName(grpjs["groupname"]);
                                    group.setDesc(grpjs["groupdesc"]);

                                    vector<string> vec2=grpjs["users"];
                                    for(string &userstr:vec2)
                                    {
                                        GroupUser user;
                                        json js=json::parse(userstr);
                                        user.setId(js["id"].get<int>());
                                        user.setName(js["name"]);
                                        user.setState(js["state"]);
                                        user.setRole(js["role"]);
                                        group.getUsers().push_back(user);
                                    }
                                    g_currentUserGroupList.push_back(group);
                                }
                            }
                            //显示登录用户基本信息
                            showCurrentUserData();
                            //显示当前用户的离线消息，个人聊天消息或群组消息
                            if(responsejs.contains("offlinemsg"))
                            {
                                vector<string> vec=responsejs["offlinemsg"];
                                for(string &str:vec){
                                    json js=json::parse(str);
                                    //time+id+name+said:+xxxx
                                    if (js["msgid"].get<int>() == ONE_CHAT_MSG)
                                    {
                                        cout << js["time"].get<string>() << "[" << js["id"] << "]" << js["name"].get<string>() << "said: " << js["msg"].get<string>() << endl;
                                    }
                                    else if (js["msgid"].get<int>() == GROUP_CHAT_MSG)
                                    {
                                        cout << "群消息[" << js["groupid"] << "]" << js["time"].get<string>() << "[" << js["id"] << "]" << js["name"].get<string>() << "said: " << js["msg"].get<string>() << endl;
                                    }
                                }
                            }
                            //d登录成功，启动线程负责接收数据,只启动一次
                            static int readthreadnumber=0;
                            if(readthreadnumber==0){
                                thread readTask(readTaskHandler,clientfd);
                                readTask.detach();
                                readthreadnumber++;
                            }
                            //进入聊天主页面
                            isMainMenuRuning=true;
                            mainMenu(clientfd);
                        }
                    }
                }
                break;
            }
            case 2://注册业务
            {
                char name[50]={0};
                char pwd[50]={0};
                cout<<"username:";
                cin.getline(name,50);
                cout<<"userpassword:";
                cin.getline(pwd,50);

                json js;
                js["msgid"]=REG_MSG;
                js["name"]=name;
                js["password"]=pwd;
                string request=js.dump();
                int len=send(clientfd,request.c_str(),strlen(request.c_str())+1,0);
                if(len==-1){
                    cerr<<"send reg msg error:"<<request<<endl;
                }
                else{
                    char buffer[1024]={0};
                    len=recv(clientfd,buffer,1024,0);
                    if(-1==len){
                        cerr<<"recv reg response error!"<<endl;
                    }
                    else{
                        json responsejs=json::parse(buffer);
                        if(0!=responsejs["errno"].get<int>())
                        {
                            cerr<<name<<" is already exist,register error"<<endl;
                        }
                        else{
                            //注册成功
                            cout<<name <<"regidter success,userid id"<<responsejs["id"]<<", do not froget it!"<<endl;
                        }
                    }
                }
            }
            break;
            case 3://quit业务
            close(clientfd);
            exit(0);
            default:
            cout<<"invalid input"<<endl;
            //cout<<endl;
            break;
        }
    }
    return 0;
}
//显示当前登录成功用户的基本信息
void showCurrentUserData()
{
    cout<<"=========================login user============================"<<endl;
    cout<<"current login user =>id:"<<g_currentUser.getId()<<" name:"<<g_currentUser.getName()<<endl;
    cout<<"-------------------------friend list---------------------------"<<endl;
    if(!g_currentUserFriendList.empty())
    {
        for(User &user:g_currentUserFriendList)
        {
            cout<<user.getId()<<" "<<user.getName()<<" "<<user.getState()<<endl;
        }
    }
    cout<<"-------------------------group list---------------------------"<<endl;
    if(!g_currentUserGroupList.empty())
    {
        for(Group &group:g_currentUserGroupList)
        {
            cout<<group.getId()<<" "<<group.getName()<<" "<<group.getDesc()<<endl;
            for(auto &user:group.getUsers())
            {
                cout<<user.getId()<<" "<<user.getName()<<" "<<user.getState()<<" "<<user.getRole()<<endl;
            }
        }
    }
    cout<<"=============================================================="<<endl;
}
//获取系统时间(聊天信息需要添加时间信息)
string getCurrentTime()
{
    auto tt=std::chrono::system_clock::to_time_t(chrono::system_clock::now());
    struct tm*ptm=localtime(&tt);
    char date[60]={0};
    sprintf(date,"%d-%02d-%02d %02d:%02d:%02d",(int)ptm->tm_year+1900,(int)ptm->tm_mon+1,(int)ptm->tm_mday,
    (int)ptm->tm_hour,(int)ptm->tm_min,(int)ptm->tm_sec);
    return string(date);
}
//接收线程
void readTaskHandler(int clientfd)
{
    for(;;)
    {
        char buffer[1024]={0};
        int len=recv(clientfd,buffer,1024,0);
        if(len==-1||len==0){
            close(clientfd);
            exit(-1);
        }
        json js=json::parse(buffer);
        if(js["msgid"].get<int>()==ONE_CHAT_MSG){
            cout<<js["time"].get<string>()<<"["<<js["id"]<<"]"<<js["name"].get<string>()<<"said: "<<js["msg"].get<string>()<<endl;
            continue;
        }
        else if(js["msgid"].get<int>()==GROUP_CHAT_MSG)
        {
            cout<<"群消息["<<js["groupid"]<<"]"<<js["time"].get<string>()<<"["<<js["id"]<<"]"<<js["name"].get<string>()<<"said: "<<js["msg"].get<string>()<<endl;
            continue;
        }
    }
}

void help(int client=0,string str="");
void chat(int client,string str);
void creategroup(int client,string str);
void addgroup(int client,string str);
void groupchat(int client,string str);
void addfriend(int client,string str);
void quit(int client,string str);

//系统支持客户端命令列表
unordered_map<string,string> commandMap={
    {"help","显示所有支持的命令,格式help"},
    {"chat","一对一聊天,格式chat:friend:message"},
    {"addfriend","添加好友,格式addfriend:friendid"},
    {"creategroup","创建群组,格式creategroup:groupname:groupdesc"},
    {"addgroup","加入群组,格式addgroup:groupid"},
    {"groupchat","群聊,格式groupchat:groupid:message"},
    {"quit","注销,格式quit"}
};
//注册系统支持的客户端命令
unordered_map<string,function<void(int,string)>> commandHandlerMap={
    {"help",help},
    {"chat",chat},
    {"addfriend",addfriend},
    {"creategroup",creategroup},
    {"addgroup",addgroup},
    {"groupchat",groupchat},
    {"quit",quit}
};
//主聊天页面程序
void mainMenu(int clientfd)
{
    help();
    char buffer[1024]={0};
    while(isMainMenuRuning)
    {
        cin.getline(buffer,1024);
        string commandbuf(buffer);
        string command;//存储命令
        int idx=commandbuf.find(":");
        if(-1==idx){
            command=commandbuf;
        }
        else{
            command=commandbuf.substr(0,idx);
        }
        auto it=commandHandlerMap.find(command);
        if(it==commandHandlerMap.end()){
            cerr<<"invalid inout command!"<<endl;
            continue;
        }
        //调用相应的事件处理回调
        it->second(clientfd,commandbuf.substr(idx+1,commandbuf.size()-idx));
    }
}
void help(int client,string str)
{
    cout<<"show commandl list>>>"<<endl;
    for(auto& p:commandMap){
        cout<<p.first<<" : "<<p.second<<endl;
    }
    cout<<endl;
}
void addfriend(int client,string str)
{
    int friendid=atoi(str.c_str());
    json js;
    js["msgid"]=ADD_FRIEND_MSG;
    js["id"]=g_currentUser.getId();
    js["friendid"]=friendid;

    string buffer=js.dump();
    int len=send(client,buffer.c_str(),strlen(buffer.c_str())+1,0);
    if(-1==len){
        cerr<<"send addfriend msg error-> "<<buffer<<endl;
    }
}
void chat(int client,string str)
{
    int idx=str.find(":");
    if(-1==idx){
        cerr<<"chat command invalid!"<<endl;
        return;
    }
    int friendid=atoi(str.substr(0,idx).c_str());
    string message=str.substr(idx+1,str.size()-idx);

    json js;
    js["msgid"]=ONE_CHAT_MSG;
    js["id"]=g_currentUser.getId();
    js["name"]=g_currentUser.getName();
    js["toid"]=friendid;
    js["msg"]=message;
    js["time"]=getCurrentTime();
    string buffer=js.dump();

    int len=send(client,buffer.c_str(),strlen(buffer.c_str())+1,0);
    if(-1==len){
        cerr<<"send chat msg error->>"<<buffer<<endl;
    }
}
void creategroup(int client,string str)
{
    int idx=str.find(":");
    if(-1==idx){
        cerr<<"creategroup command invalid!"<<endl;
        return;
    }
    string groupname=str.substr(0,idx);
    string groupdesc=str.substr(idx+1,str.size()-idx);

    json js;
    js["msgid"]=CREATE_GROUP_MSG;
    js["id"]=g_currentUser.getId();
    js["groupname"]=groupname;
    js["groupdesc"]=groupdesc;
    string buffer=js.dump();

    int len=send(client,buffer.c_str(),strlen(buffer.c_str())+1,0);
    if(-1==len){
        cerr<<"send creategroup msg error->>"<<buffer<<endl;
    }
}
void addgroup(int client,string str)
{
    int groupid=atoi(str.c_str());

    json js;
    js["msgid"]=ADD_GROUP_MSG;
    js["id"]=g_currentUser.getId();
    js["groupid"]=groupid;
    string buffer=js.dump();

    int len=send(client,buffer.c_str(),strlen(buffer.c_str())+1,0);
    if(-1==len){
        cerr<<"send addgroup msg error->>"<<buffer<<endl;
    }
}
void groupchat(int client,string str)
{
    int idx=str.find(":");
    if(-1==idx){
        cerr<<"chatgroup command invalid!"<<endl;
        return;
    }
    int groupid=atoi(str.substr(0,idx).c_str());
    string message=str.substr(idx+1,str.size()-idx);
    //auto it=g_currentUserGroupList
    json js;
    js["msgid"]=GROUP_CHAT_MSG;
    js["id"]=g_currentUser.getId();
    js["name"]=g_currentUser.getName();
    js["groupid"]=groupid;
    js["msg"]=message;
    js["time"]=getCurrentTime();
    string buffer=js.dump();
    int len=send(client,buffer.c_str(),strlen(buffer.c_str())+1,0);
    if(-1==len){
        cerr<<"send chatgroup msg error->>"<<buffer<<endl;
    }
}
void quit(int client,string str)
{
    json js;
    js["msgid"]=LOGOUT_MSG;
    js["id"]=g_currentUser.getId();
    string buffer=js.dump();

    int len=send(client,buffer.c_str(),strlen(buffer.c_str())+1,0);
    if(-1==len){
        cerr<<"send quit msg error->>"<<buffer<<endl;
    }
    else{
        isMainMenuRuning=false;
    }
}