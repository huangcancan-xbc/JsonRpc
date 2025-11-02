#include <iostream>
#include <jsoncpp/json/json.h>
#include <vector>
#include <sstream>
#include <memory>

int main()
{
    Json::Value stu;           // 构造一个 JSON 对象 stu，用于存放学生信息
    stu["name"] = "zhangsan";  // 字符串类型键值
    stu["age"] = 18;           // 整型键值
    stu["socre"].append(100);  // 数组类型，追加第 1 个分数
    stu["socre"].append(99.9); // 追加第 2 个分数
    stu["socre"].append(60);   // 追加第 3 个分数

    // 创建一个数组类型的 Json::Value
    Json::Value arr;
    arr.append(1);
    arr.append(2);
    arr.append(3);
    stu["arr"] = arr; // 把数组对象赋值给 stu["arr"]

    // // 序列化即转成字符串
    // Json::StreamWriterBuilder writer;
    // std::string json_str = Json::writeString(writer, stu);
    // std::cout << json_str << std::endl;  // 打印 JSON 内容
    Json::StreamWriterBuilder write;                                 //  创建一个JSON字符串生成器的配置工具（配置器，相当于告诉工人"怎么生成字符串"的规则手册）
    std::unique_ptr<Json::StreamWriter> sw(write.newStreamWriter()); // 创建一个JSON字符串生成器对象（工人），并用智能指针管理它

    std::stringstream ss;          // 创建一个既能读又能写的流
    int ret = sw->write(stu, &ss); // 不能直接返回一个string，只能写到一个输出流中，所以让sw这个工人把stu对象的内容写进流中
    if (ret != 0)
    {
        std::cout << "序列化失败！" << std::endl;
        return -1;
    }
    // 输出序列化结果（此处键会按字典序排序）
    std::cout << "序列化结果：\n"
              << ss.str() << std::endl;

    std::string str = ss.str();                                // 把序列化后的字符串取出，准备反序列化
    Json::Value root;                                          // 创建一个空的JSON对象（root），用来存放反序列化后的结果
    Json::CharReaderBuilder crb;                               // 创建一个JSON字符串解析器的配置器（相当于告诉解析工人"怎么解析字符串"的规则手册）
    std::unique_ptr<Json::CharReader> cr(crb.newCharReader()); // 根据配置器创建一个实际的JSON解析器对象（工人），并用智能指针管理它

    // 解析字符串str，将结果存入root中，参数说明：
    // 前两个参数：指定要解析的字符串范围（从首地址到末尾），第三个参数：解析结果存放的JSON对象（root）
    // 第四个参数：错误信息输出（这里传nullptr表示忽略错误详情），返回值ret1为true表示解析成功，false表示失败
    // parse(起始地址，结束地址，解析结果存放处，错误信息输出);
    bool ret1 = cr->parse(str.c_str(), str.c_str() + str.size(), &root, nullptr);
    if (!ret1)
    {
        std::cout << "反序列化失败！" << std::endl;
        return -1;
    }

    // std::cout << "反序列化的结果：\n"
    //           << root["name"].asString() << std::endl
    //           << root["age"].asInt() << std::endl
    //           << root["socre"][0].asFloat() << " "
    //           << root["socre"][1].asFloat() << " "
    //           << root["socre"][2].asFloat() << std::endl
    //           << root["arr"][0].asInt() << " "
    //           << root["arr"][1].asInt() << " "
    //           << root["arr"][2].asInt() << std::endl;
    std::cout << "反序列化的结果：" << std::endl;
    std::cout << root["name"].asString() << std::endl; // 输出 name 字段
    std::cout << root["age"].asInt() << std::endl;     // 输出 age 字段

    // 输出 socre 数组中的内容
    std::cout << root["socre"][0].asFloat() << " "
              << root["socre"][1].asFloat() << " "
              << root["socre"][2].asFloat() << std::endl;

    // 输出 arr 数组中的内容
    std::cout << root["arr"][0].asInt() << " "
              << root["arr"][1].asInt() << " "
              << root["arr"][2].asInt() << std::endl;

    return 0;
}