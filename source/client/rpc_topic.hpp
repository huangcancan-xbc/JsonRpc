/*
    主题请求：创建、删除、订阅、取消订阅、发布
*/
#pragma once
#include "requestor.hpp"
#include <unordered_set>


namespace rpc
{
    namespace client
    {
        // 主题管理
        class TopicManager
        {
        public:
            using SubCallback = std::function<void(const std::string &key, const std::string &msg)>;
            using ptr = std::shared_ptr<TopicManager>;

            TopicManager(const Requestor::ptr &requestor)
                : _requestor(requestor)
            {
            }

            bool create(const BaseConnection::ptr &conn, const std::string &key)
            {
                return commonRequest(conn, key, TopicOptype::TOPIC_CREATE);
            }

            bool remove(const BaseConnection::ptr &conn, const std::string &key)
            {
                return commonRequest(conn, key, TopicOptype::TOPIC_REMOVE);
            }

            bool subscribe(const BaseConnection::ptr &conn, const std::string &key, const SubCallback &cb)
            {
                addSubscribe(key, cb);

                bool ret = commonRequest(conn, key, TopicOptype::TOPIC_SUBSCRIBE);
                if (ret == false)
                {
                    delSubscribe(key);
                    return false;
                }

                return true;
            }

            bool cancel(const BaseConnection::ptr &conn, const std::string &key)
            {
                delSubscribe(key);
                return commonRequest(conn, key, TopicOptype::TOPIC_CANCEL);
            }

            bool publish(const BaseConnection::ptr &conn, const std::string &key, const std::string &msg)
            {
                return commonRequest(conn, key, TopicOptype::TOPIC_PUBLISH, msg);
            }

            void onPublish(const BaseConnection::ptr &conn, const TopicRequest::ptr &msg)
            {
                // 先判断消息类型是否属于请求
                auto type = msg->optype();
                if (type != TopicOptype::TOPIC_PUBLISH)
                {
                    ELOG("收到了错误类型的主题操作！");
                    return;
                }

                // 取出消息主题名称和内容
                std::string topic_key = msg->topicKey();
                std::string topic_msg = msg->topicMsg();

                // 通过主题找到对应的回调函数进行处理，有则处理，无则报错
                auto callback = getSubscribe(topic_key);
                if (!callback)
                {
                    ELOG("收到了 %s 主题消息，但是该消息无主题处理回调函数！", topic_key.c_str());
                    return;
                }

                return callback(topic_key, topic_msg);
            }

        private:
            // 添加订阅
            void addSubscribe(const std::string &key, const SubCallback &cb)
            {
                std::unique_lock<std::mutex> lock(_mutex);
                _topic_callbacks.insert(std::make_pair(key, cb));
            }

            // 删除订阅
            void delSubscribe(const std::string &key)
            {
                std::unique_lock<std::mutex> lock(_mutex);
                _topic_callbacks.erase(key);
            }

            // 获取订阅回调
            const SubCallback getSubscribe(const std::string &key)
            {
                std::unique_lock<std::mutex> lock(_mutex);
                auto it = _topic_callbacks.find(key);
                if (it == _topic_callbacks.end())
                {
                    return SubCallback();
                }

                return it->second;
            }

            // 发送请求：目标服务器连接对象、主题名称、主题操作、消息
            bool commonRequest(const BaseConnection::ptr &conn, const std::string &key, TopicOptype type, const std::string &msg = "")
            {
                // 1. 构造请求对象，并填充数据
                auto msg_req = MessageFactory::create<TopicRequest>();
                msg_req->setId(UUID::uuid());
                msg_req->setMType(MType::REQ_TOPIC);
                msg_req->setOptype(type);
                msg_req->setTopicKey(key);
                if (type == TopicOptype::TOPIC_PUBLISH)
                {
                    msg_req->setTopicMsg(msg);
                }

                // 2. 向服务端发送请求，等待响应
                BaseMessage::ptr msg_rsp;
                bool ret = _requestor->send(conn, msg_req, msg_rsp);
                if (ret == false)
                {
                    ELOG("主题操作请求失败！");
                    return false;
                }

                // 3. 判断请求处理是否成功
                auto topic_rsp_msg = std::dynamic_pointer_cast<TopicResponse>(msg_rsp);
                if (!topic_rsp_msg)
                {
                    ELOG("主题操作响应，向下类型转换失败！");
                    return false;
                }

                if (topic_rsp_msg->rcode() != RCode::RCODE_OK)
                {
                    ELOG("主题操作请求出错：%s", errReason(topic_rsp_msg->rcode()).c_str());
                    return false;
                }

                return true;
            }

        private:
            std::mutex _mutex;
            std::unordered_map<std::string, SubCallback> _topic_callbacks; // key：主题，val：对应回调
            Requestor::ptr _requestor;                                     // rpc远端服务通信
        };
    }
}