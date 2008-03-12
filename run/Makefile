.PHONY: all	pavo adm5120 tool clean clean-tool

default: all

MAKE = make
RM = rm -f

all: pavo adm5120 tool
	
clean: clean-tool
	$(RM) adm5120 pavo ilt* ilt*  *_log.txt sim.txt
	
tool:
	@echo "Building tool"
	(cd ../tool && $(MAKE))  

pavo:
	@echo "Building pavo"
	(cd ../emulator && $(MAKE) -f Makefile-pavo clean)  
	(cd ../emulator && $(MAKE) -f Makefile-pavo) 
	(cd ../emulator && mv pavo ../run) 
	
adm5120:
	@echo "Building adm5120"
	(cd ../emulator && $(MAKE) -f Makefile-adm5120 clean)  
	(cd ../emulator && $(MAKE) -f Makefile-adm5120) 
	(cd ../emulator && mv adm5120 ../run)   

clean-tool:
	(cd ../tool && $(MAKE) clean) 