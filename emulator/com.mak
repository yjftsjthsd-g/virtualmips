SRCDIRS += .
SRCDIRS += ./device
SRCDIRS += ./system
SRCDIRS += ./mips
SRCDIRS += ./utils
SRCDIRS += ./utils/net
SRCDIRS += ./memory
SRCDIRS += ./gdb

INCLUDE_DIR += -I.
INCLUDE_DIR += -I./device
INCLUDE_DIR += -I./mips
INCLUDE_DIR += -I./system
INCLUDE_DIR += -I./utils
INCLUDE_DIR += -I./utils/net
INCLUDE_DIR += -I./memory
INCLUDE_DIR += -I./gdb

CC = gcc
#CC = gcc-3.4

RM = rm -f
SRCEXTS :=  .c

CFLAGS += -g -Wall -O3  -fomit-frame-pointer

#enable to show emulation performance
#CFLAGS+= -DDEBUG_MHZ

RT_LIB =-lrt
CONFUSE_LIB =-lconfuse
PTHREAD_LIBS =-lpthread
ELF_LIBS =-lelf
LIBS=-L/usr/lib -L.  $(PTHREAD_LIBS)  $(ELF_LIBS) $(CONFUSE_LIB) $(RT_LIB)
LDFLAGS += -O3 
LDFLAGS +=  $(LIBS) 

SHELL = /bin/sh
SOURCES = $(foreach d,$(SRCDIRS),$(wildcard $(addprefix $(d)/*,$(SRCEXTS))))
OBJS = $(foreach x,$(SRCEXTS), \
$(patsubst %$(x),%.o,$(filter %$(x),$(SOURCES))))
DEPS = $(foreach x,$(SRCEXTS), \
$(patsubst %$(x),%.d,$(filter %$(x),$(SOURCES))))

.PHONY : all clean 

# Rules for creating the dependency files (.d).
#---------------------------------------------------
%.d : %.c
	$(CC) -M $(CFLAGS) $<  > $@

%.o : %.c
	$(CC) -c $(CPPFLAGS) $(CFLAGS) $< -o $@

$(PROGRAM) : $(OBJS)
	$(CC) -o $(PROGRAM) $(OBJS) $(LDFLAGS)

-include $(DEPS)

rebuild: clean all

clean :
	$(RM) $(OBJS) $(DEPS)  $(PROGRAM)  ilt*  *_log.txt sim.txt
