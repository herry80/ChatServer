#include"groupmodel.hpp"
#include"db.h"

// 创建群组
bool GroupModel::createGroup(Group &group)
{
    //组装sql语句
    char sql[1024]={0};
    sprintf(sql,"insert into AllGroup(groupname,groupdesc) values('%s','%s')",group.getName().c_str(),group.getDesc().c_str());
    //创建MySQL对象
    MySQL mysql;
    if(mysql.connect())
    {
        if(mysql.update(sql))
        {
            group.setId(mysql_insert_id(mysql.getConnection()));
            return true;
        }
    }
    return false;
}
// 加入群组
void GroupModel::addGroup(int userid, int groupid, string role)
{
     //组装sql语句
    char sql[1024]={0};
    sprintf(sql,"insert into GroupUser values(%d,%d,'%s')",groupid,userid,role.c_str());
    //创建MySQL对象
    MySQL mysql;
    if(mysql.connect())
    {
        mysql.update(sql);
    }
}
// 查询用户所在的群组信息
vector<Group> GroupModel::queryGroups(int userid)
{
    /*
    1.根据userid在groupuser表中查询出该用户所属的群组信息
    2.再根据群组信息，查询属于改组所有用户的userid,并且和user表进行多表联合查询，查出用户的详细信息
    */
     char sql[1024]={0};
    sprintf(sql,"select a.id,a.groupname,a.groupdesc from AllGroup a inner join GroupUser b on a.id=b.groupid where b.userid=%d",userid);
    vector<Group> msg;
    //创建MySQL对象
    MySQL mysql;
    if(mysql.connect())
    {
        MYSQL_RES *res=mysql.query(sql);
        if(res!=nullptr)
        {
            MYSQL_ROW row;
            while((row=mysql_fetch_row(res))!=nullptr){
                Group group;
                group.setId(atoi(row[0]));
                group.setName(row[1]);
                group.setDesc(row[2]);
                msg.push_back(group);
            }
            mysql_free_result(res);
        }
    }
    memset(sql,0,sizeof(sql));
    //查询群组用户信息
    for(Group&group:msg)
    {
        sprintf(sql,"select a.id,a.name,a.state,b.grouprole from User a inner join GroupUser b on b.userid=a.id where b.groupid=%d",
        group.getId());
        MYSQL_RES *res=mysql.query(sql);
        if(res!=nullptr)
        {
            MYSQL_ROW row;
            while((row=mysql_fetch_row(res))!=nullptr)
            {
                GroupUser groupuser;
                groupuser.setId(atoi(row[0]));
                groupuser.setName(row[1]);
                groupuser.setState(row[2]);
                groupuser.setRole(row[3]);
                group.getUsers().push_back(groupuser);
            }
            mysql_free_result(res);
        }
    }
    return msg;
}
// 根据指定的groupid查询群组用户组id列表，除了自己
vector<int> GroupModel::queryGroupUser(int userid, int groupid)
{
    char sql[1024]={0};
    sprintf(sql,"select userid from GroupUser where groupid=%d and userid!=%d",groupid,userid);
    vector<int> msg;
    //创建MySQL对象
    MySQL mysql;
    if(mysql.connect())
    {
        MYSQL_RES *res=mysql.query(sql);
        if(res!=nullptr)
        {
            MYSQL_ROW row;
            while((row=mysql_fetch_row(res))!=nullptr){
                msg.push_back(atoi(row[0]));
            }
            mysql_free_result(res);
        }
    }
    return msg;
}