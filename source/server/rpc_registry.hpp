#pragma once
#include "../common/net.hpp"
#include "../common/message.hpp"
#include <set>


namespace rpc
{
    namespace server
    {
        class ProviderManger
        {
        public:
            using ptr = std::shared_ptr<ProviderManger>;
            struct Provider
            {
                using ptr = std::shared_ptr<Provider>;

                std::mutex _mutex;
                BaseConnection::ptr conn;
                Address host;
                std::vector<std::string> methods;
                Provider(const BaseConnection::ptr &c, const Address &h)
                    :conn(c),host(h)
                {
                    
                }

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

                // 如果是服务提供者，看看具体提供的服务是什么，从提供信息中删除当前服务提供者
                for(auto &method:it->second->methods)
                {
                    auto &providers = _providers[method];
                    providers.erase(it->second);
                }

                // 删除连接与服务提供者的关联关系
                _conns.erase(it);
            }

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
            std::unordered_map<std::string, std::set<Provider::ptr>> _providers;
            std::unordered_map<BaseConnection::ptr, Provider::ptr> _conns;
        };



        class DiscovererManager
        {
        public:
            using ptr = std::shared_ptr<DiscovererManager>;

            struct Discoverer
            {
                using ptr = std::shared_ptr<Discoverer>;
                std::mutex _mutex;
                BaseConnection::ptr conn;        // 发现关联的客户端连接
                std::vector<std::string> methods; // 发现过的服务名称
                Discoverer(const BaseConnection::ptr &c)
                    :conn(c)
                {
                    
                }

                void appendMethod(const std::string &method)
                {
                    std::unique_lock<std::mutex> lock(_mutex);
                    methods.push_back(method);
                }
            };

            // 当每次客户端进行服务发现的时候新增发现者，新增服务名称
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

            // 发现者客户端断开连接时，找到发现者的信息，删除关联的数据
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
                    auto discoverers = _discoverers[method];
                    discoverers.erase(it->second);
                }

                _conns.erase(it);
            }

            // 当一个新的服务提供者上线时，进行上线通知
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
            std::unordered_map<std::string, std::set<Discoverer::ptr>> _discoverers;
            std::unordered_map<BaseConnection::ptr, Discoverer::ptr> _conns;
        };



        class PDManager
        {
        public:
            using ptr=std::shared_ptr<PDManager>;

            PDManager()
                :_providers(std::make_shared<ProviderManger>()),
                _discoverers(std::make_shared<DiscovererManager>())
            {

            }

            void onServiceRequest(const BaseConnection::ptr &conn, const ServiceRequest::ptr &msg)
            {
                ServiceOptype optype = msg->optype();
                if(optype == ServiceOptype::SERVICE_REGISTRY)
                {
                    _providers->addProvider(conn, msg->host(), msg->method());
                    _discoverers->onlineNotify(msg->method(), msg->host());
                    return registryRespose(conn, msg);
                }
                else if(optype == ServiceOptype::SERVICE_DISCOVERY)
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

            void onConnShutdown(const BaseConnection::ptr &conn)
            {
                auto provider = _providers->getProvider(conn);
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
            void errorResponse(const BaseConnection::ptr &conn, const ServiceRequest::ptr &msg)
            {
                auto msg_rsp = MessageFactory::create<ServiceResponse>();
                msg_rsp->setId(msg->rid());
                msg_rsp->setMType(MType::RSP_SERVICE);
                msg_rsp->setRCode(RCode::RCODE_INVALID_OPTYPE);
                msg_rsp->setOptype(ServiceOptype::SERVICE_UNKNOW);
                conn->send(msg_rsp);
            }

            void registryRespose(const BaseConnection::ptr &conn, const ServiceRequest::ptr &msg)
            {
                auto msg_rsp = MessageFactory::create<ServiceResponse>();
                msg_rsp->setId(UUID::uuid());
                msg_rsp->setMType(MType::REQ_SERVICE);
                msg_rsp->setRCode(RCode::RCODE_OK);
                msg_rsp->setOptype(ServiceOptype::SERVICE_REGISTRY);
                conn->send(msg_rsp);
            }

            void discoveryResponse(const BaseConnection::ptr &conn, const ServiceRequest::ptr &msg)
            {
                auto msg_rsp = MessageFactory::create<ServiceResponse>();
                msg_rsp->setId(msg->rid());
                msg_rsp->setMType(MType::RSP_SERVICE);
                msg_rsp->setOptype(ServiceOptype::SERVICE_DISCOVERY);
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
            ProviderManger::ptr _providers;
            DiscovererManager::ptr _discoverers;
        };
    }
}