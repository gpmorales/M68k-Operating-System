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
DIRP2=../part2


all: hoca


clean:
	rm -f hoca support.o page.o slsyscall1.o slsyscall2.o


support.o: support.c ../../h/const.h ../../h/types.h
	$(GCC) $(GCC_FLAGS) -o support.o support.c


page.o: $(DIRP2)/page.c ../../h/const.h ../../h/types.h
	$(GCC) $(GCC_FLAGS) -o page.o $(DIRP2)/page.c


slsyscall1.o: slsyscall1.c ../../h/const.h ../../h/types.h
	$(GCC) $(GCC_FLAGS) -o slsyscall1.o slsyscall1.c


slsyscall2.o: $(DIRP2)/slsyscall2.c
	$(GCC) $(GCC_FLAGS) -o slsyscall2.o $(DIRP2)/slsyscall2.c



hoca: $(DIRTRAP)/main.o $(DIRTRAP)/syscall.o $(DIRTRAP)/trap.o $(DIRINT)/int.o $(DIRQUEUE)/procq.o $(DIRQUEUE)/asl.o support.o slsyscall1.o page.o slsyscall2.o
	$(LD) $(LD_FLAGS) -o hoca $(CRT0) $(DIRTRAP)/main.o $(DIRTRAP)/syscall.o $(DIRTRAP)/trap.o $(DIRINT)/int.o  $(DIRQUEUE)/procq.o $(DIRQUEUE)/asl.o $(CRT1) support.o slsyscall1.o page.o slsyscall2.o $(LIBS)
