all: client.cpp client.h leafset.cpp leafset.h pastry.cpp pastry.h routing.cpp routing.h semaphore.cpp semaphore.h server.cpp server.h util.cpp util.h Makefile
	g++ -std=c++0x -o pastry *.cpp -l:libcrypto.so -fpermissive -lpthread -g

debug: client.cpp client.h leafset.cpp leafset.h pastry.cpp pastry.h routing.cpp routing.h semaphore.cpp semaphore.h server.cpp server.h util.cpp util.h Makefile
	g++ -Wall -Wextra -DDEBUG -std=c++0x -o pastry *.cpp -l:libcrypto.so -fpermissive -lpthread -g

clean:
	rm -f ./pastry

ctags:
	ctags -e --c-kinds=+defgstu -R -f tags
