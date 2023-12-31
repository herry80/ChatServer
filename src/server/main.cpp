#include"chatserver.hpp"
#include<iostream>
#include"chatservice.hpp"
#include<signal.h>
#include<unistd.h>
#include<sys/socket.h>
#include<sys/types.h>
#include<netinet/in.h>
#include<arpa/inet.h>
using namespace std;

void resetHandler(int)
{
    ChatService::instance()->reset();
    exit(0);
}
int main(int argc,char** argv)
{

    if(argc<3){
        cerr<<"command invalid example: ./chatclient 127.0.0.1 6000"<<endl;
        exit(-1);
    }
    //解析通过命令行参数传递的ip和port
    char *ip=argv[1];
    uint16_t port=atoi(argv[2]);
    signal(SIGINT,resetHandler);
    EventLoop loop;
    InetAddress Addr(ip,port);
    ChatServer _server(&loop,Addr,"server");
    
    _server.start();
    loop.loop();
    return 0;
}