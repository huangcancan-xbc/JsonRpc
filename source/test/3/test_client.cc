#include "../../common/dispatcher.hpp"
#include "../../client/requestor.hpp"
#include "../../client/rpc_caller.hpp"

void callback(const Json::Value &result)
{
    // std::cout << "callback result: %d" << result.asInt() << std::endl;
    ILOG("callback result: %d", result.asInt());
}

int main()
{
    auto requestor = std::make_shared<rpc::client::Requestor>();
    auto caller = std::make_shared<rpc::client::RpcCaller>(requestor);

    auto dispatcher = std::make_shared<rpc::Dispatcher>();
    auto rsp_cb = std::bind(&rpc::client::Requestor::onResponse, requestor.get(), std::placeholders::_1, std::placeholders::_2);
    dispatcher->registerHandler<rpc::BaseMessage>(rpc::MType::RSP_RPC, rsp_cb);

    auto client = rpc::ClientFactory::create("127.0.0.1", 8080);
    auto message_cb = std::bind(&rpc::Dispatcher::onMessage, dispatcher.get(), std::placeholders::_1, std::placeholders::_2);
    client->setMessageCallback(message_cb);
    client->connect();

    auto conn = client->connection();
    Json::Value params, result;
    params["num1"] = 11;
    params["num2"] = 22;
    bool ret = caller->call(conn, "Add", params, result);
    if(ret != false)
    {
        std::cout << "result:" << result.asInt() << std::endl;
    }

    rpc::client::RpcCaller::JsonAsyncResponse res_future;
    params["num1"] = 500;
    params["num2"] = 500;
    ret = caller->call(conn, "Add", params, res_future);
    if (ret != false)
    {
        result = res_future.get();
        std::cout << "result:" << result.asInt() << std::endl;
    }

    params["num1"] = 333;
    params["num2"] = 333;
    ret = caller->call(conn, "Add", params, callback);
    if (ret != false)
    {
        // result = res_future.get(); // std::future_error: No associated state
        // std::cout << "result:" << result.asInt() << std::endl;
    }

    client->shutdown();

    return 0;
}