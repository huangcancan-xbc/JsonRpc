#pragma once
#include "../common/dispatcher.hpp"
#include "requestor.hpp"
#include "rpc_caller.hpp"
#include "rpc_registry.hpp"


namespace rpc
{
    namespace client
    {
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
            client::Provider::ptr _provider;
            Requestor::ptr _requestor;
            Dispatcher::ptr _dispatcher;
            BaseClient::ptr _client;
        };



        class DiscoveryClient
        {
        public:
            using ptr = std::shared_ptr<DiscoveryClient>;

            // 构造函数传入注册中心的地址信息，用于连接注册中心
            DiscoveryClient(const std::string &ip, int port, Discoverer::OfflineCallback &cb)
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

            // 向外提供的接口
            bool serviceDiscovery(const std::string &method, Address &host)
            {
                return _discoverer->serviceDiscovery(_client->connection(), method, host);
            }

        private:
            Requestor::ptr _requestor;
            client::Discoverer::ptr _discoverer;
            Dispatcher::ptr _dispatcher;
            BaseClient::ptr _client;
        };



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
                if(-enableDiscovery)
                {
                    _discovery_client = std::make_shared<DiscoveryClient>(ip, port);
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
                size_t operator()(const Address &host)
                {
                    std::string addr = host.first + std::to_string(host.second);
                    return std::hash<std::string>{}(addr);
                }
            };
            
            bool _enableDiscovery;
            DiscoveryClient::ptr _discovery_client;
            Requestor::ptr _requestor;
            RpcCaller::ptr _caller;
            Dispatcher::ptr _dispatcher;
            BaseClient::ptr _rpc_client;
            std::mutex _mutex;
            std::unordered_map<Address, BaseClient::ptr, AddressHash> _rpc_clients;
        };
    }
}