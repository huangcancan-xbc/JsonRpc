#include "../../server/rpc_server.hpp"
#include "test_config.hpp"

int main()
{
    rpc::server::RegistryServer reg_server(test8::PORT_REGISTRY);
    reg_server.start();
    return 0;
}
