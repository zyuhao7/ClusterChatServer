#include "json.hpp"
#include "group.hpp"
#include "user.hpp"
#include "public.hpp"
#include <iostream>
#include <thread>
#include <string>
#include <vector>
#include <cstring>
#include <cstdlib>
#include <chrono>
#include <ctime>
#include <unordered_map>
#include <functional>
#include <sstream>
using namespace std;
using json = nlohmann::json;

#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <semaphore.h>
#include <atomic>

// 记录当前系统登录的用户信息
User g_CurrentUser;
// 记录当前登录用户的好友信息
vector<User> g_CurrentUserFriendsList;
// 记录当前登录用户的群组消息列表
vector<Group> g_CurrentUserGroupsList;

// 控制主菜单页面程序
bool isMainMenuRunning = false;

// 用于读写线程之间的通信
sem_t rwsem;
// 记录登录状态
atomic_bool g_isLoginSuccess{false};
atomic_ullong g_requestIdCounter{1};

string nextRequestId()
{
    unsigned long long id = g_requestIdCounter.fetch_add(1);
    std::ostringstream oss;
    oss << "req-" << id;
    return oss.str();
}

void setProtocolMeta(json &js, int msgid)
{
    js["version"] = CHAT_PROTOCOL_VERSION;
    js["msgid"] = msgid;
    js["request_id"] = nextRequestId();
}

// 接收线程
void readTaskHandler(int clientfd);
// 获取系统时间（聊天信息需要添加时间信息）
string getCurrentTime();
// 主聊天页面程序
void mainMenu(int);
// 显示当前登录成功用户的基本信息
void showCurrentUserInfo();

// 聊天客户端程序实现，main线程用作发送线程，子线程用作接收线程
int main(int argc, char **argv)
{
    if (argc < 3)
    {
        cerr << "command invalid! example: ./ChatClient 127.0.0.1 6000" << endl;
        exit(-1);
    }

    // 解析通过命令行参数传递的ip和port
    char *ip = argv[1];
    uint16_t port = atoi(argv[2]);

    // 创建client端的socket
    int clientfd = socket(AF_INET, SOCK_STREAM, 0);
    if (-1 == clientfd)
    {
        cerr << "socket create error" << endl;
        exit(-1);
    }

    // 填写 client 需要连接的 server 信息 ip + port
    sockaddr_in server;
    memset(&server, 0, sizeof(sockaddr_in));

    server.sin_family = AF_INET;
    server.sin_port = htons(port);
    server.sin_addr.s_addr = inet_addr(ip);

    // client和server进行连接
    if (-1 == connect(clientfd, (sockaddr *)&server, sizeof(sockaddr_in)))
    {
        cerr << "connect server error" << endl;
        close(clientfd);
        exit(-1);
    }

    // 初始化读写线程通信用的信号量
    sem_init(&rwsem, 0, 0);

    // 连接服务器成功，启动接收子线程
    std::thread readTask(readTaskHandler, clientfd); // pthread_create
    readTask.detach();                               // pthread_detach

    // main线程用于接收用户输入，负责发送数据
    for (;;)
    {
        // 显示首页面菜单 登录、注册、退出
        cout << "========================" << endl;
        cout << "1. Login:>" << endl;
        cout << "2. Register:>" << endl;
        cout << "3. Quit:>" << endl;
        cout << "========================" << endl;
        cout << "Choice:>";
        int choice = 0;
        cin >> choice;
        cin.get(); // 读掉缓冲区残留的回车

        switch (choice)
        {
        case 1: // 登录业务
        {
            int id = 0;
            char pwd[50] = {0};
            cout << "Userid:";
            cin >> id;
            cin.get(); // 读掉缓冲区残留的回车
            cout << "UserPassword:";
            cin.getline(pwd, 50);

            json js;
            setProtocolMeta(js, LOGIN_MSG);
            js["id"] = id;
            js["password"] = pwd;
            string request = js.dump(); // 将json对象序列化为字符串

            g_isLoginSuccess = false;

            int len = send(clientfd, request.c_str(), strlen(request.c_str()) + 1, 0);
            if (len == -1)
            {
                cerr << "send login msg error:" << request << endl;
            }

            sem_wait(&rwsem); // 等待信号量，由子线程处理完登录的响应消息后，通知这里

            if (g_isLoginSuccess)
            {
                // 进入聊天主菜单页面
                isMainMenuRunning = true;
                mainMenu(clientfd);
            }
        }
        break;
        case 2: // Register 业务
        {
            char name[50] = {0};
            char pwd[50] = {0};
            cout << "Username:";
            cin.getline(name, 50);
            cout << "UserPassword:";
            cin.getline(pwd, 50);

            json js;
            setProtocolMeta(js, REG_MSG);
            js["name"] = name;
            js["password"] = pwd;
            string request = js.dump();

            int len = send(clientfd, request.c_str(), strlen(request.c_str()) + 1, 0);
            if (len == -1)
            {
                cerr << "send reg msg error:" << request << endl;
            }

            sem_wait(&rwsem); // 等待信号量，子线程处理完注册消息会通知
        }
        break;
        case 3: // quit业务
            close(clientfd);
            sem_destroy(&rwsem);
            exit(0);
        default:
            cerr << "invalid input!" << endl;
            break;
        }
    }

    return 0;
}

// 处理注册的响应逻辑
void doRegResponse(json &response)
{
    if (0 != response["errno"].get<int>()) // 注册失败
    {
        cerr << "register error: " << response.value("errmsg", "unknown error") << endl;
    }
    else // 注册成功
    {
        cout << "name register success, userid is " << response["id"]
             << ", do not forget it!" << endl;
    }
}

// 处理登录的响应逻辑
void doLoginResponse(json &response)
{
    // 登录失败
    if (0 != response["errno"].get<int>())
    {
        cerr << response["errmsg"] << endl;
        g_isLoginSuccess = false; // 登录失败
    }
    else // 登陆成功
    {
        // 1. 记录当前用户的 id 和 name
        g_CurrentUser.SetId(response["id"].get<int>());
        g_CurrentUser.SetName(response["name"]);

        // 2. 记录当前用户的好友列表
        if (response.contains("friends"))
        {
            // 初始化
            g_CurrentUserFriendsList.clear();

            vector<string> vec = response["friends"];
            for (string &str : vec)
            {
                json js = json::parse(str);
                User user;
                user.SetId(js["id"].get<int>());
                user.SetName(js["name"]);
                user.SetState(js["state"]);
                g_CurrentUserFriendsList.push_back(user);
            }
        }
        // 3.记录当前用户的群组列表信息
        if (response.contains("groups"))
        {
            // 初始化
            g_CurrentUserGroupsList.clear();

            vector<string> vec1 = response["groups"];
            for (string &str : vec1)
            {
                json js = json::parse(str);
                Group group;
                group.SetId(js["id"].get<int>());
                group.SetName(js["groupname"]);
                group.SetDesc(js["groupdesc"]);

                vector<string> vec2 = js["users"];
                for (string &user : vec2)
                {
                    GroupUser g_user;
                    json js = json::parse(user); // 解析每个群组中的用户信息
                    g_user.SetId(js["id"].get<int>());
                    g_user.SetName(js["name"]);
                    g_user.SetState(js["state"]);
                    g_user.SetRole(js["role"]);
                    group.GetUsers().push_back(g_user);
                }
                g_CurrentUserGroupsList.push_back(group);
            }
        }
        // 显示当前登录成功用户的基本信息
        showCurrentUserInfo();
        // 显示当前用户的离线消息  个人聊天信息或者群组消息
        if (response.contains("offlinemsg"))
        {
            vector<string> vec = response["offlinemsg"];
            for (string &str : vec)
            {
                json js = json::parse(str);
                // time + [id] + name + " said: " + xxx
                if (ONE_CHAT_MSG == js["msgid"].get<int>())
                {
                    cout << js["time"].get<string>() << " [" << js["id"] << "]" << js["name"].get<string>()
                         << " said: " << js["msg"].get<string>() << endl;
                }
                else
                {
                    cout << "群消息[" << js["groupid"] << "]:" << js["time"].get<string>() << " [" << js["id"] << "]" << js["name"].get<string>()
                         << " said: " << js["msg"].get<string>() << endl;
                }
            }
        }
        g_isLoginSuccess = true;
    }
}

// 处理添加好友响应逻辑
void doAddFriendResponse(json &response)
{
    if (0 != response["errno"].get<int>())
    {
        cerr << "add friend failed: " << response["errmsg"].get<string>() << endl;
        return;
    }

    cout << "add friend success";
    if (response.contains("friendid"))
    {
        cout << ", friendid: " << response["friendid"].get<int>();
    }
    if (response.contains("friendname"))
    {
        cout << ", friendname: " << response["friendname"].get<string>();
    }
    cout << endl;
}

void doGenericAck(const string &title, json &response)
{
    int err = response.value("errno", 1);
    if (err == 0)
    {
        cout << title << " success";
        if (response.contains("groupid"))
        {
            cout << ", groupid: " << response["groupid"].get<int>();
        }
    if (response.contains("message_id"))
    {
        cout << ", message_id: " << response["message_id"].get<long long>();
    }
    if (response.value("duplicate", false))
    {
        cout << ", duplicate: true";
    }
    if (response.contains("deliver_count"))
    {
        cout << ", deliver_count: " << response["deliver_count"].get<int>();
    }
        cout << endl;
    }
    else
    {
        cerr << title << " failed: " << response.value("errmsg", "unknown error") << endl;
    }
}

void doHistoryResponse(json &response)
{
    if (response.value("errno", ERR_MESSAGE_HISTORY_INVALID_SCOPE) != ERR_OK)
    {
        cerr << "history query failed: " << response.value("errmsg", "unknown error") << endl;
        return;
    }

    cout << "history query success";
    if (response.contains("scope"))
    {
        cout << ", scope: " << response["scope"].get<string>();
    }
    if (response.contains("order"))
    {
        cout << ", order: " << response["order"].get<string>();
    }
    if (response.contains("targetid"))
    {
        cout << ", targetid: " << response["targetid"].get<int>();
    }
    if (response.contains("groupid"))
    {
        cout << ", groupid: " << response["groupid"].get<int>();
    }
    cout << endl;

    vector<string> history = response.value("history", vector<string>{});
    for (const string &item : history)
    {
        json js = json::parse(item);
        if (response.value("scope", string("direct")) == "group")
        {
            cout << js.value("created_at", "") << " [group " << js.value("group_id", 0)
                 << "] sender=" << js.value("sender_id", 0)
                 << " recalled=" << js.value("recalled", 0)
                 << " message=" << js.value("message", "") << endl;
        }
        else
        {
            cout << js.value("created_at", "") << " sender=" << js.value("sender_id", 0)
                 << " receiver=" << js.value("receiver_id", 0)
                 << " recalled=" << js.value("recalled", 0)
                 << " message=" << js.value("message", "") << endl;
        }
    }
}

void doSearchUserResponse(json &response)
{
    if (response.value("errno", ERR_USER_SEARCH_KEYWORD_EMPTY) != ERR_OK)
    {
        cerr << "search user failed: " << response.value("errmsg", "unknown error") << endl;
        return;
    }

    cout << "search user success";
    if (response.contains("keyword"))
    {
        cout << ", keyword: " << response["keyword"].get<string>();
    }
    cout << endl;

    vector<string> users = response.value("users", vector<string>{});
    for (const string &item : users)
    {
        json js = json::parse(item);
        cout << "user id=" << js.value("id", 0)
             << ", name=" << js.value("name", "")
             << ", state=" << js.value("state", "offline")
             << ", is_friend=" << (js.value("is_friend", false) ? "true" : "false")
             << ", has_blocked=" << (js.value("has_blocked", false) ? "true" : "false")
             << ", blocked_by_target=" << (js.value("blocked_by_target", false) ? "true" : "false")
             << endl;
    }
}

// 子线程-接收线程
void readTaskHandler(int clientfd)
{
    // 接收线程不断接收来自server的消息
    for (;;)
    {
        // 接收数据
        char buffer[1024] = {0};
        int len = recv(clientfd, buffer, sizeof(buffer), 0);
        if (len == -1 || len == 0)
        {
            cerr << "recv error, clientfd:" << clientfd << endl;
            close(clientfd);
            exit(-1);
        }

        // 接收成功，处理数据
        json js = json::parse(buffer);
        if (js.value("version", CHAT_PROTOCOL_VERSION) != CHAT_PROTOCOL_VERSION)
        {
            cerr << "protocol version mismatch: " << js.value("version", -1) << endl;
            continue;
        }
        int msgtype = js["msgid"].get<int>();
        // 根据消息类型做不同的处理
        if (ONE_CHAT_MSG == msgtype)
        {
            // 私聊消息
            cout << js["time"].get<string>() << " [" << js["id"].get<int>() << "] " << js["name"].get<string>()
                 << " said: " << js["msg"].get<string>();
            if (js.contains("message_id"))
            {
                cout << " (message_id=" << js["message_id"].get<long long>() << ")";
            }
            if (js.contains("read_state"))
            {
                cout << " [" << js["read_state"].get<string>() << "]";
            }
            cout << endl;
            continue;
        }
        else if (GROUP_CHAT_MSG == msgtype)
        {
            // 群聊消息
            cout << "群消息[" << js["groupid"].get<int>() << "]: "
                 << js["time"].get<string>() << " [" << js["id"].get<int>() << "] "
                 << js["name"].get<string>() << " said: " << js["msg"].get<string>();
            if (js.contains("message_id"))
            {
                cout << " (message_id=" << js["message_id"].get<long long>() << ")";
            }
            cout << endl;
            continue;
        }
        else if (LOGIN_MSG_ACK == msgtype)
        {
            // 登录响应消息
            doLoginResponse(js);
            sem_post(&rwsem); // 通知主线程登录响应已处理完毕
            continue;
        }
        else if (REG_MSG_ACK == msgtype)
        {
            // 注册响应消息
            doRegResponse(js);
            sem_post(&rwsem); // 通知主线程注册响应已处理完毕
            continue;
        }
        else if (ADD_FRIEND_MSG_ACK == msgtype)
        {
            // 添加好友响应消息
            doAddFriendResponse(js);
            continue;
        }
        else if (CREATE_GROUP_MSG_ACK == msgtype)
        {
            doGenericAck("create group", js);
            continue;
        }
        else if (ADD_GROUP_MSG_ACK == msgtype)
        {
            doGenericAck("add group", js);
            continue;
        }
        else if (GROUP_CHAT_MSG_ACK == msgtype)
        {
            doGenericAck("group chat", js);
            continue;
        }
        else if (ONE_CHAT_MSG_ACK == msgtype)
        {
            doGenericAck("one chat", js);
            continue;
        }
        else if (LOGINOUT_MSG_ACK == msgtype)
        {
            doGenericAck("logout", js);
            continue;
        }
        else if (LEAVE_GROUP_MSG_ACK == msgtype)
        {
            doGenericAck("leave group", js);
            continue;
        }
        else if (SET_GROUP_ROLE_MSG_ACK == msgtype)
        {
            doGenericAck("set group role", js);
            continue;
        }
        else if (MARK_READ_MSG_ACK == msgtype)
        {
            doGenericAck("mark read", js);
            continue;
        }
        else if (RECALL_MSG_ACK == msgtype)
        {
            doGenericAck("recall message", js);
            continue;
        }
        else if (RECALL_NOTIFY_MSG == msgtype)
        {
            cout << "message recalled, message_id=" << js["message_id"].get<long long>()
                 << ", operator=" << js["operator_id"].get<int>() << endl;
            continue;
        }
        else if (QUERY_HISTORY_MSG_ACK == msgtype)
        {
            doHistoryResponse(js);
            continue;
        }
        else if (SEARCH_USER_MSG_ACK == msgtype)
        {
            doSearchUserResponse(js);
            continue;
        }
        else if (ADD_BLACKLIST_MSG_ACK == msgtype)
        {
            doGenericAck("add blacklist", js);
            continue;
        }
        else if (REMOVE_BLACKLIST_MSG_ACK == msgtype)
        {
            doGenericAck("remove blacklist", js);
            continue;
        }
        else if (SET_USER_STATE_MSG_ACK == msgtype)
        {
            if (js.value("errno", ERR_USER_STATE_INVALID) == ERR_OK && js.contains("state"))
            {
                g_CurrentUser.SetState(js["state"].get<string>());
            }
            doGenericAck("set status", js);
            continue;
        }
    }
}

// 显示当前登录成功用户的基本信息
void showCurrentUserInfo()
{
    cout << "======================Login User======================" << endl;
    cout << "Current Login User => Id:" << g_CurrentUser.GetId() << " Name:" << g_CurrentUser.GetName() << endl;
    cout << "----------------------Friend List---------------------" << endl;
    if (!g_CurrentUserFriendsList.empty())
    {
        for (User &user : g_CurrentUserFriendsList)
        {
            cout << user.GetId() << " " << user.GetName() << " " << user.GetState() << endl;
        }
    }
    cout << "----------------------Group List----------------------" << endl;
    if (!g_CurrentUserGroupsList.empty())
    {
        for (Group &group : g_CurrentUserGroupsList)
        {
            cout << group.GetId() << " " << group.GetName() << " " << group.GetDesc() << endl;
            for (GroupUser &user : group.GetUsers())
            {
                cout << user.GetId() << " " << user.GetName() << " " << user.GetState()
                     << " " << user.GetRole() << endl;
            }
        }
    }
    cout << "======================================================" << endl;
}
// "help" command handler
void help(int fd = 0, string str = "");
// "chat" command handler
void chat(int, string);
// "addfriend" command handler
void addfriend(int, string);
// "creategroup" command handler
void creategroup(int, string);
// "addgroup" command handler
void addgroup(int, string);
// "groupchat" command handler
void groupchat(int, string);
// "loginout" command handler
void loginout(int, string);
// "leavegroup" command handler
void leavegroup(int, string);
// "setrole" command handler
void setrole(int, string);
// "readmsg" command handler
void readmsg(int, string);
// "recall" command handler
void recallmsg(int, string);
// "history" command handler
void historymsg(int, string);
// "grouphistory" command handler
void grouphistory(int, string);
// "searchuser" command handler
void searchuser(int, string);
// "blockuser" command handler
void blockuser(int, string);
// "unblockuser" command handler
void unblockuser(int, string);
// "setstatus" command handler
void setstatus(int, string);

// 系统支持的客户端命令列表
unordered_map<string, string> commandMap = {
    {"help", "显示所有支持的命令,格式help"},
    {"chat", "一对一聊天,格式chat:friendid:message"},
    {"addfriend", "添加好友,格式addfriend:friendid"},
    {"creategroup", "创建群组,格式creategroup:groupname:groupdesc"},
    {"addgroup", "加入群组,格式addgroup:groupid"},
    {"leavegroup", "退出群组,格式leavegroup:groupid"},
    {"setrole", "设置群成员角色,格式setrole:groupid:userid:normal|admin"},
    {"groupchat", "群聊,格式groupchat:groupid:message"},
    {"history", "查询私聊历史,格式history:targetid[:limit[:offset[:asc|desc]]]"},
    {"grouphistory", "查询群历史,格式grouphistory:groupid[:limit[:offset[:asc|desc]]]"},
    {"searchuser", "搜索用户,格式searchuser:keyword[:limit[:offset]]"},
    {"blockuser", "加入黑名单,格式blockuser:userid"},
    {"unblockuser", "移出黑名单,格式unblockuser:userid"},
    {"setstatus", "设置当前状态,格式setstatus:online|busy"},
    {"readmsg", "标记消息已读,格式readmsg:message_id"},
    {"recall", "撤回消息,格式recall:message_id[:toid|groupid]"},
    {"loginout", "注销,格式loginout"}};

// 注册系统支持的客户端命令处理
unordered_map<string, function<void(int, string)>> commandHandlerMap = {
    {"help", help},
    {"chat", chat},
    {"addfriend", addfriend},
    {"creategroup", creategroup},
    {"addgroup", addgroup},
    {"leavegroup", leavegroup},
    {"setrole", setrole},
    {"groupchat", groupchat},
    {"history", historymsg},
    {"grouphistory", grouphistory},
    {"searchuser", searchuser},
    {"blockuser", blockuser},
    {"unblockuser", unblockuser},
    {"setstatus", setstatus},
    {"readmsg", readmsg},
    {"recall", recallmsg},
    {"loginout", loginout}};

// 主聊天页面程序
void mainMenu(int clientfd)
{
    help();
    char buffer[1024] = {0};
    while (isMainMenuRunning)
    {
        cin.getline(buffer, sizeof(buffer));
        string commandbuf(buffer);
        string command;                 // 存储命令
        int idx = commandbuf.find(":"); // 找到命令和参数之间的分隔符
        if (idx != -1)
        {
            command = commandbuf.substr(0, idx); // 提取命令
        }
        else
        {
            command = commandbuf; // 没有参数的命令
        }
        auto it = commandHandlerMap.find(command);
        if (it == commandHandlerMap.end())
        {
            cout << "invalid cmd, please try again" << endl;
            continue;
        }
        // 执行对应的命令处理函数
        it->second(clientfd, commandbuf.substr(idx + 1));
    }
}

// "help" command handler
void help(int, string)
{
    cout << "show command list >>> " << endl;
    for (auto &p : commandMap)
    {
        cout << p.first << " : " << p.second << endl;
    }
    cout << endl;
}

// "addfriend" command handler
void addfriend(int clientfd, string str)
{
    int friendid = atoi(str.c_str());
    json js;
    setProtocolMeta(js, ADD_FRIEND_MSG);
    js["id"] = g_CurrentUser.GetId(); // 当前登录用户id
    js["friendid"] = friendid;

    string request = js.dump();
    int len = send(clientfd, request.c_str(), strlen(request.c_str()) + 1, 0);
    if (len == -1)
    {
        cerr << "send add friend msg error: " << request << endl;
    }
    else
    {
        cout << "Add friend request sent to server." << endl;
    }
}

// "chat" command handler
void chat(int clientfd, string str)
{
    int idx = str.find(":"); // friendid:message
    if (idx == -1)
    {
        cerr << "invalid chat command format, please use: chat:friendid:message" << endl;
        return;
    }
    int friendid = atoi(str.substr(0, idx).c_str());
    string msg = str.substr(idx + 1);
    json js;
    setProtocolMeta(js, ONE_CHAT_MSG);
    js["id"] = g_CurrentUser.GetId(); // 当前登录用户id
    js["name"] = g_CurrentUser.GetName();
    js["toid"] = friendid;
    js["msg"] = msg;
    js["time"] = getCurrentTime();

    string request = js.dump();
    int len = send(clientfd, request.c_str(), strlen(request.c_str()) + 1, 0);
    if (len == -1)
    {
        cerr << "send chat msg error: " << request << endl;
    }
}

// "creategroup" command handler  groupname:groupdesc
void creategroup(int clientfd, string str)
{
    int idx = str.find(":"); // groupname:groupdesc
    if (idx == -1)
    {
        cerr << "invalid creategroup command format, please use: creategroup:groupname:groupdesc" << endl;
        return;
    }
    string groupname = str.substr(0, idx);
    string groupdesc = str.substr(idx + 1);
    json js;
    setProtocolMeta(js, CREATE_GROUP_MSG);
    js["id"] = g_CurrentUser.GetId(); // 当前登录用户id
    js["groupname"] = groupname;
    js["groupdesc"] = groupdesc;

    string request = js.dump();
    int len = send(clientfd, request.c_str(), strlen(request.c_str()) + 1, 0);
    if (len == -1)
    {
        cerr << "send create group msg error: " << request << endl;
    }
}

// "addgroup" command handler
void addgroup(int clientfd, string str)
{
    int groupid = atoi(str.c_str());
    json js;
    setProtocolMeta(js, ADD_GROUP_MSG);
    js["id"] = g_CurrentUser.GetId(); // 当前登录用户id
    js["groupid"] = groupid;

    string request = js.dump();
    int len = send(clientfd, request.c_str(), strlen(request.c_str()) + 1, 0);
    if (len == -1)
    {
        cerr << "send add group msg error: " << request << endl;
    }
}

// "groupchat" command handler   groupid:message
void groupchat(int clientfd, string str)
{
    int idx = str.find(":"); // groupid:message
    if (idx == -1)
    {
        cerr << "invalid groupchat command format, please use: groupchat:groupid:message" << endl;
        return;
    }
    int groupid = atoi(str.substr(0, idx).c_str());
    string msg = str.substr(idx + 1);
    json js;
    setProtocolMeta(js, GROUP_CHAT_MSG);
    js["id"] = g_CurrentUser.GetId(); // 当前登录用户id
    js["name"] = g_CurrentUser.GetName();
    js["groupid"] = groupid;
    js["msg"] = msg;
    js["time"] = getCurrentTime();

    string request = js.dump();
    int len = send(clientfd, request.c_str(), strlen(request.c_str()) + 1, 0);
    if (len == -1)
    {
        cerr << "send group chat msg error: " << request << endl;
    }
}

// "loginout" command handler
void loginout(int clientfd, string)
{
    json js;
    setProtocolMeta(js, LOGINOUT_MSG);
    js["id"] = g_CurrentUser.GetId(); // 当前登录用户id

    string request = js.dump();
    int len = send(clientfd, request.c_str(), strlen(request.c_str()) + 1, 0);
    if (len == -1)
    {
        cerr << "send loginout msg error: " << request << endl;
    }
    else
    {
        isMainMenuRunning = false; // 退出主菜单页面
        cout << "Login out successfully!" << endl;
    }
}

void leavegroup(int clientfd, string str)
{
    int groupid = atoi(str.c_str());
    json js;
    setProtocolMeta(js, LEAVE_GROUP_MSG);
    js["id"] = g_CurrentUser.GetId();
    js["groupid"] = groupid;

    string request = js.dump();
    if (send(clientfd, request.c_str(), strlen(request.c_str()) + 1, 0) == -1)
    {
        cerr << "send leave group msg error: " << request << endl;
    }
}

void setrole(int clientfd, string str)
{
    int first = str.find(":");
    int second = str.find(":", first + 1);
    if (first == -1 || second == -1)
    {
        cerr << "invalid setrole format, please use: setrole:groupid:userid:normal|admin" << endl;
        return;
    }
    int groupid = atoi(str.substr(0, first).c_str());
    int targetid = atoi(str.substr(first + 1, second - first - 1).c_str());
    string role = str.substr(second + 1);

    json js;
    setProtocolMeta(js, SET_GROUP_ROLE_MSG);
    js["id"] = g_CurrentUser.GetId();
    js["groupid"] = groupid;
    js["targetid"] = targetid;
    js["role"] = role;
    string request = js.dump();
    if (send(clientfd, request.c_str(), strlen(request.c_str()) + 1, 0) == -1)
    {
        cerr << "send set role msg error: " << request << endl;
    }
}

void readmsg(int clientfd, string str)
{
    long long message_id = atoll(str.c_str());
    json js;
    setProtocolMeta(js, MARK_READ_MSG);
    js["id"] = g_CurrentUser.GetId();
    js["message_id"] = message_id;
    string request = js.dump();
    if (send(clientfd, request.c_str(), strlen(request.c_str()) + 1, 0) == -1)
    {
        cerr << "send mark read msg error: " << request << endl;
    }
}

void recallmsg(int clientfd, string str)
{
    int idx = str.find(":");
    long long message_id = 0;
    int target = -1;
    bool is_group = false;
    if (idx == -1)
    {
        message_id = atoll(str.c_str());
    }
    else
    {
        message_id = atoll(str.substr(0, idx).c_str());
        int second = str.find(":", idx + 1);
        if (second == -1)
        {
            target = atoi(str.substr(idx + 1).c_str());
        }
        else
        {
            string kind = str.substr(idx + 1, second - idx - 1);
            target = atoi(str.substr(second + 1).c_str());
            is_group = (kind == "groupid" || kind == "group");
        }
    }

    json js;
    setProtocolMeta(js, RECALL_MSG);
    js["id"] = g_CurrentUser.GetId();
    js["message_id"] = message_id;
    if (target > 0)
    {
        if (is_group)
        {
            js["groupid"] = target;
        }
        else
        {
            js["toid"] = target;
        }
    }

    string request = js.dump();
    if (send(clientfd, request.c_str(), strlen(request.c_str()) + 1, 0) == -1)
    {
        cerr << "send recall msg error: " << request << endl;
    }
}

vector<string> splitArgs(const string &text)
{
    vector<string> parts;
    size_t start = 0;
    while (start <= text.size())
    {
        size_t pos = text.find(':', start);
        if (pos == string::npos)
        {
            parts.push_back(text.substr(start));
            break;
        }
        parts.push_back(text.substr(start, pos - start));
        start = pos + 1;
    }
    return parts;
}

void historymsg(int clientfd, string str)
{
    vector<string> parts = splitArgs(str);
    if (parts.empty() || parts[0].empty())
    {
        cerr << "invalid history format, please use: history:targetid[:limit[:offset[:asc|desc]]]" << endl;
        return;
    }

    int targetid = atoi(parts[0].c_str());
    int limit = parts.size() > 1 && !parts[1].empty() ? atoi(parts[1].c_str()) : 20;
    int offset = parts.size() > 2 && !parts[2].empty() ? atoi(parts[2].c_str()) : 0;
    string order = parts.size() > 3 && !parts[3].empty() ? parts[3] : "desc";

    json js;
    setProtocolMeta(js, QUERY_HISTORY_MSG);
    js["id"] = g_CurrentUser.GetId();
    js["targetid"] = targetid;
    js["limit"] = limit;
    js["offset"] = offset;
    js["order"] = order;

    string request = js.dump();
    if (send(clientfd, request.c_str(), strlen(request.c_str()) + 1, 0) == -1)
    {
        cerr << "send history query error: " << request << endl;
    }
}

void grouphistory(int clientfd, string str)
{
    vector<string> parts = splitArgs(str);
    if (parts.empty() || parts[0].empty())
    {
        cerr << "invalid grouphistory format, please use: grouphistory:groupid[:limit[:offset[:asc|desc]]]" << endl;
        return;
    }

    int groupid = atoi(parts[0].c_str());
    int limit = parts.size() > 1 && !parts[1].empty() ? atoi(parts[1].c_str()) : 20;
    int offset = parts.size() > 2 && !parts[2].empty() ? atoi(parts[2].c_str()) : 0;
    string order = parts.size() > 3 && !parts[3].empty() ? parts[3] : "desc";

    json js;
    setProtocolMeta(js, QUERY_HISTORY_MSG);
    js["id"] = g_CurrentUser.GetId();
    js["groupid"] = groupid;
    js["limit"] = limit;
    js["offset"] = offset;
    js["order"] = order;

    string request = js.dump();
    if (send(clientfd, request.c_str(), strlen(request.c_str()) + 1, 0) == -1)
    {
        cerr << "send group history query error: " << request << endl;
    }
}

void searchuser(int clientfd, string str)
{
    vector<string> parts = splitArgs(str);
    if (parts.empty() || parts[0].empty())
    {
        cerr << "invalid searchuser format, please use: searchuser:keyword[:limit[:offset]]" << endl;
        return;
    }

    int limit = parts.size() > 1 && !parts[1].empty() ? atoi(parts[1].c_str()) : 20;
    int offset = parts.size() > 2 && !parts[2].empty() ? atoi(parts[2].c_str()) : 0;

    json js;
    setProtocolMeta(js, SEARCH_USER_MSG);
    js["id"] = g_CurrentUser.GetId();
    js["keyword"] = parts[0];
    js["limit"] = limit;
    js["offset"] = offset;

    string request = js.dump();
    if (send(clientfd, request.c_str(), strlen(request.c_str()) + 1, 0) == -1)
    {
        cerr << "send search user msg error: " << request << endl;
    }
}

void blockuser(int clientfd, string str)
{
    int targetid = atoi(str.c_str());
    json js;
    setProtocolMeta(js, ADD_BLACKLIST_MSG);
    js["id"] = g_CurrentUser.GetId();
    js["targetid"] = targetid;

    string request = js.dump();
    if (send(clientfd, request.c_str(), strlen(request.c_str()) + 1, 0) == -1)
    {
        cerr << "send add blacklist msg error: " << request << endl;
    }
}

void unblockuser(int clientfd, string str)
{
    int targetid = atoi(str.c_str());
    json js;
    setProtocolMeta(js, REMOVE_BLACKLIST_MSG);
    js["id"] = g_CurrentUser.GetId();
    js["targetid"] = targetid;

    string request = js.dump();
    if (send(clientfd, request.c_str(), strlen(request.c_str()) + 1, 0) == -1)
    {
        cerr << "send remove blacklist msg error: " << request << endl;
    }
}

void setstatus(int clientfd, string str)
{
    if (str != "online" && str != "busy")
    {
        cerr << "invalid setstatus format, please use: setstatus:online|busy" << endl;
        return;
    }

    json js;
    setProtocolMeta(js, SET_USER_STATE_MSG);
    js["id"] = g_CurrentUser.GetId();
    js["state"] = str;

    string request = js.dump();
    if (send(clientfd, request.c_str(), strlen(request.c_str()) + 1, 0) == -1)
    {
        cerr << "send set user state msg error: " << request << endl;
    }
}

// 获取系统时间（聊天信息需要添加时间信息）
string getCurrentTime()
{
    auto tt = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    struct tm *ptm = localtime(&tt);
    char timeStr[60];
    sprintf(timeStr, "%d-%02d-%02d %02d:%02d:%02d",
            (int)ptm->tm_year + 1900,
            (int)ptm->tm_mon + 1,
            (int)ptm->tm_mday,
            (int)ptm->tm_hour,
            (int)ptm->tm_min,
            (int)ptm->tm_sec);

    return std::string(timeStr);
}
