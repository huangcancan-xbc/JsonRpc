#include <iostream>
#include <time.h>

// // 1. 
// int main()
// {
//     printf("%s\n", "Hello, world!");

//     return 0;
// }



// // 2.
// #define LOG(msg) printf("%s\n", msg)
// int main()
// {
//     LOG("hello, world");

//     return 0;
// }



// // 3.
// #define LOG(format, msg) printf(format "\n", msg)
// int main()
// {
//     LOG("%d", 123);

//     return 0;
// }



// // 4.
// #define LOG(format, msg) printf("[%s:%d] " format "\n", __FILE__, __LINE__, msg)
// int main()
// {
//     LOG("%d", 123);

//     return 0;
// }



// // 5.
// #define LOG(format, msg) {\
//     time_t t=time(NULL);\
//     struct tm *lt =localtime(&t);\
//     char time_tmp[32]={0};\
//     strftime(time_tmp, 31, "%m-%d %T", lt);\
//     printf("[%s][%s:%d] " format "\n", time_tmp, __FILE__, __LINE__, msg);\
// }

// int main()
// {
//     LOG("%d", 123);

//     return 0;
// }



// // 6.
// #define LOG(format, ...) {\
//     time_t t=time(NULL);\
//     struct tm *lt =localtime(&t);\
//     char time_tmp[32]={0};\
//     strftime(time_tmp, 31, "%m-%d %T", lt);\
//     printf("[%s][%s:%d] " format "\n", time_tmp, __FILE__, __LINE__, ##__VA_ARGS__);\
// }

// int main()
// {
//     LOG("hello");

//     return 0;
// }




// 7.
#include <sstream>
#include <chrono>
#include <random>
#include <string>
#include <sstream>
#include <atomic>
#include <iomanip>

#define LDBG 0
#define LINF 1
#define LERR 2

// #define LDEFAULT LDBG
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

int main()
{
    // DLOG("hello");

    std::stringstream ss;

    // 1. 构造一个机器随机数对象
    std::random_device rd;

    // 2. 以机器随机数种子初始化一个伪随机数对象
    std::mt19937 generator(rd());

    // 3. 构造限定数据范围的对象
    std::uniform_int_distribution<int> distribution(0, 255);

    for (int i = 0; i < 8;i++)
    {
        if(i==4||i==6)
        {
            ss << "-";
        }

        ss << std::setw(2) << std::setfill('0') << std::hex << distribution(generator);
    }

    ss << "-";
    static std::atomic<size_t> seq(1);
    size_t cur = seq.fetch_add(1);

    for (int i=7; i>=0; i--)
    {
        if(i==5)
        {
            ss << "-";
        }

        ss << std::setw(2) << std::setfill('0') << std::hex << ((cur >> (i * 8)) & 0xff);
    }

    std::cout << ss.str() << std::endl;

    return 0;
}