/*
    管理rpc请求的发送和处理
    * 同步、异步、回调请求
    * rid映射rpc请求的完整对象
*/
#pragma once
#include "../common/net.hpp"
#include "../common/message.hpp"
#include <future>


namespace rpc
{
    namespace client
    {
        // rpc请求
        class Requestor
        {
        public:
            using ptr = std::shared_ptr<Requestor>;
            using RequestCallback = std::function<void(const BaseMessage::ptr &)>;
            using AsyncResponse = std::future<BaseMessage::ptr>;

            struct RequestDescribe
            {
                using ptr = std::shared_ptr<RequestDescribe>;

                BaseMessage::ptr request;                // 请求消息的对象
                RType rtype;                             // 请求类型（同步/异步/回调）
                std::promise<BaseMessage::ptr> response; // 用于异步返回
                RequestCallback callback;                // 回调函数
            };

            void onResponse(const BaseConnection::ptr &conn, BaseMessage::ptr &msg)
            {
                std::string rid = msg->rid();
                RequestDescribe::ptr rdp = getDescribe(rid);
                if(rdp.get() == nullptr)
                {
                    ELOG("收到响应 - %s，但是没有找到对应的请求描述！", rid.c_str());
                    return;
                }

                if(rdp->rtype == RType::REQ_ASYNC)
                {
                    rdp->response.set_value(msg);
                }
                else if(rdp->rtype == RType::REQ_CALLBACK)
                {
                    if(rdp->callback)
                    {
                        rdp->callback(msg);
                    }
                }
                else
                {
                    ELOG("请求类型未知！");
                }

                delDescribe(rid);
            }

            // 异步请求
            bool send(const BaseConnection::ptr &conn, const BaseMessage::ptr &req, AsyncResponse &async_rsp)
            {
                if (!conn || conn->connected() == false)
                {
                    ELOG("发送失败：连接不可用！");
                    return false;
                }

                RequestDescribe::ptr rdp = newDescribe(req, RType::REQ_ASYNC);
                if(rdp.get() == nullptr)
                {
                    ELOG("构造请求描述对象失败！");
                    return false;
                }

                conn->send(req);
                async_rsp = rdp->response.get_future();
                return true;
            }

            // 同步请求
            bool send(const BaseConnection::ptr &conn, const BaseMessage::ptr &req, BaseMessage::ptr &rsp)
            {
                AsyncResponse rsp_future;
                bool ret = send(conn, req, rsp_future);
                if(ret == false)
                {
                    return false;
                }

                // 同步请求增加超时，避免连接断开/响应丢失时无限阻塞
                // 超时时间保持较短，避免上层多次重试时累积成明显卡顿
                auto status = rsp_future.wait_for(std::chrono::seconds(1));
                if (status != std::future_status::ready)
                {
                    ELOG("同步请求等待响应超时: %s", req->rid().c_str());
                    delDescribe(req->rid());
                    return false;
                }

                rsp = rsp_future.get();
                return true;
            }

            // 回调请求
            bool send(const BaseConnection::ptr &conn, const BaseMessage::ptr &req, RequestCallback &cb)
            {
                if (!conn || conn->connected() == false)
                {
                    ELOG("发送失败：连接不可用！");
                    return false;
                }

                RequestDescribe::ptr rdp = newDescribe(req, RType::REQ_CALLBACK, cb);
                if(rdp.get() == nullptr)
                {
                    ELOG("构造请求描述对象失败！");
                    return false;
                }

                conn->send(req);
                return true;
            }

        private:
            RequestDescribe::ptr newDescribe(const BaseMessage::ptr &req, RType rtype, const RequestCallback &cb = RequestCallback())
            {
                RequestDescribe::ptr rd = std::make_shared<RequestDescribe>();
                rd->request = req;
                rd->rtype = rtype;
                if(rtype == RType::REQ_CALLBACK && cb)
                {
                    rd->callback = cb;
                }

                {
                    std::unique_lock<std::mutex> lock(_mutex);
                    _request_desc.insert(std::make_pair(req->rid(), rd));
                }

                return rd;
            }

            // 根据rid找对应的请求描述
            RequestDescribe::ptr getDescribe(const std::string &rid)
            {
                std::unique_lock<std::mutex> lock(_mutex);
                auto it = _request_desc.find(rid);
                if(it == _request_desc.end())
                {
                    return RequestDescribe::ptr();
                }

                return it->second;
            }

            // 删除已经处理完了的请求描述
            void delDescribe(const std::string &rid)
            {
                std::unique_lock<std::mutex> lock(_mutex);
                _request_desc.erase(rid);
            }

        private:
            std::mutex _mutex;

            // key：rid，val：请求描述对象，请求描述对象有点复杂，还是直接看示例吧：
            // rid: "12345" → RequestDescribe {
            //   request: {
            //     method: "add",
            //     params: {a: 11, b:22}
            //   },
            //   rtype: 异步/同步/回调,
            //   promise: 等待响应的容器,
            //   callback: 可选的回调函数
            // }
            std::unordered_map<std::string, RequestDescribe::ptr> _request_desc;
        };
    }
}
