
.PHONY: all

all:mknandflash

mknandflash.o:mknandflash.c
	gcc -c -g mknandflash.c
mknandflash:mknandflash.o
	gcc -o mknandflash mknandflash.o -L. libconfuse.a
	
clean:
	rm -f mknandflash mknandflash.o