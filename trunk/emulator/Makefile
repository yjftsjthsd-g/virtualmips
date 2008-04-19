PROGRAM := pavo

SRCDIRS += ./system/jz/soc
SRCDIRS += ./system/jz/pavo
INCLUDE_DIR += -I./system/jz/soc
INCLUDE_DIR += -I./system/jz/pavo




#comment these if you do not want LCD emulation
CFLAGS += -DSIM_LCD  
LDFLAGS += -lSDL
SRCDIRS += ./utils/sdl
INCLUDE_DIR += -I./utils/sdl



CFLAGS +=   -DSIM_PAVO $(INCLUDE_DIR)



-include ./com.mak

all : $(PROGRAM)

u-boot:
	cp /home/root/newdevelop/jzsrc/u-boot-1.1.6/u-boot-nand.bin .
	../tool/mknandflash
linux:
	cp /home/root/newdevelop/jzsrc/linux-2.6.24/arch/mips/boot/uImage .
	../tool/mknandflash
