CXX ?= g++

DEBUG ?= 1
ifeq ($(DEBUG), 1)
    CXXFLAGS += -g
else
    CXXFLAGS += -O2

endif

server: main.cpp ./timer/timer.cpp ./localepoll/localepoll.cpp ./http/http.cpp ./log/log.cpp ./CGImysql/cgi_mysql.cpp ./webserver/webserver.cpp
	$(CXX) -o server  $^ $(CXXFLAGS) -lpthread -lmysqlclient

clean:
	rm  -r server
