
.PHONY: all

all:mknandflash

mknandflash.o:mknandflash.c
	gcc -c -g mknandflash.c -I../emulator/contrib
mknandflash:mknandflash.o
	gcc -o mknandflash mknandflash.o -L. ../emulator/contrib/libconfuse.a
	
clean:
	rm -f mknandflash mknandflash.o