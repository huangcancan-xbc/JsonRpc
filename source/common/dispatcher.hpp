/*
    这是一个通用的分发模块
    * 将rpc请求分发到相应的处理函数：RpcRequest到onRpcRequest处理具体业务逻辑
    * 分发服务注册和发现的请求：ServiceRequest到PDManager::onServiceRequest进行处理
    * 分发主题订阅、发布：TopicRequest到TopicManager::onTopicRequest处理
    * 分发注册中心的响应：注册中心响应到Requestor::onResponse处理
*/
#pragma once
#include "net.hpp"
#include "message.hpp"

namespace rpc
{
    class Callback
    {
    public:
        using ptr = std::shared_ptr<Callback>;
        virtual void onMessage(const BaseConnection::ptr &conn, BaseMessage::ptr &msg) = 0;
    };

    template<typename T>
    class CallbackT : public Callback
    {
    public:
        using ptr = std::shared_ptr<CallbackT<T>>;
        using MessageCallback = std::function<void(const BaseConnection::ptr &conn, std::shared_ptr<T> &msg)>;

        CallbackT(const MessageCallback &handler) : _handler(handler) {}

        void onMessage(const BaseConnection::ptr &conn, BaseMessage::ptr &msg) override
        {
            auto type_msg = std::dynamic_pointer_cast<T>(msg);
            _handler(conn, type_msg);
        }
    private:
        MessageCallback _handler;   // 具体的处理函数，由我们进行提供
    };



    // 具体的分发器，实现类似下面的功能（我也不知道怎么描述了，直接看示例吧）：
    // 消息类型1(登录) → 处理登录的函数
    // 消息类型2(查询) → 处理查询的函数
    // 消息类型3(注册) → 处理注册的函数
    class Dispatcher
    {
    public:
        using ptr = std::shared_ptr<Dispatcher>;

        template <typename T>
        void registerHandler(MType mtype, const typename CallbackT<T>::MessageCallback &handler)
        {
            std::unique_lock<std::mutex> lock(_mutex);
            auto cb = std::make_shared<CallbackT<T>>(handler);
            _handlers.insert(std::make_pair(mtype, cb));
        }

        void onMessage(const BaseConnection::ptr &conn, BaseMessage::ptr &msg)
        {
            // 找到消息类型对应的业务处理函数，直接进行调用
            std::unique_lock<std::mutex> lock(_mutex);
            auto it = _handlers.find(msg->mtype());
            if(it != _handlers.end())
            {
                return it->second->onMessage(conn, msg);
            }

            // 没有找到指定类型的处理回调，说明这个消息并不是自己的，属于未知，这里选择直接关闭了
            ELOG("收到了未知类型的消息: %d", static_cast<int>(msg->mtype()));
            conn->shutdown();
        }

    private:
        std::mutex _mutex;
        std::unordered_map<MType, Callback::ptr> _handlers;     // key：消息类型，val：处理函数
    };
}