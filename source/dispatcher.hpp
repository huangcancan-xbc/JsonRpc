#pragma once
#include "net.hpp"
#include "message.hpp"

namespace rpc
{
    class Dispatcher
    {
    public:
        using ptr = std::shared_ptr<Dispatcher>;

        void registerHandler(MType mtype, const MessageCallback &handler)
        {
            std::unique_lock<std::mutex> lock(_mutex);
            _handlers.insert(std::make_pair(mtype, handler));
        }

        void onMessage(const BaseConnection::ptr &conn, BaseMessage::ptr &msg)
        {
            // 找到消息类型对应的业务处理函数，直接进行调用
            std::unique_lock<std::mutex> lock(_mutex);
            auto it = _handlers.find(msg->mtype());
            if(it != _handlers.end())
            {
                return it->second(conn, msg);
            }

            // 没有找到指定类型的处理回调，说明这个消息并不是自己的，属于未知，这里选择直接关闭了
            ELOG("收到了未知类型的消息！");
            conn->shutdown();
        }

    private:
        std::mutex _mutex;
        std::unordered_map<MType, MessageCallback> _handlers;
    };
}