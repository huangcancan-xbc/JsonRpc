/*
    服务注册客户端：向注册中心注册服务，告诉注册中心能提供什么样的服务、主机信息
    服务发现客户端：从注册中心发现服务，客户端需要服务时，会先问注册中心该服务在哪，谁能提供
    rpc调用客户端：发起rpc调用（同步/异步/回调）
    主题客户端：创建、删除、发布、订阅、取消订阅
*/
#pragma once
#include "../common/dispatcher.hpp"
#include "requestor.hpp"
#include "rpc_caller.hpp"
#include "rpc_registry.hpp"
#include "rpc_topic.hpp"


namespace rpc
{
    namespace client
    {
        // 连接注册中心和注册服务
        class RegistryClient
        {
        public:
            using ptr = std::shared_ptr<RegistryClient>;

            // 构造函数传入注册中心的地址信息，用于连接注册中心
            RegistryClient(const std::string &ip, int port)
                : _requestor(std::make_shared<Requestor>()),
                _provider(std::make_shared<client::Provider>(_requestor)),
                _dispatcher(std::make_shared<Dispatcher>())
            {
                auto rsp_cb = std::bind(&client::Requestor::onResponse, _requestor.get(), std::placeholders::_1, std::placeholders::_2);
                _dispatcher->registerHandler<BaseMessage>(MType::RSP_SERVICE, rsp_cb);

                auto message_cb = std::bind(&Dispatcher::onMessage, _dispatcher.get(), std::placeholders::_1, std::placeholders::_2);

                _client = ClientFactory::create(ip, port);
                _client->setMessageCallback(message_cb);
                _client->connect();
            }

            // 向外提供服务的接口
            bool registryMethod(const std::string &method, const Address &host)
            {
                return _provider->registryMethod(_client->connection(), method, host);
            }

        private:
            // 犯了一个最愚蠢的错误：初始化列表的初始化顺序！导致 _provider 拿到的是一个还没构造好的 _requestor（空指针），然后在里面加锁直接炸成 std::system_error；
            // 回调 + 线程里，用同步原语（CountDownLatch 等）时，唤醒顺序也要保证被唤醒线程看到的是“完全初始化好的状态”。
            // client::Provider::ptr _provider;
            // Requestor::ptr _requestor;

            Requestor::ptr _requestor;       // rpc请求发送和响应接收
            client::Provider::ptr _provider; // 注册中心注册
            Dispatcher::ptr _dispatcher;     // 注册中心响应
            BaseClient::ptr _client;         // rpc客户端（与注册中心建立连接）
        };



        // 服务发现
        class DiscoveryClient
        {
        public:
            using ptr = std::shared_ptr<DiscoveryClient>;

            // 构造函数传入注册中心的地址信息，用于连接注册中心
            DiscoveryClient(const std::string &ip, int port, const Discoverer::OfflineCallback &cb)
                : _requestor(std::make_shared<Requestor>()),
                  _discoverer(std::make_shared<client::Discoverer>(_requestor, cb)),
                  _dispatcher(std::make_shared<Dispatcher>())
            {
                auto rsp_cb = std::bind(&client::Requestor::onResponse, _requestor.get(), std::placeholders::_1, std::placeholders::_2);
                _dispatcher->registerHandler<BaseMessage>(MType::RSP_SERVICE, rsp_cb);

                auto req_cb = std::bind(&client::Discoverer::onServiceRequest, _discoverer.get(), std::placeholders::_1, std::placeholders::_2);
                _dispatcher->registerHandler<ServiceRequest>(MType::REQ_SERVICE, req_cb);

                auto message_cb = std::bind(&Dispatcher::onMessage, _dispatcher.get(), std::placeholders::_1, std::placeholders::_2);

                _client = ClientFactory::create(ip, port);
                _client->setMessageCallback(message_cb);
                _client->connect();
            }

            // 查询服务提供者的相关信息
            bool serviceDiscovery(const std::string &method, Address &host)
            {
                return _discoverer->serviceDiscovery(_client->connection(), method, host);
            }

        private:
            Requestor::ptr _requestor;           // rpc请求发送和响应接收
            client::Discoverer::ptr _discoverer; // 从注册中心查询服务的提供者
            Dispatcher::ptr _dispatcher;         // 处理注册中心和服务提供者的响应
            BaseClient::ptr _client;             // rpc客户端（与注册中心建立连接）
        };



        // 客户端发起rpc请求
        class RpcClient
        {
        public:
            using ptr = std::shared_ptr<RpcClient>;

            // enableDiscovery：是否启用服务发现功能，这决定了传入的地址信息是注册中心的地址，还是服务提供者的地址
            RpcClient (bool enableDiscovery, const std::string &ip, int port)
                :_enableDiscovery(enableDiscovery),
                _requestor(std::make_shared<Requestor>()),
                _dispatcher(std::make_shared<Dispatcher>()),
                _caller(std::make_shared<rpc::client::RpcCaller>(_requestor))
            {
                // 针对rpc请求后的响应进行回调处理
                auto rsp_cb = std::bind(&client::Requestor::onResponse, _requestor.get(), std::placeholders::_1, std::placeholders::_2);
                _dispatcher->registerHandler<BaseMessage>(MType::RSP_RPC, rsp_cb);

                // 如果启用了服务发现，地址信息就是注册中心的地址，是服务发现客户端需要连接的地址，那么就通过地址信息实例化discovery_client
                // 如果没有启动服务发现，那么地址信息就是服务提供者的地址，直接实例化好rpc_client
                if(_enableDiscovery)
                {
                    auto offline_cb = std::bind(&RpcClient::delClient, this, std::placeholders::_1);
                    _discovery_client = std::make_shared<DiscoveryClient>(ip, port, offline_cb);
                }
                else
                {
                    auto message_cb = std::bind(&Dispatcher::onMessage, _dispatcher.get(), std::placeholders::_1, std::placeholders::_2);
                    _rpc_client = ClientFactory::create(ip, port);
                    _rpc_client->setMessageCallback(message_cb);
                    _rpc_client->connect();
                }
            }

            bool call(const std::string &method, const Json::Value &params, Json::Value &result)
            {
                // 获取服务提供者：1. 服务发现；  2. 固定服务提供者
                BaseClient::ptr client = getClient(method);
                if (client.get() == nullptr)
                {
                    return false;
                }

                // 3. 通过客户端连接，发送rpc请求
                return _caller->call(client->connection(), method, params, result);
            }

            bool call(const std::string &method, const Json::Value &params, RpcCaller::JsonAsyncResponse &result)
            {
                BaseClient::ptr client = getClient(method);
                if (client.get() == nullptr)
                {
                    return false;
                }
                
                return _caller->call(client->connection(), method, params, result);
            }

            bool call(const std::string &method, const Json::Value &params, const RpcCaller::JsonResponseCallback &cb)
            {
                BaseClient::ptr client = getClient(method);
                if (client.get() == nullptr)
                {
                    return false;
                }

                return _caller->call(client->connection(), method, params, cb);
            }

        private:
            BaseClient::ptr newClient(const Address &host)
            {
                auto message_cb = std::bind(&Dispatcher::onMessage, _dispatcher.get(), std::placeholders::_1, std::placeholders::_2);
                auto client = ClientFactory::create(host.first, host.second);
                client->setMessageCallback(message_cb);
                client->connect();
                putClient(host, client);
                return client;
            }

            BaseClient::ptr getClient(const Address &host)
            {
                std::unique_lock<std::mutex> lock(_mutex);
                auto it = _rpc_clients.find(host);
                if(it == _rpc_clients.end())
                {
                    return BaseClient::ptr();
                }

                return it->second;
            }

            BaseClient::ptr getClient(const std::string method)
            {
                BaseClient::ptr client;
                if (_enableDiscovery)
                {
                    // 1.获取服务提供者的地址信息
                    Address host;
                    bool ret = _discovery_client->serviceDiscovery(method, host);

                    if(ret == false)
                    {
                        ELOG("当前 %s 服务，没有找到服务提供者！", method.c_str());
                        return BaseClient::ptr();
                    }

                    // 2.看看服务提供者是否已存在实例化客户端，有就直接用，没有就创建
                    client = getClient(host);
                    if(client.get() == nullptr)
                    {
                        client = newClient(host);
                    }
                }
                else
                {
                    client = _rpc_client;
                }

                return client;
            }

            void putClient(const Address &host, BaseClient::ptr &client)
            {
                std::unique_lock<std::mutex> lock(_mutex);
                _rpc_clients.insert(std::make_pair(host, client));
            }

            void delClient(const Address &host)
            {
                std::unique_lock<std::mutex> lock(_mutex);
                _rpc_clients.erase(host);
            }

        private:
            struct AddressHash
            {
                size_t operator()(const Address &host) const
                {
                    std::string addr = host.first + std::to_string(host.second);
                    return std::hash<std::string>{}(addr);
                }
            };

            bool _enableDiscovery;                  // 是否启用服务发现
            DiscoveryClient::ptr _discovery_client; // 用于服务发现的客户端
            Requestor::ptr _requestor;              // RPC请求发送和响应接收
            RpcCaller::ptr _caller;                 // 发起rpc调用
            Dispatcher::ptr _dispatcher;            // 分发响应消息
            BaseClient::ptr _rpc_client;            // 和服务提供者通信的客户端
            std::mutex _mutex;

            // 已连接rpc客户端的表，key：主机地址信息，val：rpc客户端
            std::unordered_map<Address, BaseClient::ptr, AddressHash> _rpc_clients;
        };



        // rpc主题功能：创建、发布、订阅、删除
        class TopicClient
        {
        public:
            TopicClient(const std::string &ip,int port)
                :_requestor(std::make_shared<Requestor>()),
                _dispatcher(std::make_shared<Dispatcher>()),
                _topic_manager(std::make_shared<TopicManager>(_requestor))
            {
                auto rsp_cb = std::bind(&client::Requestor::onResponse, _requestor.get(), std::placeholders::_1, std::placeholders::_2);
                _dispatcher->registerHandler<BaseMessage>(MType::RSP_TOPIC, rsp_cb);

                auto msg_cb = std::bind(&TopicManager::onPublish, _topic_manager.get(), std::placeholders::_1, std::placeholders::_2);
                _dispatcher->registerHandler<TopicRequest>(MType::REQ_TOPIC, msg_cb);

                auto message_cb = std::bind(&Dispatcher::onMessage, _dispatcher.get(), std::placeholders::_1, std::placeholders::_2);

                _rpc_client = ClientFactory::create(ip, port);
                _rpc_client->setMessageCallback(message_cb);
                _rpc_client->connect();
            }

            bool create(const std::string &key)
            {
                return _topic_manager->create(_rpc_client->connection(), key);
            }

            bool remove(const std::string &key)
            {
                return _topic_manager->remove(_rpc_client->connection(), key);
            }

            // 订阅主题
            bool subscribe(const std::string &key, const TopicManager::SubCallback &cb)
            {
                return _topic_manager->subscribe(_rpc_client->connection(), key, cb);
            }

            // 取消订阅
            bool cancel(const std::string &key)
            {
                return _topic_manager->cancel(_rpc_client->connection(), key);
            }

            bool publish(const std::string &key, const std::string &msg)
            {
                return _topic_manager->publish(_rpc_client->connection(), key, msg);
            }

            // 关闭rpc客户端
            void shutdown()
            {
                _rpc_client->shutdown();
            }

        private:
            Requestor::ptr _requestor;        // rpc请求发送和响应接收
            TopicManager::ptr _topic_manager; // 处理主题（创建、订阅、发布、删除）
            Dispatcher::ptr _dispatcher;      // 处理主题的响应
            BaseClient::ptr _rpc_client;      // rpc客户端（与服务提供者通信）
        };
    }
}