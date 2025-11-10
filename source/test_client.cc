#include "message.hpp"
#include "net.hpp"
#include <thread>


void onMessage(const rpc::BaseConnection::ptr &conn, rpc::BaseMessage::ptr &msg)
{
    std::string body = msg->serialize();
    std::cout << body << std::endl;
}

int main()
{
    auto client = rpc::ClientFactory::create("127.0.0.1", 8080);
    client->setMessageCallback(onMessage);
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
    std::this_thread::sleep_for(std::chrono::seconds(10));
    client->shutdown();

    return 0;
}