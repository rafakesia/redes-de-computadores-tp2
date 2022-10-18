all:
	gcc -Wall -g3 -pthread server.c -o server
	gcc -Wall -g3 -pthread equipment.c -o equipment
	
clean:
	rm equipment server