#include "message.hpp"
#include "net.hpp"

void onMessage(const rpc::BaseConnection::ptr &conn, rpc::BaseMessage::ptr &msg)
{
    std::string body = msg->serialize();
    std::cout << body << std::endl;
    auto rpc_req = rpc::MessageFactory::create<rpc::RpcResponse>();
    rpc_req->setId("12345");
    rpc_req->setMType(rpc::MType::RSP_RPC);
    rpc_req->setRCode(rpc::RCode::RCODE_OK);
    rpc_req->setResult(666);
    conn->send(rpc_req);
}

int main()
{
    auto server = rpc::ServerFactory::create(8080);
    server->setMessageCallback(onMessage);
    server->start();

    return 0;
}