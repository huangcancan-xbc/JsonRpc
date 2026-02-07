#include "../../server/rpc_server.hpp"
#include "test_config.hpp"

namespace
{
    void Add(const Json::Value &req, Json::Value &rsp)
    {
        rsp = req["num1"].asInt() + req["num2"].asInt();
    }
}

int main()
{
    std::unique_ptr<rpc::server::ServiceDescribeFactory> add_factory(new rpc::server::ServiceDescribeFactory());
    add_factory->setMethodName("Add");
    add_factory->setParamsDesc("num1", rpc::server::VType::INTEGRAL);
    add_factory->setParamsDesc("num2", rpc::server::VType::INTEGRAL);
    add_factory->setReturnType(rpc::server::VType::INTEGRAL);
    add_factory->setCallback(Add);

    rpc::server::RpcServer server(
        rpc::Address("127.0.0.1", test8::PORT_REGISTRY_RPC),
        true,
        rpc::Address("127.0.0.1", test8::PORT_REGISTRY));
    server.registerMethod(add_factory->build());
    server.start();
    return 0;
}
