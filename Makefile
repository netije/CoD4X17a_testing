#
# Makefile for cod4x 1.7a
# 

CC = gcc
ASM = nasm

CFLAGS = -s -m32 -Wall -O2 -g -fno-omit-frame-pointer -mtune=prescott
LDFLAGS = -s -m32 -Wl,-ldl,-lpthread,-lm,-Tlinkerscript.ld,--dynamic-list=pluginExports.ld  
AFLAGS = -f elf

CFILES = $(wildcard *.c)
AFILES = $(wildcard *.asm)

COBJS = $(CFILES:.c=.o) 
AOBJS = $(AFILES:.asm=.o)
 
EXEC=cod4x17n

all: $(EXEC)

clean:
	@rm -rf $(COBJS) $(AOBJS) $(EXEC)  

$(EXEC): $(COBJS) $(AOBJS) 
	$(CC) $(COBJS) $(AOBJS) -o $@ $(LDFLAGS)  
	
$(CFILES):
	$(CC) $(FLAGS) $(CFLAGS) -c $@

$(AOBJS): 
	$(ASM) $(AFLAGS) $(*F).asm 
 