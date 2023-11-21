#pragma once

#include"user.hpp"
//群组用户，多一个role角色信息，从User中继承
class GroupUser:public User
{
public:
    void setRole(string role){this->role=role;}
    string getRole(){return this->role;}
private:
    string role;
};