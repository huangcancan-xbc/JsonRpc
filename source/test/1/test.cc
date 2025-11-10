// #include <iostream>
// #include <time.h>

// // // 1. 
// // int main()
// // {
// //     printf("%s\n", "Hello, world!");

// //     return 0;
// // }



// // // 2.
// // #define LOG(msg) printf("%s\n", msg)
// // int main()
// // {
// //     LOG("hello, world");

// //     return 0;
// // }



// // // 3.
// // #define LOG(format, msg) printf(format "\n", msg)
// // int main()
// // {
// //     LOG("%d", 123);

// //     return 0;
// // }



// // // 4.
// // #define LOG(format, msg) printf("[%s:%d] " format "\n", __FILE__, __LINE__, msg)
// // int main()
// // {
// //     LOG("%d", 123);

// //     return 0;
// // }



// // // 5.
// // #define LOG(format, msg) {\
// //     time_t t=time(NULL);\
// //     struct tm *lt =localtime(&t);\
// //     char time_tmp[32]={0};\
// //     strftime(time_tmp, 31, "%m-%d %T", lt);\
// //     printf("[%s][%s:%d] " format "\n", time_tmp, __FILE__, __LINE__, msg);\
// // }

// // int main()
// // {
// //     LOG("%d", 123);

// //     return 0;
// // }



// // // 6.
// // #define LOG(format, ...) {\
// //     time_t t=time(NULL);\
// //     struct tm *lt =localtime(&t);\
// //     char time_tmp[32]={0};\
// //     strftime(time_tmp, 31, "%m-%d %T", lt);\
// //     printf("[%s][%s:%d] " format "\n", time_tmp, __FILE__, __LINE__, ##__VA_ARGS__);\
// // }

// // int main()
// // {
// //     LOG("hello");

// //     return 0;
// // }




// // 7.
// #include <sstream>
// #include <chrono>
// #include <random>
// #include <string>
// #include <sstream>
// #include <atomic>
// #include <iomanip>

// #define LDBG 0
// #define LINF 1
// #define LERR 2

// // #define LDEFAULT LDBG
// #define LDEFAULT LINF

// #define LOG(level, format, ...) {\
//     if(level >= LDEFAULT)\
//     {\
//         time_t t = time(NULL);\
//         struct tm *lt = localtime(&t);\
//         char time_tmp[32] = {0};\
//         strftime(time_tmp, 31, "%m-%d %T", lt);\
//         fprintf(stdout, "[%s][%s:%d] " format "\n", time_tmp, __FILE__, __LINE__, ##__VA_ARGS__);\
//     }\
// }
// #define DLOG(format, ...) LOG(LDBG, format, ##__VA_ARGS__);
// #define ILOG(format, ...) LOG(LINF, format, ##__VA_ARGS__);
// #define ELOG(format, ...) LOG(LERR, format, ##__VA_ARGS__);

// int main()
// {
//     // DLOG("hello");

//     std::stringstream ss;

//     // 1. 构造一个机器随机数对象
//     std::random_device rd;

//     // 2. 以机器随机数种子初始化一个伪随机数对象
//     std::mt19937 generator(rd());

//     // 3. 构造限定数据范围的对象
//     std::uniform_int_distribution<int> distribution(0, 255);

//     for (int i = 0; i < 8;i++)
//     {
//         if(i==4||i==6)
//         {
//             ss << "-";
//         }

//         ss << std::setw(2) << std::setfill('0') << std::hex << distribution(generator);
//     }

//     ss << "-";
//     static std::atomic<size_t> seq(1);
//     size_t cur = seq.fetch_add(1);

//     for (int i=7; i>=0; i--)
//     {
//         if(i==5)
//         {
//             ss << "-";
//         }

//         ss << std::setw(2) << std::setfill('0') << std::hex << ((cur >> (i * 8)) & 0xff);
//     }

//     std::cout << ss.str() << std::endl;

//     return 0;
// }





// #include "message.hpp"
// int main()
// {
//     // rpc::BaseMessage::ptr bmp = rpc::MessageFactory::create(rpc::MType::REQ_RPC);
//     // rpc::RpcRequest::ptr rrp =std::dynamic_pointer_cast<rpc::RpcRequest>(bmp);

//     rpc::RpcRequest::ptr rrp=rpc::MessageFactory::create<rpc::RpcRequest>();
//     Json::Value param;
//     param["num1"] = 11;
//     param["num2"] = 22;

//     rrp->setMethod("Add");
//     rrp->setParams(param);

//     std::string str=rrp->serialize();
//     std::cout << str << std::endl;

//     rpc::BaseMessage::ptr bmp = rpc::MessageFactory::create(rpc::MType::REQ_RPC);
//     bool ret=bmp->unserialize(str);
//     if(ret==false)
//     {
//         return -1;
//     }

//     ret = bmp->check();
//     if(ret==false)
//     {
//         return -1;
//     }

//     rpc::RpcRequest::ptr rrp2=std::dynamic_pointer_cast<rpc::RpcRequest>(bmp);
//     std::cout << rrp2->method() << std::endl;
//     std::cout << rrp2->params()["num1"].asInt() << std::endl;
//     std::cout << rrp2->params()["num2"].asInt() << std::endl;

//     return 0;
// }





// #include "message.hpp"
// int main()
// {
//     rpc::TopicRequest::ptr trp=rpc::MessageFactory::create<rpc::TopicRequest>();
//     trp->setTopicKey("news");
//     trp->setOptype(rpc::TopicOptype::TOPIC_PUBLISH);
//     trp->setTopicMsg("hello, world!");
//     std::string str=trp->serialize();
//     std::cout << str << std::endl;

//     rpc::BaseMessage::ptr bmp = rpc::MessageFactory::create(rpc::MType::REQ_TOPIC);
//     bool ret=bmp->unserialize(str);
//     if(ret==false)
//     {
//         return -1;
//     }
//     ret = bmp->check();
//     if(ret==false)
//     {
//         return -1;
//     }

//     rpc::TopicRequest::ptr trp2=std::dynamic_pointer_cast<rpc::TopicRequest>(bmp);
//     std::cout << trp2->topicKey() << std::endl;
//     std::cout << (int)trp2->optype() << std::endl;
//     std::cout << trp2->topicMsg() << std::endl;

//     return 0;
// }





// #include "message.hpp"
// int main()
// {
//     rpc::ServiceRequest::ptr trp = rpc::MessageFactory::create<rpc::ServiceRequest>();
//     trp->setMethod("Add");
//     trp->setOptype(rpc::ServiceOptype::SERVICE_DISCOVERY);
//     std::string str = trp->serialize();
//     std::cout << str << std::endl;

//     rpc::BaseMessage::ptr bmp = rpc::MessageFactory::create(rpc::MType::REQ_SERVICE);
//     bool ret = bmp->unserialize(str);
//     if (ret == false)
//     {
//         return -1;
//     }
//     ret = bmp->check();
//     if (ret == false)
//     {
//         return -1;
//     }

//     rpc::ServiceRequest::ptr trp2 = std::dynamic_pointer_cast<rpc::ServiceRequest>(bmp);
//     std::cout << trp2->method() << std::endl;
//     std::cout << (int)trp2->optype() << std::endl;

//     return 0;
// }





// #include "message.hpp"
// int main()
// {
//     rpc::ServiceRequest::ptr trp = rpc::MessageFactory::create<rpc::ServiceRequest>();
//     trp->setMethod("Add");
//     trp->setOptype(rpc::ServiceOptype::SERVICE_DISCOVERY);
//     trp->setHost(rpc::Address("127.0.0.1", 8080));
//     std::string str = trp->serialize();
//     std::cout << str << std::endl;

//     rpc::BaseMessage::ptr bmp = rpc::MessageFactory::create(rpc::MType::REQ_SERVICE);
//     bool ret = bmp->unserialize(str);
//     if (ret == false)
//     {
//         return -1;
//     }
//     ret = bmp->check();
//     if (ret == false)
//     {
//         return -1;
//     }

//     rpc::ServiceRequest::ptr trp2 = std::dynamic_pointer_cast<rpc::ServiceRequest>(bmp);
//     std::cout << trp2->method() << std::endl;
//     std::cout << (int)trp2->optype() << std::endl;
//     std::cout << trp2->host().first << std::endl;
//     std::cout << trp2->host().second << std::endl;

//     return 0;
// }





// #include "message.hpp"
// int main()
// {
//     rpc::RpcResponse::ptr trp = rpc::MessageFactory::create<rpc::RpcResponse>();
//     trp->setRCode(rpc::RCode::RCODE_OK);
//     // Json::Value val;
//     // val = 33;
//     // trp->setResult(val);
//     trp->setResult(33);
//     std::string str = trp->serialize();
//     std::cout << str << std::endl;

//     rpc::BaseMessage::ptr bmp = rpc::MessageFactory::create(rpc::MType::RSP_RPC);
//     bool ret = bmp->unserialize(str);
//     if (ret == false)
//     {
//         return -1;
//     }
//     ret = bmp->check();
//     if (ret == false)
//     {
//         return -1;
//     }

//     rpc::RpcResponse::ptr trp2 = std::dynamic_pointer_cast<rpc::RpcResponse>(bmp);
//     std::cout << (int)trp2->rcode()<< std::endl;
//     std::cout << trp2->result().asInt() << std::endl;

//     return 0;
// }





// #include "message.hpp"
// int main()
// {
//     rpc::TopicResponse::ptr trp = rpc::MessageFactory::create<rpc::TopicResponse>();
//     trp->setRCode(rpc::RCode::RCODE_OK);
//     std::string str = trp->serialize();
//     std::cout << str << std::endl;

//     rpc::BaseMessage::ptr bmp = rpc::MessageFactory::create(rpc::MType::RSP_TOPIC);
//     bool ret = bmp->unserialize(str);
//     if (ret == false)
//     {
//         return -1;
//     }
//     ret = bmp->check();
//     if (ret == false)
//     {
//         return -1;
//     }

//     rpc::TopicResponse::ptr trp2 = std::dynamic_pointer_cast<rpc::TopicResponse>(bmp);
//     std::cout << (int)trp2->rcode() << std::endl;

//     return 0;
// }





#include "message.hpp"
int main()
{
    rpc::ServiceResponse::ptr trp = rpc::MessageFactory::create<rpc::ServiceResponse>();
    trp->setRCode(rpc::RCode::RCODE_OK);
    trp->setMethod("Add");
    trp->setOptype(rpc::ServiceOptype::SERVICE_DISCOVERY);
    std::vector<rpc::Address> addrs;
    addrs.push_back(rpc::Address("127.0.0.1", 8080));
    addrs.push_back(rpc::Address("127.0.0.1", 8081));
    trp->setHost(addrs);
    std::string str = trp->serialize();
    std::cout << str << std::endl;


    rpc::BaseMessage::ptr bmp = rpc::MessageFactory::create(rpc::MType::RSP_SERVICE);
    bool ret = bmp->unserialize(str);
    if (ret == false)
    {
        return -1;
    }
    ret = bmp->check();
    if (ret == false)
    {
        return -1;
    }

    rpc::ServiceResponse::ptr trp2 = std::dynamic_pointer_cast<rpc::ServiceResponse>(bmp);
    std::cout << (int)trp2->rcode() << std::endl;
    std::cout << (int)trp2->optype() << std::endl;
    std::cout << trp2->method() << std::endl;
    std::vector<rpc::Address> addrs1 = trp2->hosts();
    for(auto &addr : addrs1)
    {
        std::cout << addr.first << ":" << addr.second << std::endl;
    }
    std::cout << trp2->hosts().size() << std::endl;

    return 0;
}