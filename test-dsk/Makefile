.PHONY: default
default: test-dsk.dsk

include ../Makefile


test-dsk.dsk: ../upd765pro$(DOTEXE) main.bin image.bin
	../upd765pro $@ main.bin,ro image.bin,ro,sys


main.bin: main.asm cpc.inc
	sjasmplus $<


LDLIBS:=$(LDLIBS) -lpng
make-image$(DOTEXE): make-image.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS) $(LDLIBS)

image.bin: jungle.png make-image$(DOTEXE)
	./make-image $< $@ 0x50
