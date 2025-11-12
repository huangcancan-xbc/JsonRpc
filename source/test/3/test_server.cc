#include "../../common/dispatcher.hpp"
#include "../../server/rpc_router.hpp"

void Add(const Json::Value &req, Json::Value &rsp)
{
    int num1 = req["num1"].asInt();
    int num2 = req["num2"].asInt();
    rsp = num1 + num2;
}

int main()
{
    auto dispatcher = std::make_shared<rpc::Dispatcher>();
    auto router = std::make_shared<rpc::server::RpcRouter>();
    std::unique_ptr<rpc::server::ServiceDescribeFactory> desc_factory(new rpc::server::ServiceDescribeFactory());
    desc_factory->setMethodName("Add");
    desc_factory->setParamsDesc("num1", rpc::server::VType::INTEGRAL);
    desc_factory->setParamsDesc("num2", rpc::server::VType::INTEGRAL);
    desc_factory->setReturnType(rpc::server::VType::INTEGRAL);
    desc_factory->setCallback(Add);
    router->registerMethod(desc_factory->build());

    auto cb = std::bind(&rpc::server::RpcRouter::onRpcRequest, router.get(), std::placeholders::_1, std::placeholders::_2);
    dispatcher->registerHandler<rpc::RpcRequest>(rpc::MType::REQ_RPC, cb);

    auto server = rpc::ServerFactory::create(8080);
    auto message_cb = std::bind(&rpc::Dispatcher::onMessage, dispatcher.get(), std::placeholders::_1, std::placeholders::_2);

    server->setMessageCallback(message_cb);
    server->start();

    return 0;
}