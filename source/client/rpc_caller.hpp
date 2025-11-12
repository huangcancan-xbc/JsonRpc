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

                // 3.等待响应
                auto rpc_rsp_msg = std::dynamic_pointer_cast<RpcResponse>(rsp_msg);
                if (!rpc_rsp_msg)
                {
                    ELOG("rpc响应，向下类型转换失败！");
                    return false;
                }

                if (rpc_rsp_msg->rcode() != RCode::RCODE_OK)
                {
                    ELOG("rpc请求出错：%s", errReason(rpc_rsp_msg->rcode()));
                    return false;
                }

                result = rpc_rsp_msg->result();
                return true;
            }

            bool call(const BaseConnection::ptr &conn, const std::string &method, const Json::Value &params, std::future<Json::Value> &result)
            {
                auto req_msg = MessageFactory::create<RpcRequest>();
                req_msg->setId(UUID::uuid());
                req_msg->setMType(MType::REQ_RPC);
                req_msg->setMethod(method);
                req_msg->setParams(params);

                auto json_promise = std::shared_ptr<std::promise<Json::Value>>();
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
                    ELOG("rpc回调请求出错：%s", errReason(rpc_rsp_msg->rcode()));
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
                    return;
                }

                if(rpc_rsp_msg->rcode() != RCode::RCODE_OK)
                {
                    ELOG("rpc异步请求出错：%s", errReason(rpc_rsp_msg->rcode()));
                    return;
                }

                result->set_value(rpc_rsp_msg->result());
            }

        private:
            Requestor::ptr _requestor;
        };
    }
}