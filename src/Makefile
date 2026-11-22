CC      = gcc
PROF    = -O -g
NOCRYPT =
# gnu89: the dialect this code was written in, so a newer compiler's default (C23 in gcc 15) doesn't
# reinterpret it.
C_FLAGS =  -Wall -std=gnu89 $(PROF) $(NOCRYPT)
L_FLAGS =  $(PROF)
# On Linux, crypt() is in libcrypt, as Makefile.linux has it.
LIBS    = -lcrypt

O_FILES = act_comm.o act_enter.o act_info.o act_move.o act_obj.o act_wiz.o \
          alias.o ban.o comm.o const.o db.o db2.o effects.o fight.o flags.o \
	  handler.o healer.o interp.o note.o lookup.o magic.o magic2.o \
	  music.o recycle.o save.o scan.o skills.o special.o tables.o \
	  update.o

rom: $(O_FILES)
	rm -f rom
	$(CC) $(L_FLAGS) -o rom $(O_FILES) $(LIBS)

.c.o: merc.h
	$(CC) -c $(C_FLAGS) $<
