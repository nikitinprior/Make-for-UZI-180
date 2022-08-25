# Makefile for make (!)

.SUFFIXES: .c .obj

CFLAGS = -v

SRCS = main.c \
	check.c \
	input.c \
	macro.c \
	make.c \
	reader.c \
	rules.c \
	archive.c

OBJS = $(SRCS:.c=.obj)

LIBS = uzilibc.lib

.c.obj:
	zxc -o -c $(CFLAGS) $<

all: make

make: $(OBJS) h.h
	@echo "-Z -W3 -Ptext=0,data,bss -C100h -o"$@" \\" > linkmake
	@echo "crt.obj "$(OBJS)" \\" >> linkmake
	@echo "uzilibc.lib" >> linkmake
	zxlink <linkmake

clean:
	rm -f make linkmake $(OBJS) *.bak core *~ *.\$$\$$\$$

rmbak:
	rm -f *.bak core *~ *.\$$\$$\$$
