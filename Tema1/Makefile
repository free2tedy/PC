all: link send recv

link:
	make -C link_emulator

send: send.o link_emulator/lib.o
	gcc -g send.o link_emulator/lib.o -o send -pthread

recv: recv.o link_emulator/lib.o
	gcc -g recv.o link_emulator/lib.o -o recv -pthread

.c.o: 
	gcc -Wall -g -c $? -pthread

clean:
	rm -f *.o send recv link_emulator/*.o link_emulator/link 
