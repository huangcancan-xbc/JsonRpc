CFLAG= -std=c++11 -I ../../../build/release-install-cpp11/include/
LFLAG= -L ../../../build/release-install-cpp11/lib -lmuduo_net -lmuduo_base -lpthread -ljsoncpp

TEST5_DIR=../5

all: bug_regression

bug_regression: bug_regression.cc
	g++ -g $(CFLAG) $^ -o $@ $(LFLAG)

.PHONY: prepare run case clean

prepare:
	$(MAKE) -C $(TEST5_DIR)

run: prepare all
	./bug_regression

case: prepare all
	@if [ -z "$(NAME)" ]; then \
		echo "Usage: make case NAME=<malformed|connect|sync|semantic>"; \
		exit 1; \
	fi
	./bug_regression --case $(NAME)

clean:
	rm -f bug_regression
