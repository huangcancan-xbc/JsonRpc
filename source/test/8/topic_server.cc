#include "../../server/rpc_server.hpp"
#include "test_config.hpp"

int main()
{
    rpc::server::TopicServer server(test8::PORT_TOPIC);
    server.start();
    return 0;
}
