#pragma once
#include "requestor.hpp"


namespace rpc
{
    namespace client
    {
        class RpcCaller
        {
        public:
            using ptr = std::shared_ptr<RpcCaller>;
            using JsonAsyncResponse = std::future<Json::Value>;
            using JsonResponseCallback = std::function<void(const Json::Value &)>;

            RpcCaller(const Requestor::ptr &requestor)
                :_requestor(requestor)
            {
                
            }

            bool call(const BaseConnection::ptr &conn, const std::string &method, const Json::Value &params, Json::Value &result)
            {
                DLOG("开始同步rpc调用！");
                // 1.组织请求
                auto req_msg = MessageFactory::create<RpcRequest>();
                req_msg->setId(UUID::uuid());
                req_msg->setMType(MType::REQ_RPC);
                req_msg->setMethod(method);
                req_msg->setParams(params);
                BaseMessage::ptr rsp_msg;

                // 2.发送请求
                bool ret = _requestor->send(conn, std::dynamic_pointer_cast<BaseMessage>(req_msg), rsp_msg);
                if(ret == false)
                {
                    ELOG("同步Rpc请求失败！");
                    return false;
                }
                DLOG("收到响应，进行解析，获取结果！");

                // 3.等待响应
                auto rpc_rsp_msg = std::dynamic_pointer_cast<RpcResponse>(rsp_msg);
                if (!rpc_rsp_msg)
                {
                    ELOG("rpc响应，向下类型转换失败！");
                    return false;
                }

                if (rpc_rsp_msg->rcode() != RCode::RCODE_OK)
                {
                    ELOG("rpc请求出错：%s", errReason(rpc_rsp_msg->rcode()).c_str());
                    return false;
                }

                result = rpc_rsp_msg->result();
                DLOG("结果设置完成！");
                return true;
            }

            bool call(const BaseConnection::ptr &conn, const std::string &method, const Json::Value &params, std::future<Json::Value> &result)
            {
                auto req_msg = MessageFactory::create<RpcRequest>();
                req_msg->setId(UUID::uuid());
                req_msg->setMType(MType::REQ_RPC);
                req_msg->setMethod(method);
                req_msg->setParams(params);

                auto json_promise = std::make_shared<std::promise<Json::Value>>();
                result = json_promise->get_future();
                Requestor::RequestCallback cb = std::bind(&RpcCaller::Callback, this, json_promise, std::placeholders::_1);
                bool ret = _requestor->send(conn, std::dynamic_pointer_cast<BaseMessage>(req_msg), cb);
                if(ret == false)
                {
                    ELOG("同步Rpc请求失败！");
                    return false;
                }

                return true;
            }

            bool call(const BaseConnection::ptr &conn, const std::string &method, const Json::Value &params, const JsonResponseCallback &cb)
            {
                auto req_msg = MessageFactory::create<RpcRequest>();
                req_msg->setId(UUID::uuid());
                req_msg->setMType(MType::REQ_RPC);
                req_msg->setMethod(method);
                req_msg->setParams(params);

                Requestor::RequestCallback req_cb = std::bind(&RpcCaller::Callback1, this, cb, std::placeholders::_1);
                bool ret = _requestor->send(conn, std::dynamic_pointer_cast<BaseMessage>(req_msg), req_cb);
                if (ret == false)
                {
                    ELOG("回调Rpc请求失败！");
                    return false;
                }
                
                return true;
            }

        private:
            void Callback1(const JsonResponseCallback &cb, const BaseMessage::ptr &msg)
            {
                auto rpc_rsp_msg = std::dynamic_pointer_cast<RpcResponse>(msg);
                if (!rpc_rsp_msg)
                {
                    ELOG("rpc响应，向下类型转换失败！");
                    return;
                }
                
                if (rpc_rsp_msg->rcode() != RCode::RCODE_OK)
                {
                    ELOG("rpc回调请求出错：%s", errReason(rpc_rsp_msg->rcode()).c_str());
                    // 回调路径，尝试回传空或者错误信息，这里传空对象表示失败
                    cb(Json::Value());
                    return;
                }

                cb(rpc_rsp_msg->result());
            }

            void Callback(std::shared_ptr<std::promise<Json::Value>> result, const BaseMessage::ptr &msg)
            {
                auto rpc_rsp_msg = std::dynamic_pointer_cast<RpcResponse>(msg);
                if(!rpc_rsp_msg)
                {
                    ELOG("rpc响应，向下类型转换失败！");
                    try
                    {
                        throw std::runtime_error("rpc响应转换失败！");
                    }
                    catch(...)
                    {
                        result->set_exception(std::current_exception());
                    }

                    return;
                }

                if(rpc_rsp_msg->rcode() != RCode::RCODE_OK)
                {
                    ELOG("rpc异步请求出错：%s", errReason(rpc_rsp_msg->rcode()).c_str());
                    // 将错误信息包装进异常，通知等待
                    try
                    {
                        throw std::runtime_error("rpc error: " + errReason(rpc_rsp_msg->rcode()));
                    }
                    catch (...)
                    {
                        result->set_exception(std::current_exception());
                    }
                    return;
                }

                result->set_value(rpc_rsp_msg->result());
            }

        private:
            Requestor::ptr _requestor;
        };
    }
}