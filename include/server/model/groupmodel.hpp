#pragma once

#include"group.hpp"
#include<string>
#include<vector>
using namespace std;

//维护群组信息的操作接口方法
class GroupModel
{
public:
    //创建群组
    bool createGroup(Group &group);
    //加入群组
    void addGroup(int userid,int groupid,string role);
    //查询用户所在的群组
    vector<Group> queryGroups(int userid);
    //根据指定的groupid查询群组用户组id列表，除了自己
    vector<int> queryGroupUser(int userid,int groupid);
};