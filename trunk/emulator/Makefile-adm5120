PROGRAM := adm5120

SRCDIRS := .
SRCDIRS += ./device
SRCDIRS += ./mips
SRCDIRS += ./system
SRCDIRS += ./system/adm5120
SRCDIRS += ./utils
SRCDIRS += ./memory
SRCDIRS += ./contrib
SRCDIRS += ./gdb

INCLUDE_DIR := -I.
INCLUDE_DIR += -I./device
INCLUDE_DIR += -I./mips
INCLUDE_DIR += -I./system
INCLUDE_DIR += -I./system/adm5120
INCLUDE_DIR += -I./utils
INCLUDE_DIR += -I./memory
INCLUDE_DIR += -I./contrib
INCLUDE_DIR += -I./gdb


SRCEXTS :=  .c


CFLAGS := -g -Wall -DSIM_ADM5120 -O3  -fomit-frame-pointer  $(INCLUDE_DIR)

#enable to show emulation performance
#CFLAGS+= -DDEBUG_MHZ

CONFUSE_LIB =./contrib/libconfuse.a
PTHREAD_LIBS =-lpthread
ELF_LIBS =-lelf
LIBS=-L/usr/lib -L.  $(PTHREAD_LIBS)  $(ELF_LIBS) $(CONFUSE_LIB)
LDFLAGS := -O3 
LDFLAGS +=  $(LIBS) 

CC = gcc

RM = rm -f

SHELL = /bin/sh
SOURCES = $(foreach d,$(SRCDIRS),$(wildcard $(addprefix $(d)/*,$(SRCEXTS))))
OBJS = $(foreach x,$(SRCEXTS), \
$(patsubst %$(x),%.o,$(filter %$(x),$(SOURCES))))
DEPS = $(foreach x,$(SRCEXTS), \
$(patsubst %$(x),%.d,$(filter %$(x),$(SOURCES))))



.PHONY : all objs clean cleanall rebuild  

all : $(PROGRAM)

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
	echo $(DEPS)
	$(RM) $(OBJS) $(DEPS)  $(PROGRAM)  ilt*  *.txt vmlinuz pad

cleanall: clean
	@$(RM) $(PROGRAM) $(PROGRAM).exe *.txt vmlinuz pad




