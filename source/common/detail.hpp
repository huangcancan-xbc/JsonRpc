/*
    实现项目中用到的一些琐碎功能代码
    * 日志宏的定义
    * json的序列化和反序列化
    * uuid的生成
*/
#pragma once
#include <cstdio>
#include <ctime>
#include <iostream>
#include <sstream>
#include <memory>
#include <string>
#include <jsoncpp/json/json.h>
#include <sstream>
#include <chrono>
#include <random>   // 随机数生成器
#include <atomic>
#include <iomanip>  // 格式化输出

namespace rpc
{
    #define LDBG 0
    #define LINF 1
    #define LERR 2

    #define LDEFAULT LINF

    #define LOG(level, format, ...) {\
        if(level >= LDEFAULT)\
        {\
            time_t t = time(NULL);\
            struct tm *lt = localtime(&t);\
            char time_tmp[32] = {0};\
            strftime(time_tmp, 31, "%m-%d %T", lt);\
            fprintf(stdout, "[%s][%s:%d] " format "\n", time_tmp, __FILE__, __LINE__, ##__VA_ARGS__);\
        }\
    }

    #define DLOG(format, ...) LOG(LDBG, format, ##__VA_ARGS__);
    #define ILOG(format, ...) LOG(LINF, format, ##__VA_ARGS__);
    #define ELOG(format, ...) LOG(LERR, format, ##__VA_ARGS__);


    class JSON
    {
    public:
        // 实现 Json::Value 序列化为字符串
        static bool serialize(const Json::Value &val, std::string &body)
        {
            std::stringstream ss;                                          // 输出流，用于接收 JSON 字符串结果
            Json::StreamWriterBuilder swb;                                 // 创建序列化配置器
            swb["emitUTF8"] = true;                                        // 允许输出 UTF-8 中文，不转义
            std::unique_ptr<Json::StreamWriter> sw(swb.newStreamWriter()); // 创建序列化对象

            int ret = sw->write(val, &ss); // 将 JSON 对象写入流中
            if (ret != 0)
            {
                ELOG("json 序列化失败！");
                return false;
            }

            body = ss.str(); // 获取序列化后的字符串
            return true;
        }

        // 实现字符串反序列化为 Json::Value
        static bool unserialize(const std::string &body, Json::Value &val)
        {
            Json::CharReaderBuilder crb;                               // 创建反序列化配置器
            std::string errs;                                          // 存放错误信息
            std::unique_ptr<Json::CharReader> cr(crb.newCharReader()); // 创建反序列化对象

            bool ret = cr->parse(body.c_str(), body.c_str() + body.size(), &val, &errs);
            if (ret == false)
            {
                ELOG("json 反序列化失败: %s", errs.c_str());
                return false;
            }

            return true;
        }
    };


    class UUID
    {
    public:
        static std::string uuid()
        {
            std::stringstream ss;

            // 1. 构造一个机器随机数对象
            std::random_device rd;

            // 2. 以机器随机数种子初始化一个伪随机数对象（基于梅森旋转算法，mt19937）
            // 用rd()生成的真随机数作为种子，确保每次程序运行时，生成的随机序列不同
            std::mt19937 generator(rd());

            // 3. 构造限定数据范围的对象
            // 0-255是1个字节（8位二进制）的取值范围，方便后续转换为2个十六进制字符
            std::uniform_int_distribution<int> distribution(0, 255);

            // 生成UUID前半部分（8个随机字节，对应16个十六进制字符），循环8次，每次生成1个随机字节（0-255）
            for (int i = 0; i < 8; i++)
            {
                if (i == 4 || i == 6) // 在第4、6个字节前插入分隔符"-"，模拟UUID的分段格式
                {
                    ss << "-";
                }

                // 将随机数转换为十六进制字符串：std::hex：以十六进制输出，std::setw(2)：确保输出占2个字符位置，std::setfill('0')：如果不足2位，用'0'补位（如5→"05"，255→"ff"）
                ss << std::setw(2) << std::setfill('0') << std::hex << distribution(generator);
            }

            ss << "-";                         // 随机部分和自增序号之间需要一个分隔符"-"
            static std::atomic<size_t> seq(1); // 定义静态原子自增变量seq（初始值1），用于生成唯一序号（无数据竞争，序号不重复）
            size_t cur = seq.fetch_add(1);     // 获取当前序号值，然后自增1（fetch_add：先取值后加1，原子操作），每次调用cur都是唯一的，保证序号不重复

            // 生成UUID后半部分（8字节自增序号，对应16个十六进制字符），循环8次，每次提取序号cur的1个字节（从高位到低位）
            for (int i = 7; i >= 0; i--)
            {
                if (i == 5)
                {
                    ss << "-";
                }

                // 提取序号cur的第i个字节：(cur >> (i * 8))：将cur右移i*8位，把第i个字节移到最低8位，& 0xff：与11111111（二进制）按位与，保留最低8位（即第i个字节的值）
                ss << std::setw(2) << std::setfill('0') << std::hex << ((cur >> (i * 8)) & 0xff);
            }

            return ss.str();
        }
    };
}