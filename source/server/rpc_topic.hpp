#pragma once
#include "../common/net.hpp"
#include "../common/message.hpp"
#include <unordered_set>


namespace rpc
{
    namespace server
    {
        class TopicManager
        {
        public:
            using ptr = std::shared_ptr<TopicManager>;

            TopicManager()
            {

            }

            void onTopicRequest(const BaseConnection::ptr &conn, const TopicRequest::ptr &msg);
            void onShutdown(const BaseConnection::ptr &conn);
        private:
            void topicCreate(const BaseConnection::ptr &conn, const TopicRequest::ptr &msg)
            {
                std::unique_lock<std::mutex> lock(_mutex);
                // 构造一个主题对象，对其添加映射关系进行管理
                std::string topic_name = msg->topicKey();
                auto topic = std::make_shared<Topic>(topic_name);
                _topics.insert(std::make_pair(topic_name, topic));
            }

            void topicRemove(const BaseConnection::ptr &conn, const TopicRequest::ptr &msg)
            {
                std::string topic_name = msg->topicKey();
                std::vector<Subscriber::ptr> subscribers;

                {
                    std::unique_lock<std::mutex> lock(_mutex);
                    // 删除主题之前要先找出会受到影响的订阅者
                    auto it = _topics.find(topic_name);
                    if(it == _topics.end())
                    {
                        return;
                    }

                    for(auto &conn : it->second->subscribers)
                    {
                        auto it_sub = _subscribers.find(conn);
                        if(it_sub ==_subscribers.end())
                        {
                            continue;
                        }

                        subscribers.push_back(it_sub->second);
                    }

                    _topics.erase(it);
                }

                for(auto &subscriber : subscribers)
                {
                    subscriber->removeTopic(topic_name);
                }
            }

            bool topicSubscriber(const BaseConnection::ptr &conn, const TopicRequest::ptr &msg)
            {
                Topic::ptr topic;
                Subscriber::ptr subscriber;

                {
                    std::unique_lock<std::mutex> lock(_mutex);
                    auto topic_it = _topics.find(msg->topicKey());
                    if(topic_it == _topics.end())
                    {
                        return false;
                    }

                    topic = topic_it->second;
                    auto sub_it = _subscribers.find(conn);
                    if(sub_it == _subscribers.end())
                    {
                        subscriber = sub_it->second;
                    }
                    else
                    {
                        subscriber=std::make_shared<Subscriber>(conn);
                        _subscribers.insert(std::make_pair(conn, subscriber));
                    }
                }

                topic->appendSubscriber(conn);
                subscriber->appendTopic(msg->topicKey());
                return true;
            }

            void topicCancel(const BaseConnection::ptr &conn, const TopicRequest::ptr &msg)
            {
                Topic::ptr topic;
                Subscriber::ptr subscriber;

                {
                    std::unique_lock<std::mutex> lock(_mutex);
                    auto topic_it = _topics.find(msg->topicKey());
                    if (topic_it != _topics.end())
                    {
                        topic = topic_it->second;
                    }

                    auto sub_it = _subscribers.find(conn);
                    if (sub_it == _subscribers.end())
                    {
                        subscriber = sub_it->second;
                    }
                }

                if(topic)
                {
                    topic->removeSubscriber(conn);
                }

                if(subscriber)
                {
                    subscriber->removeTopic(msg->topicKey());
                }
            }

            bool topicPublish(const BaseConnection::ptr &conn, const TopicRequest::ptr &msg)
            {
                Topic::ptr topic;

                {
                    std::unique_lock<std::mutex> lock(_mutex);
                    auto topic_it = _topics.find(msg->topicKey());
                    if(topic_it == _topics.end())
                    {
                        return false;
                    }

                    topic = topic_it->second;
                }

                topic->pushMessage(msg);
                return true;
            }

        private:
            struct Topic
            {
                using ptr = std::shared_ptr<Topic>;
                std::mutex _mutex;
                std::string topic_name;
                std::unordered_set<BaseConnection::ptr> subscribers; // 当前主题的订阅者

                Topic(const std::string &name)
                    :topic_name(name)
                {
                    
                }

                // 新增订阅的时候进行调用
                void appendSubscriber(const BaseConnection::ptr &conn)
                {
                    std::unique_lock<std::mutex> lock(_mutex);
                    subscribers.insert(conn);
                }

                // 取消订阅或者订阅者连接断开的时候调用
                void removeSubscriber(const BaseConnection::ptr &conn)
                {
                    std::unique_lock<std::mutex> lock(_mutex);
                    subscribers.erase(conn);
                }

                // 收到消息发布请求的时候调用
                void pushMessage(const BaseMessage::ptr &msg)
                {
                    std::unique_lock<std::mutex> lock(_mutex);
                    for (auto &subscriber : subscribers)
                    {
                        subscriber->send(msg);
                    }
                }
            };

            struct Subscriber
            {
                using ptr =std::shared_ptr<Subscriber>;
                std::mutex _mutex;
                BaseConnection::ptr conn;
                std::unordered_set<std::string> topics;// 订阅者订阅的主题名称

                Subscriber(const BaseConnection::ptr &c)
                    :conn(c)
                {

                }

                // 订阅主题的时候调用
                void appendTopic(const std::string &topic_name)
                {
                    std::unique_lock<std::mutex> lock(_mutex);
                    topics.insert(topic_name);
                }

                // 主题被删除或者取消订阅的时候调用
                void removeTopic(const std::string &topic_name)
                {
                    std::unique_lock<std::mutex> lock(_mutex);
                    topics.erase(topic_name);
                }
            };

        private:
            std::mutex _mutex;
            std::unordered_map<std::string, Topic::ptr> _topics;
            std::unordered_map<BaseConnection::ptr, Subscriber::ptr> _subscribers;
        };
    }
}