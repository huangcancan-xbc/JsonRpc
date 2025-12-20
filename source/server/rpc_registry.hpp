/*
    服务提供者管理、服务发现者管理、提供者和发现者管理

    * 服务提供者注册 → ProviderManager记录 → DiscovererManager通知发现者
    * 客户端服务发现 → DiscovererManager记录 → ProviderManager查询提供者
    * 服务提供者下线 → ProviderManager清理 → DiscovererManager通知发现者
*/
#pragma once
#include "../common/net.hpp"
#include "../common/message.hpp"
#include <set>


namespace rpc
{
    namespace server
    {
        // 管理服务提供者
        class ProviderManager
        {
        public:
            using ptr = std::shared_ptr<ProviderManager>;

            // 一个服务提供者节点
            struct Provider
            {
                using ptr = std::shared_ptr<Provider>;

                std::mutex _mutex;
                BaseConnection::ptr conn;         // 服务提供者与注册中心的连接
                Address host;                     // 服务提供者的主机信息
                std::vector<std::string> methods; // 服务提供者能提供的服务（函数名）

                Provider(const BaseConnection::ptr &c, const Address &h)
                    :conn(c),host(h)
                {
                    
                }

                // 添加一个提供（函数名）的服务
                void appendMethod(const std::string &method)
                {
                    std::unique_lock<std::mutex> lock(_mutex);
                    methods.emplace_back(method);
                }
            };

            // 当一个新的服务提供者进行服务注册的时候进行调用
            void addProvider(const BaseConnection::ptr &c, const Address &h, const std::string &method)
            {
                Provider::ptr provider;

                // 查找连接所关联的服务提供者对象，找到就直接过去，没找到就创建并建立关联
                {
                    std::unique_lock<std::mutex> lock(_mutex);
                    auto it = _conns.find(c);
                    if(it != _conns.end())
                    {
                        provider = it->second;
                    }
                    else
                    {
                        provider = std::make_shared<Provider>(c, h);
                        _conns.insert(std::make_pair(c, provider));
                    }

                    // method方法所提供的主机要增加一个（_provider要新增数据）
                    auto &providers = _providers[method];
                    providers.insert(provider);
                }

                // 向服务对象中新增一个所能提供的服务的名称
                provider->appendMethod(method);
            }

            // 当一个服务提供者断开连接的时候，获取他的信息（用于服务的下线通知）
            Provider::ptr getProvider(const BaseConnection::ptr &c)
            {
                std::unique_lock<std::mutex> lock(_mutex);
                auto it =_conns.find(c);
                if(it != _conns.end())
                {
                    return it->second;
                }

                return Provider::ptr();
            }

            // 当一个服务提供者断开连接的时候，删除他所关联的信息
            void delProvider(const BaseConnection::ptr &c)
            {
                std::unique_lock<std::mutex> lock(_mutex);
                auto it = _conns.find(c);
                if(it == _conns.end())
                {
                    return;
                }

                // 在每一个注册了方法名的提供者进行删除
                // 如果是服务提供者，看看具体提供的服务是什么，从提供信息中删除当前服务提供者
                for(auto &method:it->second->methods)
                {
                    auto &providers = _providers[method];
                    providers.erase(it->second);
                }

                // 删除连接（与服务提供者的关联关系）
                _conns.erase(it);
            }

            // 找到每一个函数名对应的提供者主机信息
            std::vector<Address> methodHosts(const std::string &method)
            {
                std::unique_lock<std::mutex> lock(_mutex);

                auto it = _providers.find(method);
                if(it == _providers.end())
                {
                    return std::vector<Address>();
                }

                std::vector<Address> result;
                for(auto &provider : it->second)
                {
                    result.push_back(provider->host);
                }
                
                return result;
            }

        private:
            std::mutex _mutex;
            std::unordered_map<std::string, std::set<Provider::ptr>> _providers; // key：函数名，val：一组服务提供者
            std::unordered_map<BaseConnection::ptr, Provider::ptr> _conns;       // key：连接，val：服务提供者
        };



        // 服务发现客户端的管理
        class DiscovererManager
        {
        public:
            using ptr = std::shared_ptr<DiscovererManager>;

            // 一个正在进行服务发现的客户端
            struct Discoverer
            {
                using ptr = std::shared_ptr<Discoverer>;

                std::mutex _mutex;
                BaseConnection::ptr conn;         // 与发现者客户端的连接
                std::vector<std::string> methods; // 发现过哪些服务（函数名）

                Discoverer(const BaseConnection::ptr &c)
                    :conn(c)
                {
                    
                }

                // 添加一个发现到的服务（函数名）
                void appendMethod(const std::string &method)
                {
                    std::unique_lock<std::mutex> lock(_mutex);
                    methods.push_back(method);
                }
            };

            // 当每次客户端进行服务发现的时候，新增了发现者，就将其服务名称添加/记录下来
            Discoverer::ptr addDiscoverer(const BaseConnection::ptr &c, const std::string &method)
            {
                Discoverer::ptr discoverer;

                {
                    std::unique_lock<std::mutex> lock(_mutex);
                    auto it = _conns.find(c);
                    if (it != _conns.end())
                    {
                        discoverer = it->second;
                    }
                    else
                    {
                        discoverer = std::make_shared<Discoverer>(c);
                        _conns.insert(std::make_pair(c, discoverer));
                    }

                    auto &discoverers = _discoverers[method];
                    discoverers.insert(discoverer);
                }

                discoverer->appendMethod(method);
                return discoverer;
            }

            // 发现者客户端断开连接时，要先找到发现者的信息，再删除所有关联数据
            void delDiscoverer(const BaseConnection::ptr &c)
            {
                std::unique_lock<std::mutex> lock(_mutex);

                auto it = _conns.find(c);
                if (it == _conns.end())
                {
                    return;
                }
                
                for(auto &method: it->second->methods)
                {
                    auto& discoverers = _discoverers[method];
                    discoverers.erase(it->second);
                }

                _conns.erase(it);
            }

            // 当一个新的服务提供者上线时，进行上线通知（通知所有查询过该方法的客户端）
            void onlineNotify(const std::string method, const Address &host)
            {
                return notify(method, host, ServiceOptype::SERVICE_ONLINE);
            }

            // 当一个服务提供者断开连接时，进行下线通知
            void offlineNotify(const std::string method, const Address &host)
            {
                return notify(method, host, ServiceOptype::SERVICE_OFFLINE);
            }

        private:
            // 进行通知（上线/下线）
            void notify(const std::string method, const Address &host, ServiceOptype optype)
            {
                std::unique_lock<std::mutex> lock(_mutex);

                auto it = _discoverers.find(method);
                if (it == _discoverers.end())
                {
                    return;
                }

                auto msg_req = MessageFactory::create<ServiceRequest>();
                msg_req->setId(UUID::uuid());
                msg_req->setMType(MType::REQ_SERVICE);
                msg_req->setMethod(method);
                msg_req->setHost(host);
                msg_req->setOptype(optype);

                for (auto &discoverer : it->second)
                {
                    discoverer->conn->send(msg_req);
                }
            }

        private:
            std::mutex _mutex;
            std::unordered_map<std::string, std::set<Discoverer::ptr>> _discoverers; // key：函数名，val：一组服务发现者
            std::unordered_map<BaseConnection::ptr, Discoverer::ptr> _conns;         // key：连接，val：服务发现者
        };



        // （服务）提供者和发现者的管理（注册中心的核心）
        class PDManager
        {
        public:
            using ptr=std::shared_ptr<PDManager>;

            PDManager()
                :_providers(std::make_shared<ProviderManager>()),
                _discoverers(std::make_shared<DiscovererManager>())
            {

            }

            // 处理服务请求（注册/发现）
            void onServiceRequest(const BaseConnection::ptr &conn, const ServiceRequest::ptr &msg)
            {
                ServiceOptype optype = msg->optype();

                if(optype == ServiceOptype::SERVICE_REGISTRY)   // 提供者注册服务
                {
                    _providers->addProvider(conn, msg->host(), msg->method());
                    _discoverers->onlineNotify(msg->method(), msg->host());
                    return registryResponse(conn, msg);
                }
                else if(optype == ServiceOptype::SERVICE_DISCOVERY) // 发现者查找范围
                {
                    _discoverers->addDiscoverer(conn, msg->method());
                    return discoveryResponse(conn, msg);
                }
                else
                {
                    ELOG("收到服务操作请求，但是操作类型错误！");
                    return errorResponse(conn, msg);
                }
            }

            // 连接断开
            void onConnShutdown(const BaseConnection::ptr &conn)
            {
                auto provider = _providers->getProvider(conn);

                // 如果是提供者要对他提供的服务进行下线通知
                if(provider.get() != nullptr)
                {
                    for(auto &method : provider->methods)
                    {
                        _discoverers->offlineNotify(method, provider->host);
                    }

                    _providers->delProvider(conn);
                }

                _discoverers->delDiscoverer(conn);
            }

        private:
            // 错误响应
            void errorResponse(const BaseConnection::ptr &conn, const ServiceRequest::ptr &msg)
            {
                auto msg_rsp = MessageFactory::create<ServiceResponse>();
                msg_rsp->setId(msg->rid());
                msg_rsp->setMType(MType::RSP_SERVICE);
                msg_rsp->setRCode(RCode::RCODE_INVALID_OPTYPE);
                msg_rsp->setOptype(ServiceOptype::SERVICE_UNKNOW);
                conn->send(msg_rsp);
            }

            // 注册成功响应
            void registryResponse(const BaseConnection::ptr &conn, const ServiceRequest::ptr &msg)
            {
                auto msg_rsp = MessageFactory::create<ServiceResponse>();
                msg_rsp->setId(msg->rid());
                msg_rsp->setMType(MType::RSP_SERVICE);
                msg_rsp->setRCode(RCode::RCODE_OK);
                msg_rsp->setOptype(ServiceOptype::SERVICE_REGISTRY);
                conn->send(msg_rsp);
            }

            // 服务发现响应
            void discoveryResponse(const BaseConnection::ptr &conn, const ServiceRequest::ptr &msg)
            {
                auto msg_rsp = MessageFactory::create<ServiceResponse>();
                msg_rsp->setId(msg->rid());
                msg_rsp->setMType(MType::RSP_SERVICE);
                msg_rsp->setOptype(ServiceOptype::SERVICE_DISCOVERY);

                // 找到函数名对应的提供者
                std::vector<Address> hosts = _providers->methodHosts(msg->method());

                if (hosts.empty())
                {
                    msg_rsp->setRCode(RCode::RCODE_NOT_FOUND_SERVICE);
                    return conn->send(msg_rsp);
                }
                
                msg_rsp->setRCode(RCode::RCODE_OK);
                msg_rsp->setMethod(msg->method());
                msg_rsp->setHost(hosts);
                return conn->send(msg_rsp);
            }

        private:
            ProviderManager::ptr _providers;     // 管理全部的提供者
            DiscovererManager::ptr _discoverers; // 管理全部的发现者
        };
    }
}