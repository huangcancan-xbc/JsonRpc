#include <iostream>
#include <sstream>
#include <memory>
#include <string>
#include <jsoncpp/json/json.h>

// 实现 Json::Value 序列化为字符串
bool serialize(const Json::Value &val, std::string &body)
{
    std::stringstream ss;                                          // 输出流，用于接收 JSON 字符串结果
    Json::StreamWriterBuilder swb;                                 // 创建序列化配置器
    swb["emitUTF8"] = true;                                        // 允许输出 UTF-8 中文，不转义
    std::unique_ptr<Json::StreamWriter> sw(swb.newStreamWriter()); // 创建序列化对象

    int ret = sw->write(val, &ss); // 将 JSON 对象写入流中
    if (ret != 0)
    {
        std::cout << "json 序列化失败！" << std::endl;
        return false;
    }

    body = ss.str(); // 获取序列化后的字符串
    return true;
}

// 实现字符串反序列化为 Json::Value
bool unserialize(const std::string &body, Json::Value &val)
{
    Json::CharReaderBuilder crb;                               // 创建反序列化配置器
    std::string errs;                                          // 存放错误信息
    std::unique_ptr<Json::CharReader> cr(crb.newCharReader()); // 创建反序列化对象

    bool ret = cr->parse(body.c_str(), body.c_str() + body.size(), &val, &errs);
    if (ret == false)
    {
        std::cout << "json 反序列化失败！" << std::endl;
        return false;
    }

    return true;
}

int main()
{
    // 创建一个 JSON 对象
    const char *name = "小明";
    int age = 18;
    const char *gender = "男";
    float score[3] = {100, 99.9, 60.55};

    Json::Value student;
    student["姓名"] = name;   // 添加字符串键值对
    student["年龄"] = age;    // 添加整数键值对
    student["性别"] = gender; // 添加字符串键值对
    student["成绩"].append(score[0]);
    student["成绩"].append(score[1]);
    student["成绩"].append(score[2]);

    // 创建嵌套对象
    Json::Value fav;
    fav["书籍"] = "剑指 offer";
    fav["游戏"] = "火影忍者";

    student["爱好"] = fav;

    std::string body;
    serialize(student, body);
    std::cout << "序列化结果：\n" << body << std::endl;

    std::string str = R"({"姓名":"小红", "年龄":20,"成绩":[66.6,88,55.5]})";
    Json::Value stu;
    bool ret = unserialize(str, stu);
    if (ret == false)
    {
        return -1;
    }
    std::cout << "姓名：" << stu["姓名"].asString() << std::endl;
    std::cout << "年龄：" << stu["年龄"].asInt() << std::endl;
    int size = stu["成绩"].size();
    for (int i = 0; i < size; i++)
    {
        std::cout << "成绩：" << stu["成绩"][i].asFloat() << std::endl;
    }

    return 0;
}