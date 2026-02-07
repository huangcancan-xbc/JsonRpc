#include "../../server/rpc_server.hpp"
#include "test_config.hpp"

namespace
{
    void Add(const Json::Value &req, Json::Value &rsp)
    {
        rsp = req["num1"].asInt() + req["num2"].asInt();
    }

    void Echo(const Json::Value &req, Json::Value &rsp)
    {
        rsp = req["content"].asString();
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

    std::unique_ptr<rpc::server::ServiceDescribeFactory> echo_factory(new rpc::server::ServiceDescribeFactory());
    echo_factory->setMethodName("Echo");
    echo_factory->setParamsDesc("content", rpc::server::VType::STRING);
    echo_factory->setReturnType(rpc::server::VType::STRING);
    echo_factory->setCallback(Echo);

    rpc::server::RpcServer server(rpc::Address("127.0.0.1", test8::PORT_DIRECT_RPC));
    server.registerMethod(add_factory->build());
    server.registerMethod(echo_factory->build());
    server.start();
    return 0;
}
