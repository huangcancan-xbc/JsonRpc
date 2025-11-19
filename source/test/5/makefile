CFLAG= -std=c++11 -I ../../../build/release-install-cpp11/include/
LFLAG= -L ../../../build/release-install-cpp11/lib -lmuduo_net -lmuduo_base -lpthread -ljsoncpp
all: reg_server server client
reg_server: registry_server.cc
	g++ -g $(CFLAG) $^ -o $@  $(LFLAG)
server: rpc_server.cc
	g++ -g -O0 $(CFLAG) $^ -o $@  $(LFLAG)
client: rpc_client.cc
	g++ -g $(CFLAG) $^ -o $@  $(LFLAG)