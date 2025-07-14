#ifndef USER_H
#define USER_H
#include <string>
using namespace std;
class User
{ 
public:
    User(int id = -1, string name = "", string pwd = "", string state = "offline")
    {
        this->id = id;
        this->name = name;
        this->password = pwd;
        this->state = state;
    }
    void SetId(int id){this->id = id;}
    void SetName(string name){this->name = name;}
    void SetPwd(string pwd){this->password = pwd;}
    void SetState(string state){this->state = state;}

    int GetId(){return id;}
    string GetName(){return name;}
    string GetPwd(){return password;}
    string GetState(){return state;}


private:
    int id;
    string name;
    string password;
    string state;   
};


#endif