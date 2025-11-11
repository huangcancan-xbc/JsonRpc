// #include "message.hpp"
// #include "net.hpp"

// void onMessage(const rpc::BaseConnection::ptr &conn, rpc::BaseMessage::ptr &msg)
// {
//     std::string body = msg->serialize();
//     std::cout << body << std::endl;
//     auto rpc_req = rpc::MessageFactory::create<rpc::RpcResponse>();
//     rpc_req->setId("12345");
//     rpc_req->setMType(rpc::MType::RSP_RPC);
//     rpc_req->setRCode(rpc::RCode::RCODE_OK);
//     rpc_req->setResult(666);
//     conn->send(rpc_req);
// }

// int main()
// {
//     auto server = rpc::ServerFactory::create(8080);
//     server->setMessageCallback(onMessage);
//     server->start();

//     return 0;
// }



#include "../../common/message.hpp"
#include "../../common/net.hpp"
#include "../../common/dispatcher.hpp"

void onRpcRequest(const rpc::BaseConnection::ptr &conn, rpc::RpcRequest::ptr &msg)
{
    std::cout << "收到了Rpc请求：";
    std::string body = msg->serialize();
    std::cout << body << std::endl;
    auto rpc_req = rpc::MessageFactory::create<rpc::RpcResponse>();
    rpc_req->setId("12345");
    rpc_req->setMType(rpc::MType::RSP_RPC);
    rpc_req->setRCode(rpc::RCode::RCODE_OK);
    rpc_req->setResult(666);
    conn->send(rpc_req);
}

void onTopicRequest(const rpc::BaseConnection::ptr &conn, rpc::TopicRequest::ptr &msg)
{
    std::cout << "收到了Rpc请求：";
    std::string body = msg->serialize();
    std::cout << body << std::endl;
    auto rpc_req = rpc::MessageFactory::create<rpc::RpcResponse>();
    rpc_req->setId("54321");
    rpc_req->setMType(rpc::MType::RSP_TOPIC);
    rpc_req->setRCode(rpc::RCode::RCODE_OK);
    rpc_req->setResult(666);
    conn->send(rpc_req);
}

int main()
{
    auto dispatcher = std::make_shared<rpc::Dispatcher>();
    dispatcher->registerHandler<rpc::RpcRequest>(rpc::MType::REQ_RPC, onRpcRequest);
    dispatcher->registerHandler<rpc::TopicRequest>(rpc::MType::REQ_TOPIC, onTopicRequest);

    auto server = rpc::ServerFactory::create(8080);
    auto message_cb = std::bind(&rpc::Dispatcher::onMessage, dispatcher.get(), std::placeholders::_1, std::placeholders::_2);
    server->setMessageCallback(message_cb);
    server->start();

    return 0;
}