/*
    注册中心服务器：注册、发现和管理
    rpc服务提供：注册执行rpc、连接注册中心、处理客户端rpc请求、
    主题服务器：管理主题（创建、删除）、处理客户端（订阅、取消订阅）、广播订阅者
*/
#pragma once
#include "../common/dispatcher.hpp"
#include "../client/rpc_client.hpp"
#include "rpc_router.hpp"
#include "rpc_registry.hpp"
#include "rpc_topic.hpp"


namespace rpc
{
    namespace server
    {
        // 注册中心服务端：针对服务注册与发现请求进行处理
        class RegistryServer
        {
        public:
            using ptr = std::shared_ptr<RegistryServer>;

            RegistryServer(int port)
                : _pd_manager(std::make_shared<PDManager>()),
                  _dispatcher(std::make_shared<rpc::Dispatcher>())
            {
                auto service_cb = std::bind(&PDManager::onServiceRequest, _pd_manager.get(), std::placeholders::_1, std::placeholders::_2);
                _dispatcher->registerHandler<ServiceRequest>(MType::REQ_SERVICE, service_cb);

                _server = rpc::ServerFactory::create(port);
                auto message_cb = std::bind(&rpc::Dispatcher::onMessage, _dispatcher.get(), std::placeholders::_1, std::placeholders::_2);
                _server->setMessageCallback(message_cb);

                auto close_cb = std::bind(&RegistryServer::onConnShutdown, this, std::placeholders::_1);
                _server->setCloseCallback(close_cb);
            }

            void start()
            {
                _server->start();
            }

        private:
            void onConnShutdown(const BaseConnection::ptr &conn)
            {
                _pd_manager->onConnShutdown(conn);
            }

        private:
            PDManager::ptr _pd_manager;  // 对提供者和发现者的管理
            Dispatcher::ptr _dispatcher; // 消息的分发
            BaseServer::ptr _server;     // 服务器实例
        };



        // 提供rpc服务
        class RpcServer
        {
        public:
            using ptr = std::shared_ptr<RpcServer>;

            RpcServer(const Address &access_addr,
                      bool enableRegistry = false,
                      const Address &registry_server_addr = Address())
                : _enableRegistry(enableRegistry),
                  _access_addr(access_addr),
                  _router(std::make_shared<rpc::server::RpcRouter>()),
                  _dispatcher(std::make_shared<rpc::Dispatcher>())
            {
                if (enableRegistry)
                {
                    _reg_client = std::make_shared<client::RegistryClient>(registry_server_addr.first, registry_server_addr.second);
                }

                auto rpc_cb = std::bind(&RpcRouter::onRpcRequest, _router.get(), std::placeholders::_1, std::placeholders::_2);
                _dispatcher->registerHandler<rpc::RpcRequest>(rpc::MType::REQ_RPC, rpc_cb);

                _server = rpc::ServerFactory::create(access_addr.second);
                auto message_cb = std::bind(&rpc::Dispatcher::onMessage, _dispatcher.get(), std::placeholders::_1, std::placeholders::_2);
                _server->setMessageCallback(message_cb);
            }

            // 决定注册中心注册与否
            void registerMethod(const ServiceDescribe::ptr &service)
            {
                if (_enableRegistry)
                {
                    _reg_client->registryMethod(service->method(), _access_addr);
                }

                _router->registerMethod(service);
            }

            void start()
            {
                _server->start();
            }

        private:
            bool _enableRegistry;                    // 是否使用注册中心
            Address _access_addr;                    // 服务器主机信息
            client::RegistryClient::ptr _reg_client; // 服务的注册
            RpcRouter::ptr _router;                  // 根据请求找到对应的服务
            Dispatcher::ptr _dispatcher;             // 分发消息
            BaseServer::ptr _server;                 // 服务器
        };



        // 处理/管理主题
        class TopicServer
        {
        public:
            using ptr = std::shared_ptr<TopicServer>;

            TopicServer(int port)
                : _topic_manager(std::make_shared<TopicManager>()),
                  _dispatcher(std::make_shared<rpc::Dispatcher>())
            {
                auto topic_cb = std::bind(&TopicManager::onTopicRequest, _topic_manager.get(), std::placeholders::_1, std::placeholders::_2);
                _dispatcher->registerHandler<TopicRequest>(MType::REQ_TOPIC, topic_cb);

                _server = rpc::ServerFactory::create(port);
                auto message_cb = std::bind(&rpc::Dispatcher::onMessage, _dispatcher.get(), std::placeholders::_1, std::placeholders::_2);
                _server->setMessageCallback(message_cb);

                auto close_cb = std::bind(&TopicServer::onConnShutdown, this, std::placeholders::_1);
                _server->setCloseCallback(close_cb);
            }

            void start()
            {
                _server->start();
            }

        private:
            void onConnShutdown(const BaseConnection::ptr &conn)
            {
                _topic_manager->onShutdown(conn);
            }

        private:
            TopicManager::ptr _topic_manager; // 主题管理：创建、删除、订阅等
            Dispatcher::ptr _dispatcher;      // 分发消息
            BaseServer::ptr _server;          // 服务器
        };
    }
}