#include <vector>
#include <iostream>
#include "../json.hpp"
using namespace std;
using json = nlohmann::json;

void func1()
{
    // 1. 普通数据序列化

    json js;

    // 添加数组
    js["num"] = {1, 2, 3, 4, 5};

    // 添加key-value
    js["name"] = "zhang san";

    // 添加对象
    js["msg"]["zhang san"] = "I'm back, brothers!";

    js["msg"]["li si"] = "children, how are you ?";

    // 上面等同于下面这句一次性添加数组对象

    js["msg"] = {{"zhang san", "I'm back, brothers!"}, {"li si", "children, how are you ?"}};

    //{"msg":{"li si":"children, how are you ?","zhang san":"I'm back, brothers!"},"name":"zhang san","num":[1,2,3,4,5]}
    cout << js << endl;

    // 反序列化为 Json 格式.

    string str = js.dump();
    cout << "JsonStr:" << str << endl;

    // 模拟从网络接收到json字符串，通过 json::parse 函数把 json 字符串转为 json 对象
    json js2 = json::parse(str);

    // 直接取key-value
    string name = js2["name"];

    cout << "name: " << name << endl; // name: zhang san
}

void func2()
{
    // 2. 容器序列化
    json js;

    // 直接序列化一个vector容器
    vector<int> vec;
    vec.push_back(1);
    vec.push_back(22);
    vec.push_back(333);
    js["Array"] = vec;

    // 直接序列化一个map容器
    map<int, string> mp;

    mp.insert({1, "清华"});
    mp.insert({2, "复旦"});
    mp.insert({3, "中财"});
    js["path"] = mp;

    cout << js << endl;

    string str = js.dump();
    cout << "JsonStr:" << str << endl;

    json js2 = json::parse(str);

    cout << endl;

    // 直接反序列化vector容器
    vector<int> v = js2["Array"];

    for (int val : v)
    {
        cout << val << " "; // 1 22 333
    }
    cout << endl;

    // 直接反序列化map容器
    map<int, string> mp2 = js2["path"];
    for (auto p : mp2)
    {
        cout << p.first << " " << p.second << endl;
    }
    /*
        1 清华
        2 复旦
        3 中财
     */

    cout << endl;
}
int main()
{
    func1();
    // func2();

    return 0;
}