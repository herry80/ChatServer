#include"friendmodel.hpp"
#include"db.h"

// 添加好友关系
void friendModel::insert(int userid, int friendid)
{
    //组装sql语句
    char sql[1024]={0};
    sprintf(sql,"insert into Friend values(%d,%d)",userid,friendid);
    //创建MySQL对象
    MySQL mysql;
    if(mysql.connect())
    {
        mysql.update(sql);
    }
}
// 返回用户的好友列表
vector<User> friendModel::query(int userid)
{
    //组装sql语句
    char sql[1024]={0};
    sprintf(sql,"select User.id,User.name,User.state from User,Friend where Friend.friendid=User.id and Friend.userid=%d",userid);
    vector<User> msg;
    //创建MySQL对象
    MySQL mysql;
    if(mysql.connect())
    {
        MYSQL_RES *res=mysql.query(sql);
        if(res!=nullptr)
        {
            MYSQL_ROW row;
            while((row=mysql_fetch_row(res))!=nullptr){
                User user;
                user.setId(atoi(row[0]));
                user.setName(row[1]);
                user.setState(row[2]);
                msg.push_back(user);
            }
            mysql_free_result(res);
            return msg;
        }
    }
    return msg;
}