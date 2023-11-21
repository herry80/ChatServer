#include"db.h"
#include"offlinemessagemodel.hpp"
// 存储用户离线消息
void OfflineMsgModel::insert(int userid, string msg)
{
     //组装sql语句
    char sql[1024]={0};
    sprintf(sql,"insert into OfflineMessage values(%d,'%s')",userid,msg.c_str());
    //创建MySQL对象
    MySQL mysql;
    if(mysql.connect())
    {
        mysql.update(sql);
    }
}
// 删除用户的离线消息
void OfflineMsgModel::remove(int userid)
{
    //组装sql语句
    char sql[1024]={0};
    sprintf(sql,"delete from OfflineMessage where userid=%d",userid);
    //创建MySQL对象
    MySQL mysql;
    if(mysql.connect())
    {
        mysql.update(sql);
    }
}
// 查询用户的离线消息
vector<string> OfflineMsgModel::query(int userid)
{
    //组装sql语句
    char sql[1024]={0};
    sprintf(sql,"select message from OfflineMessage where userid=%d",userid);
    vector<string> msg;
    //创建MySQL对象
    MySQL mysql;
    if(mysql.connect())
    {
        MYSQL_RES *res=mysql.query(sql);
        if(res!=nullptr)
        {
            MYSQL_ROW row;
            while((row=mysql_fetch_row(res))!=nullptr){
                msg.push_back(row[0]);
            }
            mysql_free_result(res);
            return msg;
        }
    }
    return msg;
}
