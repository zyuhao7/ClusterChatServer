CXX ?= g++
CXXFLAGS ?= -std=c++11 -g
MUDUO_PREFIX ?= /usr/local
MUDUO_INC ?= $(MUDUO_PREFIX)/include
MUDUO_LIB ?= $(MUDUO_PREFIX)/lib
BOOST_INC ?= /usr/include

all: muduo_server muduo_client testJson

# muduo server example
muduo_server: muduo_Server.cc
	$(CXX) $(CXXFLAGS) -I$(MUDUO_INC) -I$(BOOST_INC) -L$(MUDUO_LIB) -o $@ $^ -lmuduo_net -lmuduo_base -lpthread

# muduo client example
muduo_client: muduo_client.cc
	$(CXX) $(CXXFLAGS) -I$(MUDUO_INC) -I$(BOOST_INC) -L$(MUDUO_LIB) -o $@ $^ -lmuduo_net -lmuduo_base -lpthread

# json example
testJson: testJson.cc
	$(CXX) $(CXXFLAGS) -I../thirdparty -o $@ $^

.PHONY: clean
clean:
	rm -f muduo_server muduo_client testJson
