BIN=/home/ubuntu16/emacsimdir/bin

UTIL=/home/cs580000/share/hoca/util

GCC=$(BIN)/m68k-elf-gcc
GCC_FLAGS=-m68000 -I$(UTIL)/inc/ -I$(UTIL)/inc/m68k-linux-gnu -g -gstabs -c

LD=$(BIN)/m68k-elf-ld
LD_FLAGS=-L$(UTIL)/lib -L$(BIN)/../lib/gcc/m68k-elf/4.8.2/m68000 -Ttext=0x1600

LIBS=-lutil -lc -lsim -lm -lgcc

CRT0=$(UTIL)/crtbegin.o
CRT1=$(UTIL)/crtend.o


DIRQUEUE=../../queues
DIRTRAP=../../nucleus/traps
DIRINT=../../nucleus/interrupts
DIRP1=../part1


all: hoca


clean:
	rm -f hoca page.o slsyscall2.o


slsyscall2.o: slsyscall2.c ../../h/const.h ../../h/types.h
	$(GCC) $(GCC_FLAGS) -o slsyscall2.o slsyscall2.c


page.o: page.c ../../h/const.h ../../h/types.h
	$(GCC) $(GCC_FLAGS) -o page.o page.c


hoca: $(DIRTRAP)/main.o $(DIRTRAP)/syscall.o $(DIRTRAP)/trap.o $(DIRINT)/int.o $(DIRQUEUE)/procq.o $(DIRQUEUE)/asl.o $(DIRP1)/support.o $(DIRP1)/slsyscall1.o page.o slsyscall2.o
	$(LD) $(LD_FLAGS) -o hoca $(CRT0) $(DIRTRAP)/main.o $(DIRTRAP)/syscall.o $(DIRTRAP)/trap.o $(DIRINT)/int.o  $(DIRQUEUE)/procq.o $(DIRQUEUE)/asl.o $(CRT1) $(DIRP1)/support.o $(DIRP1)/slsyscall1.o page.o slsyscall2.o $(LIBS)
