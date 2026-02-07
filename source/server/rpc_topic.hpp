/*
    处理主题：创建、删除、订阅、取消订阅、发布
*/
#pragma once
#include "../common/net.hpp"
#include "../common/message.hpp"
#include <unordered_set>
#include <vector>


namespace rpc
{
    namespace server
    {
        // 管理主题
        class TopicManager
        {
        public:
            using ptr = std::shared_ptr<TopicManager>;

            TopicManager()
            {
            }

            // 处理主题请求
            void onTopicRequest(const BaseConnection::ptr &conn, const TopicRequest::ptr &msg)
            {
                TopicOptype topic_optype = msg->optype();
                bool ret = true;

                switch (topic_optype)
                {
                case TopicOptype::TOPIC_CREATE:
                    topicCreate(conn, msg);
                    break;
                case TopicOptype::TOPIC_REMOVE:
                    ret = topicRemove(conn, msg);
                    break;
                case TopicOptype::TOPIC_SUBSCRIBE:
                    // 订阅失败（例如主题不存在）需要反馈给上层，不能吞掉返回值
                    ret = topicSubscribe(conn, msg);
                    break;
                case TopicOptype::TOPIC_CANCEL:
                    ret = topicCancel(conn, msg);
                    break;
                case TopicOptype::TOPIC_PUBLISH:
                    ret = topicPublish(conn, msg);
                    break;
                default:
                    return errorResponse(conn, msg, RCode::RCODE_INVALID_OPTYPE);
                }

                if (!ret)
                {
                    return errorResponse(conn, msg, RCode::RCODE_NOT_FOUND_TOPIC);
                }

                return topicResponse(conn, msg);
            }

            // 处理连接关闭
            void onShutdown(const BaseConnection::ptr &conn)
            {
                std::vector<Topic::ptr> topics;
                Subscriber::ptr subscriber;

                {
                    std::unique_lock<std::mutex> lock(_mutex);
                    auto it = _subscribers.find(conn);
                    if (it == _subscribers.end())
                    {
                        return;
                    }

                    subscriber = it->second;
                    // 先拿订阅列表快照，避免无锁遍历 Subscriber::topics 产生并发竞态
                    auto topic_names = subscriber->listTopics();
                    for (auto &topic_name : topic_names)
                    {
                        auto topic_it = _topics.find(topic_name);
                        if (topic_it == _topics.end())
                        {
                            continue;
                        }

                        topics.push_back(topic_it->second);
                    }

                    _subscribers.erase(it);
                }

                for (auto &topic : topics)
                {
                    topic->removeSubscriber(subscriber);
                }
            }

        private:
            // 返回错误响应
            void errorResponse(const BaseConnection::ptr &conn, const TopicRequest::ptr &msg, RCode rcode)
            {
                auto msg_rsp = MessageFactory::create<TopicResponse>();
                msg_rsp->setId(msg->rid());
                msg_rsp->setMType(MType::RSP_TOPIC);
                msg_rsp->setRCode(rcode);
                conn->send(msg_rsp);
            }

            // 返回成功响应
            void topicResponse(const BaseConnection::ptr &conn, const TopicRequest::ptr &msg)
            {
                auto msg_rsp = MessageFactory::create<TopicResponse>();
                msg_rsp->setId(msg->rid());
                msg_rsp->setMType(MType::RSP_TOPIC);
                msg_rsp->setRCode(RCode::RCODE_OK);
                conn->send(msg_rsp);
            }

            // 创建主题
            void topicCreate(const BaseConnection::ptr &conn, const TopicRequest::ptr &msg)
            {
                std::unique_lock<std::mutex> lock(_mutex);
                // 构造一个主题对象，对其添加映射关系进行管理
                std::string topic_name = msg->topicKey();
                auto topic = std::make_shared<Topic>(topic_name);
                _topics.insert(std::make_pair(topic_name, topic));
            }

            // 删除主题
            bool topicRemove(const BaseConnection::ptr &conn, const TopicRequest::ptr &msg)
            {
                std::string topic_name = msg->topicKey();
                std::unordered_set<Subscriber::ptr> subscribers;

                {
                    std::unique_lock<std::mutex> lock(_mutex);
                    // 删除主题之前要先找出会受到影响的订阅者
                    auto it = _topics.find(topic_name);
                    if (it == _topics.end())
                    {
                        // 保持与发布/订阅一致：主题不存在时返回失败
                        return false;
                    }

                    // 先拿订阅者快照，避免无锁读取 Topic::subscribers 产生并发竞态
                    subscribers = it->second->listSubscribers();
                    _topics.erase(it);
                }

                for (auto &subscriber : subscribers)
                {
                    subscriber->removeTopic(topic_name);
                }

                return true;
            }

            // 订阅主题
            bool topicSubscribe(const BaseConnection::ptr &conn, const TopicRequest::ptr &msg)
            {
                Topic::ptr topic;
                Subscriber::ptr subscriber;

                {
                    std::unique_lock<std::mutex> lock(_mutex);
                    auto topic_it = _topics.find(msg->topicKey());
                    if (topic_it == _topics.end())
                    {
                        return false;
                    }

                    topic = topic_it->second;
                    auto sub_it = _subscribers.find(conn);
                    if (sub_it != _subscribers.end())
                    {
                        subscriber = sub_it->second;
                    }
                    else
                    {
                        subscriber = std::make_shared<Subscriber>(conn);
                        _subscribers.insert(std::make_pair(conn, subscriber));
                    }
                }

                topic->appendSubscriber(subscriber);
                subscriber->appendTopic(msg->topicKey());
                return true;
            }

            // 取消订阅主题
            bool topicCancel(const BaseConnection::ptr &conn, const TopicRequest::ptr &msg)
            {
                Topic::ptr topic;
                Subscriber::ptr subscriber;

                {
                    std::unique_lock<std::mutex> lock(_mutex);
                    auto topic_it = _topics.find(msg->topicKey());
                    if (topic_it == _topics.end())
                    {
                        return false;
                    }
                    topic = topic_it->second;

                    auto sub_it = _subscribers.find(conn);
                    if (sub_it != _subscribers.end())
                    {
                        subscriber = sub_it->second;
                    }
                    else
                    {
                        // 连接不存在订阅者信息时，同样返回失败，避免误报成功
                        return false;
                    }
                }

                subscriber->removeTopic(msg->topicKey());
                topic->removeSubscriber(subscriber);
                return true;
            }

            // 发布消息
            bool topicPublish(const BaseConnection::ptr &conn, const TopicRequest::ptr &msg)
            {
                Topic::ptr topic;

                {
                    std::unique_lock<std::mutex> lock(_mutex);
                    auto topic_it = _topics.find(msg->topicKey());
                    if (topic_it == _topics.end())
                    {
                        return false;
                    }

                    topic = topic_it->second;
                }

                topic->pushMessage(msg);
                return true;
            }

        private:
            struct Subscriber
            {
                using ptr = std::shared_ptr<Subscriber>;
                std::mutex _mutex;
                BaseConnection::ptr conn;               // 存储订阅者连接对象
                std::unordered_set<std::string> topics; // 订阅者订阅的主题名称

                Subscriber(const BaseConnection::ptr &c)
                    : conn(c)
                {
                }

                // 订阅主题的时候调用，将主题添加到订阅者列表中
                void appendTopic(const std::string &topic_name)
                {
                    std::unique_lock<std::mutex> lock(_mutex);
                    topics.insert(topic_name);
                }

                // 主题被删除或者取消订阅的时候调用，从订阅者的订阅列表中移除主题
                void removeTopic(const std::string &topic_name)
                {
                    std::unique_lock<std::mutex> lock(_mutex);
                    topics.erase(topic_name);
                }

                // 返回订阅主题快照，供外部安全遍历
                std::vector<std::string> listTopics()
                {
                    std::unique_lock<std::mutex> lock(_mutex);
                    return std::vector<std::string>(topics.begin(), topics.end());
                }
            };

            struct Topic
            {
                using ptr = std::shared_ptr<Topic>;
                std::mutex _mutex;
                std::string topic_name;                          // 主题名字
                std::unordered_set<Subscriber::ptr> subscribers; // 当前主题的订阅者

                Topic(const std::string &name)
                    : topic_name(name)
                {
                }

                // 添加订阅者：新增订阅的时候进行调用
                void appendSubscriber(const Subscriber::ptr &subscriber)
                {
                    std::unique_lock<std::mutex> lock(_mutex);
                    subscribers.insert(subscriber);
                }

                // 移除订阅者：取消订阅或者订阅者连接断开的时候调用
                void removeSubscriber(const Subscriber::ptr &subscriber)
                {
                    std::unique_lock<std::mutex> lock(_mutex);
                    subscribers.erase(subscriber);
                }

                // 发布消息：收到消息发布请求的时候调用
                void pushMessage(const BaseMessage::ptr &msg)
                {
                    std::unique_lock<std::mutex> lock(_mutex);
                    for (auto &subscriber : subscribers)
                    {
                        subscriber->conn->send(msg);
                    }
                }

                // 返回订阅者快照，供外部安全遍历
                std::unordered_set<Subscriber::ptr> listSubscribers()
                {
                    std::unique_lock<std::mutex> lock(_mutex);
                    return subscribers;
                }
            };

        private:
            std::mutex _mutex;
            std::unordered_map<std::string, Topic::ptr> _topics;                   // key：主题名称，val：主题对象
            std::unordered_map<BaseConnection::ptr, Subscriber::ptr> _subscribers; // key：连接（客户端），val：订阅者对象
        };
    }
}