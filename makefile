all:
	g++ -std=c++14 -O3 -g -Wall -fmessage-length=0 -o nogo nogo.cpp
clean:
	rm nogo