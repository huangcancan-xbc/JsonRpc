CFLAG= -std=c++11 -I ../../../build/release-install-cpp11/include/
LFLAG= -L ../../../build/release-install-cpp11/lib -lmuduo_net -lmuduo_base -lpthread -ljsoncpp

all: registry_server rpc_server_direct rpc_server_registry topic_server integration_client test_runner

registry_server: registry_server.cc
	g++ -g $(CFLAG) $^ -o $@ $(LFLAG)

rpc_server_direct: rpc_server_direct.cc
	g++ -g $(CFLAG) $^ -o $@ $(LFLAG)

rpc_server_registry: rpc_server_registry.cc
	g++ -g $(CFLAG) $^ -o $@ $(LFLAG)

topic_server: topic_server.cc
	g++ -g $(CFLAG) $^ -o $@ $(LFLAG)

integration_client: integration_client.cc
	g++ -g $(CFLAG) $^ -o $@ $(LFLAG)

test_runner: test_runner.cc
	g++ -g $(CFLAG) $^ -o $@ $(LFLAG)

.PHONY: run clean

run: all
	./test_runner

clean:
	rm -f test_runner
