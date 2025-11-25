CFLAG= -std=c++11 -I ../../../build/release-install-cpp11/include/
LFLAG= -L ../../../build/release-install-cpp11/lib -lmuduo_net -lmuduo_base -lpthread -ljsoncpp
all: publish_client server subscribe_client
publish_client: publish_client.cc
	g++ -g $(CFLAG) $^ -o $@  $(LFLAG)
server: server.cc
	g++ -g $(CFLAG) $^ -o $@  $(LFLAG)
subscribe_client: subscribe_client.cc
	g++ -g $(CFLAG) $^ -o $@  $(LFLAG)