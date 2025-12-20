/*
    服务提供者：向注册中心注册服务
    服务-主机管理：同一服务可能存在多个提供者（负载均衡）
    服务发现者：从注册中心发现服务并管理服务提供者
*/
#pragma once
#include "requestor.hpp"


namespace rpc
{
    namespace client
    {
        // 注册服务提供者到注册中心，并报告自己提供的服务和主机信息
        class Provider
        {
        public:
            using ptr = std::shared_ptr<Provider>;

            Provider(const Requestor::ptr &requestor)
                :_requestor(requestor)
            {
                
            }

            // 向注册中心注册某个服务，该服务由……主机提供
            bool registryMethod(const BaseConnection::ptr &conn, const std::string &method, const Address &host)
            {
                auto msg_req = MessageFactory::create<ServiceRequest>();
                msg_req->setId(UUID::uuid());
                msg_req->setMType(MType::REQ_SERVICE);
                msg_req->setMethod(method);
                msg_req->setHost(host);
                msg_req->setOptype(ServiceOptype::SERVICE_REGISTRY);
                BaseMessage::ptr msg_rsp;

                bool ret = _requestor->send(conn, msg_req, msg_rsp);
                if(ret == false)
                {
                    ELOG("%s 服务注册失败！", method.c_str());
                    return false;
                }

                auto service_rsp = std::dynamic_pointer_cast<ServiceResponse>(msg_rsp);
                if(service_rsp.get() == nullptr)
                {
                    ELOG("响应类型向下转型失败！");
                    return false;
                }

                if(service_rsp->rcode() != RCode::RCODE_OK)
                {
                    ELOG("服务注册失败，原因：%s", errReason(service_rsp->rcode()).c_str());
                    return false;
                }

                return true;
            }

        private:
            Requestor::ptr _requestor; // 用于发送请求、接收响应
        };



        // 将rpc方法对应多个提供服务的主机进行管理
        class MethodHost
        {
        public:
            using ptr = std::shared_ptr<MethodHost>;

            MethodHost()
                :_idx(0)
            {
                
            }

            MethodHost(const std::vector<Address> &hosts)
                : _hosts(hosts.begin(), hosts.end()),
                  _idx(0)
            {
                
            }

            // 添加一个主机信息（服务上线）
            void appendHost(const Address &host)
            {
                // 中途收到服务上线请求后被调用
                std::unique_lock<std::mutex> lock(_mutex);
                _hosts.push_back(host);
            }

            // 删除一个主机信息（服务下线）
            void removeHost(const Address &host)
            {
                // 中途收到服务下线请求后被调用
                std::unique_lock<std::mutex> lock(_mutex);
                for (auto it = _hosts.begin(); it != _hosts.end(); it++)
                {
                    if(*it == host)
                    {
                        _hosts.erase(it);
                        break;
                    }
                }
            }

            Address chooseHost()
            {
                std::unique_lock<std::mutex> lock(_mutex);

                // 我发现如果下面的_hosts.size()返回0就会产生除0错误，所以特地添加一个检查
                // 即使下面的Discoverer::serviceDiscovery()方法已经检查了empty()
                // 我也愿意用这一点低成本换来较大的安全性，算是一个小安全优化吧
                if (_hosts.empty())
                {
                    return Address();
                }

                size_t pos = _idx++ % _hosts.size();
                return _hosts[pos];
            }

            // 看看是否有可用的主机
            bool empty()
            {
                std::unique_lock<std::mutex> lock(_mutex);
                return _hosts.empty();
            }

        private:
            std::mutex _mutex;
            size_t _idx;                 // 轮询的下标索引
            std::vector<Address> _hosts; // 主机信息数组/列表
        };

        

        // 客户端服务发现、上下线通知的管理
        class Discoverer
        {
        public:
            using OfflineCallback = std::function<void(const Address&)>;
            using ptr = std::shared_ptr<Discoverer>;

            Discoverer(const Requestor::ptr &requestor, const OfflineCallback &cb)
                :_requestor(requestor),
                _offline_callback(cb)
            {
                
            }

            // 服务发现，如果本地已经有主机列表就直接返回，反之就发起请求，请求注册中心提供主机列表
            bool serviceDiscovery(const BaseConnection::ptr &conn, const std::string &method, Address &host)
            {
                {
                    // 当前所保管的提供者信息存在，则直接返回地址
                    std::unique_lock<std::mutex> lock(_mutex);
                    auto it = _method_hosts.find(method);
                    if (it != _method_hosts.end())
                    {
                        if (it->second->empty() == false)
                        {
                            host = it->second->chooseHost();
                            return true;
                        }
                    }
                }

                // 当前服务的提供者为空，发起SERVICE_DISCOVERY请求
                auto msg_req = MessageFactory::create<ServiceRequest>();
                msg_req->setId(UUID::uuid());
                msg_req->setMType(MType::REQ_SERVICE);
                msg_req->setMethod(method);
                msg_req->setOptype(ServiceOptype::SERVICE_DISCOVERY);
                BaseMessage::ptr msg_rsp;
                bool ret = _requestor->send(conn, msg_req, msg_rsp);
                if (ret == false)
                {
                    ELOG("服务发现失败！");
                    return false;
                }

                auto service_rsp = std::dynamic_pointer_cast<ServiceResponse>(msg_rsp);
                if (!service_rsp)
                {
                    ELOG("服务发现失败！响应类型转换失败！");
                    return false;
                }

                if (service_rsp->rcode() != RCode::RCODE_OK)
                {
                    ELOG("服务发现失败！%s", errReason(service_rsp->rcode()).c_str());
                    return false;
                }

                // 能走到这里，代表当前是没有对应的服务提供主机的
                std::unique_lock<std::mutex> lock(_mutex);
                auto method_host = std::make_shared<MethodHost>(service_rsp->hosts());
                if (method_host->empty())
                {
                    ELOG("%s 服务发现失败！没有能够提供服务的主机！", method.c_str());
                    return false;
                }

                host = method_host->chooseHost();
                _method_hosts[method] = method_host;
                return true;
            }

            // 这个接口是提供给Dispatcher模块进行服务上线下线请求处理的回调函数（收到SERVICE_ONLINE/SERVICE_OFFLINE时调用）
            void onServiceRequest(const BaseConnection::ptr &conn, const ServiceRequest::ptr &msg)
            {
                // 1. 判断是上线还是下线请求，如果都不是那就不用处理了
                auto optype = msg->optype();
                std::string method = msg->method();
                std::unique_lock<std::mutex> lock(_mutex);
                if (optype == ServiceOptype::SERVICE_ONLINE)
                {
                    // 2. 上线请求：找到MethodHost，向其中新增一个主机地址
                    auto it = _method_hosts.find(method);
                    if (it == _method_hosts.end())
                    {
                        auto method_host = std::make_shared<MethodHost>();
                        method_host->appendHost(msg->host());
                        _method_hosts[method] = method_host;
                    }
                    else
                    {
                        it->second->appendHost(msg->host());
                    }
                }
                else if (optype == ServiceOptype::SERVICE_OFFLINE)
                {
                    // 3. 下线请求：找到MethodHost，从其中删除一个主机地址，并触发下线回调
                    auto it = _method_hosts.find(method);
                    if (it == _method_hosts.end())
                    {
                        return;
                    }
                    it->second->removeHost(msg->host());
                    _offline_callback(msg->host());
                }
            }

        private:
            OfflineCallback _offline_callback; // 当主机下线要进行通知
            std::mutex _mutex;
            std::unordered_map<std::string, MethodHost::ptr> _method_hosts; // key：服务，val：主机列表
            Requestor::ptr _requestor;                                      // 用于发送服务发现请求
        };
    }
}