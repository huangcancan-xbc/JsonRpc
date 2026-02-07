/*
    测试端口配置：
    使用高位端口，避免和项目默认示例(8080/8888/9090)冲突
*/
#pragma once

namespace test8
{
    static const int PORT_DIRECT_RPC = 18080;
    static const int PORT_REGISTRY = 19090;
    static const int PORT_REGISTRY_RPC = 18081;
    static const int PORT_TOPIC = 18888;
    static const int PORT_UNUSED_REGISTRY = 19091;
}
