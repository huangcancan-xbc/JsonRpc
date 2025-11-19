#include "../../common/detail.hpp"
#include "../../server/rpc_server.hpp"

void Add(const Json::Value &req, Json::Value &rsp)
{
    int num1 = req["num1"].asInt();
    int num2 = req["num2"].asInt();
    rsp = num1 + num2;
}

int main()
{
    std::unique_ptr<rpc::server::ServiceDescribeFactory> desc_factory(new rpc::server::ServiceDescribeFactory());
    desc_factory->setMethodName("Add");
    desc_factory->setParamsDesc("num1", rpc::server::VType::INTEGRAL);
    desc_factory->setParamsDesc("num2", rpc::server::VType::INTEGRAL);
    desc_factory->setReturnType(rpc::server::VType::INTEGRAL);
    desc_factory->setCallback(Add);

    rpc::server::RpcServer server(rpc::Address("127.0.0.1", 8080), true, rpc::Address("127.0.0.1", 9090));
    server.registerMethod(desc_factory->build());
    server.start();
    return 0;
}