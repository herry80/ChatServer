#pragma once
#include"user.hpp"
#include<vector>
using namespace std;
//维护好友信息的接口方法
class friendModel
{
public:
    //添加好友关系
    void insert(int userid,int friendid);
    //返回用户的好友列表
    vector<User> query(int userid);
};