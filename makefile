./exe/testmain : ./main.cpp
	g++ -o ./exe/testmain ./main.cpp

clean:
	rm -r ./exe/testmain
