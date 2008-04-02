PROGRAM := pavo

SRCDIRS += ./system/jz/soc
SRCDIRS += ./system/jz/pavo
INCLUDE_DIR += -I./system/jz/soc
INCLUDE_DIR += -I./system/jz/pavo

CFLAGS += -DLINUX_ETH  -DSIM_PAVO $(INCLUDE_DIR)


-include ./com.mak

all : $(PROGRAM)

