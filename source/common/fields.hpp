/*
    前置枚举类型的实现/定义
*/
#pragma once
#include <string>
#include <unordered_map>

namespace rpc
{
    // 正文字段名称宏的定义
    #define KEY_METHOD      "method"       // 方法名称（RPC调用的函数名）
    #define KEY_PARAMS      "parameters"   // 方法参数（RPC调用时的参数列表）
    #define KEY_TOPIC_KEY   "topic_key"    // 主题关键字/名称（用于发布/订阅）
    #define KEY_TOPIC_MSG   "topic_msg"    // 主题消息内容（发布的消息内容）
    #define KEY_OPTYPE      "optype"       // 操作类型（区分具体操作行为）
    #define KEY_HOST        "host"         // 主机地址/信息（可包含IP和端口）
    #define KEY_HOST_IP     "ip"           // 主机IP地址
    #define KEY_HOST_PORT   "port"         // 主机端口号
    #define KEY_RCODE       "rcode"        // 返回/响应码（表示RPC调用状态）
    #define KEY_RESULT      "result"       // 返回/调用结果（RPC响应内容）

    // 消息类型定义
    enum class MType
    {
        REQ_RPC = 0, // RPC请求消息（客户端请求调用远程方法）
        RSP_RPC,     // RPC响应消息（服务器返回调用结果）
        REQ_TOPIC,   // 主题请求（订阅/发布）
        RSP_TOPIC,   // 主题响应
        REQ_SERVICE, // 服务请求（注册/发现）
        RSP_SERVICE  // 服务响应（注册/发现）
    };

    // 响应码定义
    enum class RCode
    {
        RCODE_OK = 0,            // 成功处理
        RCODE_PAPSE_FAILED,      // 消息解析失败
        RCODE_ERROR_MSGTYPE,     // 消息类型错误
        RCODE_INVALID_MSG,       // 无效的消息
        RCODE_DISCONNECTED,      // 连接断开
        RCODE_INVALID_PARAMS,    // 无效的Rpc参数
        RCODE_NOT_FOUND_SERVICE, // 没有找到对应的服务（服务未注册/下线）
        RCODE_INVALID_OPTYPE,    // 无效的操作类型
        RCODE_NOT_FOUND_TOPIC,   // 没有找到对应的主题
        RCODE_INVALID_ERROR      // 内部错误
    };

    // 错误码定义
    static std::string errReason(RCode code)
    {
        static std::unordered_map<RCode, std::string> err_map = {
            {RCode::RCODE_OK, "成功处理！"},
            {RCode::RCODE_PAPSE_FAILED, "消息解析失败！"},
            {RCode::RCODE_ERROR_MSGTYPE, "消息类型错误"},
            {RCode::RCODE_INVALID_MSG, "无效的消息！"},
            {RCode::RCODE_DISCONNECTED, "连接断开！"},
            {RCode::RCODE_INVALID_PARAMS, "无效的Rpc参数！"},
            {RCode::RCODE_NOT_FOUND_SERVICE, "没有找到对应的服务！"},
            {RCode::RCODE_INVALID_OPTYPE, "无效的操作类型！"},
            {RCode::RCODE_NOT_FOUND_TOPIC, "没有找到对应的主题！"},
            {RCode::RCODE_INVALID_ERROR, "内部错误！"}};

        auto it = err_map.find(code);
        if (it == err_map.end())
        {
            return "未知错误！";
        }

        return it->second;
        // return err_map[code];
    }

    // RPC请求类型定义
    enum class RType
    {
        REQ_SYNC = 0, // 同步请求
        REQ_ASYNC,    // 异步请求
        RSP_CALLBACK  // 回调请求
    };

    // Topic（主题）操作类型
    enum class TopicOptype
    {
        TOPIC_CREATE = 0, // 创建主题
        TOPIC_REMOVE,     // 删除
        TOPIC_SUBSCRIBE,  // 订阅
        TOPIC_CANCEL,     // 取消订阅
        TOPIC_PUBLISH     // 发布主题消息
    };

    // Service（服务）操作类型
    enum class ServiceOptype
    {
        SERVICE_REGISTRY = 0, // 服务注册
        SERVICE_DISCOVERY,    // 服务发现
        SERVICE_ONLINE,       // 服务上线
        SERVICE_OFFLINE,      // 服务下线
    };
}