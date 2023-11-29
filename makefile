CXX ?= g++

DEBUG ?= 1
ifeq ($(DEBUG), 1)
    CXXFLAGS += -g
else
    CXXFLAGS += -O2

endif

server: ./test_scripts/test_http.cpp ./localepoll/localepoll.cpp ./http/http.cpp ./log/log.cpp
	$(CXX) -o server  $^ $(CXXFLAGS) -lpthread -lmysqlclient
# ./exe/testlog: ./test_scripts/test_log.cpp  ./localepoll/localepoll.cpp ./log/log.cpp
# 	$(CXX) -o testlog  $^ $(CXXFLAGS) -lpthread

clean:
	rm  -r server
