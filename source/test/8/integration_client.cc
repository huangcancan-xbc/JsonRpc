#include "../../client/rpc_client.hpp"
#include "../../common/detail.hpp"
#include "test_config.hpp"
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstring>
#include <future>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>
#include <vector>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

namespace
{
    bool check(bool cond, const std::string &msg)
    {
        if (!cond)
        {
            std::cerr << "[FAIL] " << msg << std::endl;
            return false;
        }
        std::cout << "[PASS] " << msg << std::endl;
        return true;
    }

    bool testDirectRpc()
    {
        rpc::client::RpcClient client(false, "127.0.0.1", test8::PORT_DIRECT_RPC);

        Json::Value params, result;
        params["num1"] = 11;
        params["num2"] = 22;
        if (!check(client.call("Add", params, result), "直连同步RPC调用成功"))
        {
            return false;
        }
        if (!check(result.asInt() == 33, "直连同步RPC结果正确"))
        {
            return false;
        }

        rpc::client::RpcCaller::JsonAsyncResponse async_future;
        params["num1"] = 30;
        params["num2"] = 47;
        if (!check(client.call("Add", params, async_future), "直连异步RPC发送成功"))
        {
            return false;
        }
        result = async_future.get();
        if (!check(result.asInt() == 77, "直连异步RPC结果正确"))
        {
            return false;
        }

        std::promise<int> callback_promise;
        auto callback_future = callback_promise.get_future();
        params["num1"] = 50;
        params["num2"] = 71;
        auto cb = [&callback_promise](const Json::Value &cb_result) {
            callback_promise.set_value(cb_result.asInt());
        };
        if (!check(client.call("Add", params, cb), "直连回调RPC发送成功"))
        {
            return false;
        }
        if (!check(callback_future.wait_for(std::chrono::seconds(2)) == std::future_status::ready, "直连回调RPC按时返回"))
        {
            return false;
        }
        if (!check(callback_future.get() == 121, "直连回调RPC结果正确"))
        {
            return false;
        }

        Json::Value invalid;
        invalid["num1"] = 1;
        if (!check(client.call("Add", invalid, result) == false, "参数缺失时RPC调用失败"))
        {
            return false;
        }

        if (!check(client.call("NoSuchMethod", params, result) == false, "未知方法调用失败"))
        {
            return false;
        }

        Json::Value echo_params;
        std::string large_payload(32 * 1024, 'x');
        echo_params["content"] = large_payload;
        if (!check(client.call("Echo", echo_params, result), "大字符串回显调用成功"))
        {
            return false;
        }
        if (!check(result.asString() == large_payload, "大字符串回显内容正确"))
        {
            return false;
        }

        // 并发场景：多个客户端同时发起请求，模拟日常并发调用
        std::atomic<int> ok_count(0);
        std::vector<std::thread> workers;
        for (int i = 0; i < 8; i++)
        {
            workers.emplace_back([&ok_count, i]() {
                rpc::client::RpcClient c(false, "127.0.0.1", test8::PORT_DIRECT_RPC);
                Json::Value p, r;
                p["num1"] = i;
                p["num2"] = i * 2;
                bool ret = c.call("Add", p, r);
                if (ret && r.asInt() == (i + i * 2))
                {
                    ok_count.fetch_add(1);
                }
            });
        }
        for (auto &t : workers)
        {
            t.join();
        }
        if (!check(ok_count.load() == 8, "并发RPC调用全部成功"))
        {
            return false;
        }

        return true;
    }

    bool testRegistryRpc()
    {
        rpc::client::RpcClient client(true, "127.0.0.1", test8::PORT_REGISTRY);
        Json::Value params, result;
        params["num1"] = 6;
        params["num2"] = 9;

        if (!check(client.call("Add", params, result), "注册中心发现后RPC调用成功"))
        {
            return false;
        }
        if (!check(result.asInt() == 15, "注册中心模式返回结果正确"))
        {
            return false;
        }

        if (!check(client.call("MissingMethod", params, result) == false, "注册中心模式未知方法失败"))
        {
            return false;
        }
        return true;
    }

    bool testTopic()
    {
        rpc::client::TopicClient subscriber("127.0.0.1", test8::PORT_TOPIC);
        rpc::client::TopicClient publisher("127.0.0.1", test8::PORT_TOPIC);
        std::atomic<int> recv_count(0);
        std::mutex mtx;
        std::condition_variable cv;
        auto cb = [&recv_count, &cv](const std::string &, const std::string &) {
            recv_count.fetch_add(1);
            cv.notify_all();
        };

        // 主题不存在时，订阅/删除/取消都应失败，避免客户端误判为成功
        if (!check(subscriber.subscribe("missing.topic", cb) == false, "订阅不存在主题失败"))
        {
            return false;
        }
        if (!check(publisher.remove("missing.topic") == false, "删除不存在主题失败"))
        {
            return false;
        }
        if (!check(subscriber.cancel("missing.topic") == false, "取消不存在主题失败"))
        {
            return false;
        }

        if (!check(subscriber.create("daily.news"), "创建主题成功"))
        {
            return false;
        }

        if (!check(subscriber.subscribe("daily.news", cb), "订阅主题成功"))
        {
            return false;
        }

        for (int i = 0; i < 20; i++)
        {
            if (!publisher.publish("daily.news", "msg-" + std::to_string(i)))
            {
                std::cerr << "[FAIL] 发布消息失败: " << i << std::endl;
                return false;
            }
        }

        {
            std::unique_lock<std::mutex> lock(mtx);
            cv.wait_for(lock, std::chrono::seconds(3), [&recv_count]() {
                return recv_count.load() >= 20;
            });
        }
        if (!check(recv_count.load() >= 20, "订阅端收齐20条消息"))
        {
            return false;
        }

        if (!check(subscriber.cancel("daily.news"), "取消订阅成功"))
        {
            return false;
        }
        int before = recv_count.load();
        publisher.publish("daily.news", "after-cancel");
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        if (!check(recv_count.load() == before, "取消订阅后不再收到消息"))
        {
            return false;
        }

        publisher.remove("daily.news");
        subscriber.shutdown();
        publisher.shutdown();
        return true;
    }

    bool testConnectTimeout()
    {
        auto begin = std::chrono::steady_clock::now();
        rpc::client::RpcClient client(true, "127.0.0.1", test8::PORT_UNUSED_REGISTRY);
        Json::Value params, result;
        params["num1"] = 1;
        params["num2"] = 2;
        bool ret = client.call("Add", params, result);
        auto cost = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - begin).count();

        if (!check(ret == false, "不可达注册中心场景调用失败"))
        {
            return false;
        }

        // 连接超时 + 1次调用失败，整体耗时应在可接受范围内
        if (!check(cost < 7000, "不可达注册中心场景不会长时间卡住"))
        {
            return false;
        }
        return true;
    }

    bool sendMalformedFrameAndVerify()
    {
        // 先发畸形帧，验证服务端不会崩溃
        int fd = socket(AF_INET, SOCK_STREAM, 0);
        if (fd < 0)
        {
            std::cerr << "[FAIL] 创建socket失败" << std::endl;
            return false;
        }

        sockaddr_in addr;
        std::memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_port = htons(test8::PORT_REGISTRY);
        inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);

        if (connect(fd, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) != 0)
        {
            close(fd);
            std::cerr << "[FAIL] 连接注册中心失败（畸形帧测试）" << std::endl;
            return false;
        }

        const unsigned char malformed[] = {
            0x00, 0x00, 0x00, 0x0c, // total_len = 12
            0x00, 0x00, 0x00, 0x04, // mtype = REQ_SERVICE
            0x00, 0x00, 0x00, 0x64, // idlen = 100 (非法)
            'A', 'B', 'C', 'D'
        };
        send(fd, malformed, sizeof(malformed), 0);
        close(fd);
        std::this_thread::sleep_for(std::chrono::milliseconds(200));

        // 再做一次正常RPC，确认服务仍可用
        rpc::client::RpcClient client(true, "127.0.0.1", test8::PORT_REGISTRY);
        Json::Value params, result;
        params["num1"] = 7;
        params["num2"] = 8;
        if (!check(client.call("Add", params, result), "畸形帧后服务仍可发现并调用"))
        {
            return false;
        }
        if (!check(result.asInt() == 15, "畸形帧后调用结果正确"))
        {
            return false;
        }
        return true;
    }
}

int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        std::cerr << "Usage: " << argv[0] << " <direct|registry|topic|timeout|malformed>" << std::endl;
        return 2;
    }

    std::string mode = argv[1];
    bool ok = false;
    if (mode == "direct")
    {
        ok = testDirectRpc();
    }
    else if (mode == "registry")
    {
        ok = testRegistryRpc();
    }
    else if (mode == "topic")
    {
        ok = testTopic();
    }
    else if (mode == "timeout")
    {
        ok = testConnectTimeout();
    }
    else if (mode == "malformed")
    {
        ok = sendMalformedFrameAndVerify();
    }
    else
    {
        std::cerr << "Unknown mode: " << mode << std::endl;
        return 2;
    }

    if (!ok)
    {
        return 1;
    }
    return 0;
}
