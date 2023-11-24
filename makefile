testlog : test_log.cpp log.cpp block_queue.h locker.h log.h
	g++ -o testlog test_log.cpp log.cpp

clean:
	rm -r testlog
