// #include "message.hpp"
// #include "net.hpp"
// #include <thread>

// void onMessage(const rpc::BaseConnection::ptr &conn, rpc::BaseMessage::ptr &msg)
// {
//     std::string body = msg->serialize();
//     std::cout << body << std::endl;
// }

// int main()
// {
//     auto client = rpc::ClientFactory::create("127.0.0.1", 8080);
//     client->setMessageCallback(onMessage);
//     client->connect();
//     auto rpc_req = rpc::MessageFactory::create<rpc::RpcRequest>();
//     rpc_req->setId("12345");
//     rpc_req->setMType(rpc::MType::REQ_RPC);
//     rpc_req->setMethod("Add");
//     Json::Value param;
//     param["num1"] = 333;
//     param["num2"] = 333;
//     rpc_req->setParams(param);
//     client->send(rpc_req);
//     std::this_thread::sleep_for(std::chrono::seconds(10));
//     client->shutdown();

//     return 0;
// }

#include "../../common/message.hpp"
#include "../../common/dispatcher.hpp"
#include "../../common/net.hpp"
#include <thread>

void onRpcResponse(const rpc::BaseConnection::ptr &conn, rpc::RpcResponse::ptr &msg)
{
    std::cout << "收到了Rpc响应！";
    std::string body = msg->serialize();
    std::cout << body << std::endl;
}

void onTopicResponse(const rpc::BaseConnection::ptr &conn, rpc::TopicResponse::ptr &msg)
{
    std::cout << "收到了Topic响应！";
    std::string body = msg->serialize();
    std::cout << body << std::endl;
}

int main()
{
    auto dispatcher = std::make_shared<rpc::Dispatcher>();
    dispatcher->registerHandler<rpc::RpcResponse>(rpc::MType::RSP_RPC, onRpcResponse);          // 注册映射关系
    dispatcher->registerHandler<rpc::TopicResponse>(rpc::MType::RSP_TOPIC, onTopicResponse);    // 注册映射关系

    auto client = rpc::ClientFactory::create("127.0.0.1", 8080);
    auto message_cb = std::bind(&rpc::Dispatcher::onMessage, dispatcher.get(), std::placeholders::_1, std::placeholders::_2);
    client->setMessageCallback(message_cb);
    client->connect();
    auto rpc_req = rpc::MessageFactory::create<rpc::RpcRequest>();
    rpc_req->setId("12345");
    rpc_req->setMType(rpc::MType::REQ_RPC);
    rpc_req->setMethod("Add");
    Json::Value param;
    param["num1"] = 333;
    param["num2"] = 333;
    rpc_req->setParams(param);
    client->send(rpc_req);

    auto topic_req = rpc::MessageFactory::create<rpc::TopicRequest>();
    topic_req->setId("54321");
    topic_req->setMType(rpc::MType::REQ_TOPIC);
    topic_req->setOptype(rpc::TopicOptype::TOPIC_CREATE);
    topic_req->setTopicKey("news");
    client->send(topic_req);

    std::this_thread::sleep_for(std::chrono::seconds(10));
    client->shutdown();

    return 0;
}