# JsonRpc（C++11 + Muduo）  

一个轻量、通用、可扩展的 RPC 框架，支持服务注册发现、发布订阅、同步/异步/回调调用。

## 1. 项目介绍

`JsonRpc` 是一个基于 **C++11 + Muduo + JsonCpp** 的 RPC 框架，目标是：

- 快速搭建 RPC 服务
- 远程函数调用
- 支持服务注册/发现（注册中心）
- 支持 Topic 发布订阅
- 支持同步/异步/回调三种调用方式

## 2. 这个项目能做什么（功能）

### 1. RPC 调用

- 客户端直接调用远端函数（例如 `Add`），就像本地函数调用一样去调远端方法。
- 支持：
    - 同步：`client.call(..., Json::Value& result)`，直接拿结果。
    - 异步：`client.call(..., std::future<Json::Value>& fut)`，返回 `future`。
    - 回调：`client.call(..., callback)`，结果到达自动回调。

### 2. 服务注册与发现

- 服务端启动后，可以把自己注册到注册中心。
- 客户端调用时先查注册中心，再决定连哪个服务端。
- 一个方法可对应多个提供者，客户端会轮询选择。

### 3. Topic 发布订阅

- 支持创建主题、删除主题。
- 支持订阅、取消订阅。
- 支持发布消息并广播给订阅者。
- 适合通知、广播、状态推送类场景。

---

## 3. 如何使用

### 1. 环境要求

- Linux
- `g++`（C++11）
- `pthread`
- `jsoncpp`
- Muduo（已在 `build/release-install-cpp11/` 放了头文件和静态库）

### 2. 跑 RPC + 注册中心测试样例（test/5）

```bash
cd source/test/5
make
./reg_server
```

新开终端：

```bash
cd source/test/5
./server
```

再开一个终端：

```bash
cd source/test/5
./client
```

会看到同步/异步/回调调用都返回正确结果。

### 3. 跑 Topic 发布订阅测试样例（test/6）

终端 1：

```bash
cd source/test/6
make
./server
```

终端 2：

```bash
cd source/test/6
./subscribe_client
```

终端 3：

```bash
cd source/test/6
./publish_client
```

订阅端会打印类似 `Hello World-0...9` 的推送消息。

### 4. 功能综合测试（test/8）

```bash
cd source/test/8
make -B -j4
make run
```

它会覆盖直连 RPC、注册发现、Topic 全流程、超时恢复等主流程。

---

## 4. 对外接口说明（功能、参数、返回值、使用示例）

### 1. 服务端接口

#### 1. `rpc::server::RegistryServer`

文件：`source/server/rpc_server.hpp`

用途：启动注册中心服务（服务注册 + 服务发现 + 上下线通知）。

核心接口：

```cpp
rpc::server::RegistryServer reg_server(int port);	// port：注册中心监听端口，比如 9090
reg_server.start();

// 示例：默认端口
#include "source/server/rpc_server.hpp"
int main()
{
    rpc::server::RegistryServer reg_server(9090); // 监听 9090
    reg_server.start();                           // 启动注册中心事件循环
    return 0;
}
```

测试隔离端口：

```cpp
#include "source/server/rpc_server.hpp"
int main()
{
    const int reg_port = 12345;                  // 使用高位端口避免冲突
    rpc::server::RegistryServer reg_server(reg_port);
    reg_server.start();
    return 0;
}
```

#### 2. `rpc::server::RpcServer`

文件：`source/server/rpc_server.hpp`

用途：启动 RPC 服务节点，可选是否接入注册中心。

构造函数：

```cpp
RpcServer(const Address &access_addr,		// 服务节点自己的地址
          bool enableRegistry = false,		// 是否启用注册中心模式
          const Address &registry_server_addr = Address());		// 注册中心地址
```

参数说明：

- `access_addr`：本 RPC 服务对外地址，类型 `Address = std::pair<std::string, int>`。
- `enableRegistry`：`true` 表示把方法注册到注册中心。
- `registry_server_addr`：注册中心地址，仅 `enableRegistry=true` 时使用。

核心接口：

```cpp
void registerMethod(const ServiceDescribe::ptr &service);
void start();
```

**示例 A：直连 & 不用注册中心**

```cpp
#include <memory>
#include <jsoncpp/json/json.h>
#include "source/server/rpc_server.hpp"
// 业务函数：两个整数相加
void Add(const Json::Value &req, Json::Value &rsp)
{
    rsp = req["num1"].asInt() + req["num2"].asInt();
}

int main()
{
    std::unique_ptr<rpc::server::ServiceDescribeFactory> f(new rpc::server::ServiceDescribeFactory());
    f->setMethodName("Add");                                 	// 方法名
    f->setParamsDesc("num1", rpc::server::VType::INTEGRAL); 	// 参数 1 类型
    f->setParamsDesc("num2", rpc::server::VType::INTEGRAL); 	// 参数 2 类型
    f->setReturnType(rpc::server::VType::INTEGRAL);         	// 返回值类型
    f->setCallback(Add);                                     	// 绑定业务函数

    rpc::server::RpcServer server(rpc::Address("127.0.0.1", 8080), false); 	// 不接注册中心
    server.registerMethod(f->build());                                     	// 注册方法
    server.start();                                                         // 启动服务
    return 0;
}
```

**示例 B：发现 & 自动向注册中心注册**

```cpp
#include <memory>
#include <jsoncpp/json/json.h>
#include "source/server/rpc_server.hpp"
void Add(const Json::Value &req, Json::Value &rsp)
{
    rsp = req["num1"].asInt() + req["num2"].asInt();
}

int main()
{
    std::unique_ptr<rpc::server::ServiceDescribeFactory> f(new rpc::server::ServiceDescribeFactory());
    f->setMethodName("Add");                                 	// 方法名
    f->setParamsDesc("num1", rpc::server::VType::INTEGRAL); 	// 参数 1
    f->setParamsDesc("num2", rpc::server::VType::INTEGRAL); 	// 参数 2
    f->setReturnType(rpc::server::VType::INTEGRAL);         	// 返回值
    f->setCallback(Add);                                     	// 绑定业务函数

    rpc::Address self("127.0.0.1", 18081);       		// 当前服务地址
    rpc::Address reg("127.0.0.1", 19090);        		// 注册中心地址
    rpc::server::RpcServer server(self, true, reg); 	// 开启注册中心模式
    server.registerMethod(f->build());               	// 注册方法到本地路由
    server.start();                                  	// 启动服务并对外提供
    return 0;
}
```

#### 3. `rpc::server::ServiceDescribeFactory`

文件：`source/server/rpc_router.hpp`

用途：定义 RPC 方法描述，即“一个可远程调用的方法/函数”的信息（方法/函数名、参数、返回值、回调）。

核心接口：

```cpp
void setMethodName(const std::string &name);
void setReturnType(VType vtype);
void setParamsDesc(const std::string &pname, VType vtype);
void setCallback(const ServiceDescribe::ServiceCallback &cb);
ServiceDescribe::ptr build();
```

`VType` 可选值：

- `BOOL`：布尔值（`true/false`）
- `INTEGRAL`：整数类型（对应 `Json::Value::isIntegral()`）
- `NUMERIC`：数值类型（整数或浮点，`isNumeric()`）
- `STRING`：字符串类型（`isString()`）
- `ARRAY`：数组类型（`isArray()`）
- `OBJECT`：对象类型（`isObject()`）

**示例：两个整数相加**

```cpp
#include <memory>
#include <jsoncpp/json/json.h>
#include "source/server/rpc_router.hpp"

std::unique_ptr<rpc::server::ServiceDescribeFactory> f(new rpc::server::ServiceDescribeFactory());
f->setMethodName("Add");                                 	// 方法名
f->setParamsDesc("num1", rpc::server::VType::INTEGRAL); 	// 参数 1
f->setParamsDesc("num2", rpc::server::VType::INTEGRAL); 	// 参数 2
f->setReturnType(rpc::server::VType::INTEGRAL);         	// 返回值
f->setCallback(Add);                                     	// 业务函数
```

**示例：字符串参数 + 对象返回**

```cpp
#include <memory>
#include <jsoncpp/json/json.h>
#include "source/server/rpc_router.hpp"

std::unique_ptr<rpc::server::ServiceDescribeFactory> f(new rpc::server::ServiceDescribeFactory());
f->setMethodName("QueryUser");                       		// 查询用户
f->setParamsDesc("uid", rpc::server::VType::STRING); 		// 用户 ID
f->setReturnType(rpc::server::VType::OBJECT);       		// 返回对象
f->setCallback(QueryUser);                           		// 绑定查询函数
```

#### 4. `rpc::server::TopicServer`

文件：`source/server/rpc_server.hpp`

用途：启动 Topic 服务端，管理主题与订阅者。

核心接口：

```cpp
rpc::server::TopicServer server(int port);
server.start();

// 示例：默认端口
#include "source/server/rpc_server.hpp"
int main()
{
    rpc::server::TopicServer server(8888); // 监听 8888
    server.start();                        // 启动 Topic 服务
    return 0;
}
```

示例：其他端口

```cpp
#include "source/server/rpc_server.hpp"
int main()
{
    rpc::server::TopicServer server(18888); // 使用高位端口做压测
    server.start();                          // 启动 Topic 服务
    return 0;
}
```

### 2. 客户端接口

#### 1. `rpc::client::RpcClient`

文件：`source/client/rpc_client.hpp`

用途：RPC 调用客户端（直连 或 发现）。

构造函数：

```cpp
RpcClient(bool enableDiscovery, const std::string &ip, int port);
```

参数说明：

- `enableDiscovery=false`：`ip:port` 表示 RPC 服务端地址（直连）
- `enableDiscovery=true`：`ip:port` 表示注册中心地址（先发现再调用）

调用接口（3 个重载）：

```cpp
bool call(const std::string &method, const Json::Value &params, Json::Value &result); // 同步
bool call(const std::string &method, const Json::Value &params, RpcCaller::JsonAsyncResponse &result); // 异步 future
bool call(const std::string &method, const Json::Value &params, const RpcCaller::JsonResponseCallback &cb); // 回调
```

返回值：

- `true`：（请求链路和业务执行）**调用成功**。
- `false`：**失败**（连接不可用/服务不存在/参数错误/超时等）。

**示例：直连 + 同步调用**

```cpp
#include <jsoncpp/json/json.h>
#include "source/common/detail.hpp"
#include "source/client/rpc_client.hpp"

int main()
{
    rpc::client::RpcClient client(false, "127.0.0.1", 8080);  // 直连模式
    Json::Value params, result;                               // 参数和返回值
    params["num1"] = 7;                                       // 第一个参数
    params["num2"] = 8;                                       // 第二个参数
    if (client.call("Add", params, result)) // 同步调用
    {
        ILOG("result=%d", result.asInt());                    // 打印调用结果
    }
    return 0;
}
```

**示例：发现 + 异步 future**

```cpp
#include <jsoncpp/json/json.h>
#include "source/common/detail.hpp"
#include "source/client/rpc_client.hpp"

int main()
{
    rpc::client::RpcClient client(true, "127.0.0.1", 9090); // 发现模式
    Json::Value params;
    params["num1"] = 70;                                     // 第一个参数
    params["num2"] = 80;                                     // 第二个参数

    rpc::client::RpcCaller::JsonAsyncResponse fut;           // future
    if (client.call("Add", params, fut)) // 异步调用
    {
        Json::Value result = fut.get();                      // 等待结果
        ILOG("result=%d", result.asInt());                   // 打印调用结果
    }
    return 0;
}
```

#### 2. `rpc::client::TopicClient`

文件：`source/client/rpc_client.hpp`

用途：Topic 客户端，处理发布和订阅。

构造函数：

```cpp
TopicClient(const std::string &ip, int port);
```

核心接口：

```cpp
bool create(const std::string &key);
bool remove(const std::string &key);
bool subscribe(const std::string &key, const TopicManager::SubCallback &cb);
bool cancel(const std::string &key);
bool publish(const std::string &key, const std::string &msg);
void shutdown();
```

说明：

- `subscribe` 的回调签名：`void(const std::string &topic, const std::string &msg)`
- 不存在主题时，`subscribe/remove/cancel` 会返回 `false`

示例：发布端

```cpp
#include "source/common/detail.hpp"
#include "source/client/rpc_client.hpp"

int main()
{
    rpc::client::TopicClient publisher("127.0.0.1", 8888); 	 // 连接 Topic 服务
    publisher.create("alarm");                     			// 创建主题
    publisher.publish("alarm", "disk 80%");       			// 发布消息
    publisher.shutdown();                          			// 关闭连接
    return 0;
}
```

示例：订阅端

```cpp
#include <chrono>
#include <thread>
#include "source/common/detail.hpp"
#include "source/client/rpc_client.hpp"
int main()
{
    rpc::client::TopicClient subscriber("127.0.0.1", 8888); 	// 连接 Topic 服务
    subscriber.subscribe("alarm", [](const std::string &topic, const std::string &msg) {
        ILOG("[%s] %s", topic.c_str(), msg.c_str()); 			// 收到推送后的处理
    });
    std::this_thread::sleep_for(std::chrono::seconds(5)); 		// 等待消息
    subscriber.cancel("alarm");                            		// 取消订阅
    subscriber.shutdown();
    return 0;
}
```

### 3. 常见示例

#### 1. 定义并启动一个 RPC 服务（带注册中心）

```cpp
#include <memory>
#include <jsoncpp/json/json.h>
#include "source/server/rpc_server.hpp"

void Add(const Json::Value &req, Json::Value &rsp)
{
    rsp = req["num1"].asInt() + req["num2"].asInt();
}

int main()
{
    std::unique_ptr<rpc::server::ServiceDescribeFactory> f(new rpc::server::ServiceDescribeFactory()); // 服务描述构造器
    f->setMethodName("Add");                                  	// 方法名
    f->setParamsDesc("num1", rpc::server::VType::INTEGRAL);  	// 参数 1
    f->setParamsDesc("num2", rpc::server::VType::INTEGRAL);  	// 参数 2
    f->setReturnType(rpc::server::VType::INTEGRAL);          	// 返回值
    f->setCallback(Add);                                      	// 绑定业务函数

    rpc::server::RpcServer server(rpc::Address("127.0.0.1", 8080), true, rpc::Address("127.0.0.1", 9090)); // 开启注册中心模式
    server.registerMethod(f->build());      // 注册方法
    server.start();                         // 启动服务
}
```

#### 2. 发现模式调用 RPC（同步 + 异步 + 回调）

```cpp
#include <chrono>
#include "source/client/rpc_client.hpp"
#include "source/common/detail.hpp"
#include <jsoncpp/json/json.h>
#include <thread>

void cb(const Json::Value &result)
{
    ILOG("callback result: %d", result.asInt());
}

int main()
{
    rpc::client::RpcClient client(true, "127.0.0.1", 9090); // true 表示走服务发现

    Json::Value p, r;                                       // 请求参数和响应
    p["num1"] = 11; p["num2"] = 22;                         // 同步调用参数
    if (client.call("Add", p, r))
    {
        ILOG("sync result: %d", r.asInt());                 // 同步结果
    }

    rpc::client::RpcCaller::JsonAsyncResponse fut;          // future 句柄
    p["num1"] = 33; p["num2"] = 44;                         // 异步调用参数
    if (client.call("Add", p, fut))
    {
        r = fut.get();                                      // 等待异步结果
        ILOG("async result: %d", r.asInt());                // 异步结果
    }

    p["num1"] = 55; p["num2"] = 66;                         // 回调调用参数
    client.call("Add", p, cb);                              // 结果到达后触发 cb
    std::this_thread::sleep_for(std::chrono::seconds(1));   // 等待回调执行
}
```

#### 3. Topic 订阅与发布

订阅端：

```cpp
#include <chrono>
#include <thread>
#include "source/client/rpc_client.hpp"
#include "source/common/detail.hpp"

void onTopic(const std::string &key, const std::string &msg)
{
    ILOG("%s: %s", key.c_str(), msg.c_str());
}

int main()
{
    rpc::client::TopicClient c("127.0.0.1", 8888);         // 连接 Topic 服务
    c.create("hello");                                     // 创建主题
    c.subscribe("hello", onTopic);                         // 订阅主题
    std::this_thread::sleep_for(std::chrono::seconds(10)); // 等待接收消息
    c.shutdown();                                          // 关闭连接
}
```

发布端：

```cpp
#include "source/client/rpc_client.hpp"

int main()
{
    rpc::client::TopicClient c("127.0.0.1", 8888);        // 连接 Topic 服务
    c.create("hello");                                    // 创建主题
    for (int i = 0; i < 10; i++)
    {
        c.publish("hello", "Hello World-" + std::to_string(i)); // 连续发布消息
    }
    c.shutdown();                                         // 关闭连接
}
```

---

## 5. 项目架构与工作原理

### 1. 目录结构

```txt
source/common/   抽象层、协议、消息、网络封装、分发器
source/server/   RPC 路由、注册中心、Topic 服务端
source/client/   RPC 调用、服务发现、Topic 客户端
source/test/     测试用例（非核心源代码）
```

- 抽象接口：`source/common/abstract.hpp`
- 网络与协议：`source/common/net.hpp`
- 消息定义：`source/common/message.hpp`
- 分发器：`source/common/dispatcher.hpp`
- RPC 路由：`source/server/rpc_router.hpp`
- 注册中心：`source/server/rpc_registry.hpp`
- 主题管理：`source/server/rpc_topic.hpp`
- 客户端总入口：`source/client/rpc_client.hpp`

### 2. 分层设计（从下到上）

1. 传输层：`MuduoServer/MuduoClient`（基于 Muduo），负责收发字节流，这一层只负责“把数据送到协议层”。
2. 协议层：`BaseProtocol` + `LVProtocol`（打包/解包）。负责打包和解包，使用格式：`total_len + mtype + idlen + id + body`（总长度 + 消息类型 + 标识符长度 + 标识符 + 正文），会做边界检查，避免越界和脏数据穿透。
3. 消息层：`BaseMessage` + 各类 Request/Response（当前 JSON），把正文 Body 反序列化为具体消息对象，每个消息都有 `check()` 做校验。
4. 分发层：`Dispatcher`，根据消息类型 MType 找到对应处理器，只做路由，不写业务逻辑。
5. 业务层：
    - RPC：`RpcRouter` 负责方法查找、参数校验、执行回调。
    - 注册中心：`PDManager` 负责注册、发现、上下线通知。
    - Topic：`TopicManager` 负责主题与订阅者关系维护。

### 3. 一次 RPC 调用的完整过程

#### 1. “直连模式”

1. 客户端 `RpcClient(false, ip, port)` 创建直连连接。
2. `call()` 组装 `RpcRequest`，生成 `rid`。
3. 请求进入协议层序列化后发给服务端。
4. 服务端协议层解包，消息层反序列化并校验。
5. `Dispatcher` 把请求交给 `RpcRouter`。
6. `RpcRouter` 找方法、校参数、执行回调。
7. 返回 `RpcResponse`，带同一个 `rid`。
8. 客户端 `Requestor` 用 `rid` 匹配原请求并完成返回。

#### 2. "发现模式"

1. 客户端 `RpcClient(true, registry_ip, registry_port)` 先连注册中心。
2. 调用前先发 `SERVICE_DISCOVERY` 查目标方法提供者。
3. 注册中心返回可用主机列表。
4. 客户端本地缓存列表，并轮询选一个主机。
5. 后续 RPC 调用流程与直连模式一致。

### 4. Topic 处理

1. 客户端调用 `create/subscribe/publish/cancel/remove`。
2. 请求被编码为 `TopicRequest` 发送给 `TopicServer`。
3. `TopicManager` 根据 `optype` 处理主题关系。
4. `publish` 时遍历该主题订阅者并逐个推送。
5. 订阅端收到 `TOPIC_PUBLISH` 后执行本地回调。

### 5. 注册中心如何处理

注册中心里核心有三张关系：

1. 提供者连接 -> 提供者对象。
2. 方法名 -> 提供者集合。
3. 方法名 -> 发现者集合。

这样设计的好处是：

- 服务注册时能快速写入。
- 服务发现时能快速返回主机列表。
- 提供者下线时能快速通知已订阅发现者。

---

## 6. 高可用设计说明

这个项目的“高可用”重点是：**失败可控、可快速恢复、可做多提供者切换**。

>可以把系统想成“可替换的积木”：
>
>1. 传输层只关心字节，不关心业务。  
>2. 协议层只关心包格式，不关心方法名。  
>3. 消息层只关心序列化，不关心路由策略。  
>4. 分发层只关心 `MType -> handler`，不关心具体业务。  
>5. 业务层只关心功能实现，不关心底层收发细节。
>
>这套拆分带来的直接收益：
>
>- 想要换协议格式时，主要改协议层。  
>- 想要换消息体编码时，主要改消息层。  
>- 想要新增业务能力时，主要改业务层。  
>- 出问题时定位更快，因为每层职责很单一。

### 1. 服务侧多提供者 + 轮询选择

- 注册中心有同种方法的多个提供者
- 客户端发现后本地缓存主机列表
- 调用时轮询选择，具备基础负载分担能力（负载均衡）

### 2. 提供者上下线通知

- 提供者注册时，注册中心会通知已关注该方法的发现者“上线”
- 提供者断开时，注册中心会通知“下线”
- 客户端可据此及时更新本地可用节点

### 3. 连接失败快速返回

- 客户端连接等待有超时（避免无限卡死）
- 服务不可达时，不会长期阻塞调用线程

### 4. 协议和语义双重防护

- 帧长度、字段长度、边界、最大包大小都有校验
- 反序列化后再做 `check()` 语义校验
- 莫名的请求（非法或自造的请求）不会轻易把服务打崩

---

## 7. 如何“一键替换”序列化/协议（Protobuf 迁移思路）

>这里先分清两件事：
>
>1. **线协议（Framing）**：比如现在的 `LVProtocol`（长度 + 类型 + id + body）
>2. **消息序列化（Body 编码）**：比如现在的 `JsonMessage`（JsonCpp）

### 1. 只换“线协议”（改动小且快，相当于“一键替换”）

替换点：`source/common/net.hpp` 的 `ProtocolFactory`，只要实现一个新的 `BaseProtocol`，再让工厂返回它即可，步骤：

1. 新建 `MyProtocol : public BaseProtocol`。
2. 实现 `serialize/onMessage/canProcessed`。
3. 在 `ProtocolFactory::create()` 返回 `MyProtocol`。

这样做，业务层通常不用改。

### 2. 替换消息体为 Protobuf（完整替换）

替换点：`source/common/message.hpp`，建议新增 `ProtoMessage` 系列，替代 `JsonMessage` 系列，步骤：

1. 定义 `.proto`（RPC/Service/Topic 请求响应）。
2. 新增 `ProtoRequest/ProtoResponse` 并实现 `check()`。
3. 修改 `MessageFactory` 的 `MType -> 消息类` 映射。
4. 保留现有线协议，或同时替换线协议。

建议分两步上线：

1. 先换线协议。  
2. 再换消息体格式。  

这样风险更小，回滚更容易。

---

## 8. 项目优势与适用场景

### 1. 优势

- 接口简单，学习和接入成本低
- 同时覆盖 RPC、服务发现、发布订阅三类核心通信能力
- 分层清晰，扩展点明确（协议、消息、网络）
- 有专门的测试样例代码保证业务逻辑正常

### 2. 适用场景

- 微服务内部通信
- 学习 RPC 框架设计与工程化落地
