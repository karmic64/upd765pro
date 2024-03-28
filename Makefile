ifdef COMSPEC
DOTEXE:=.exe
else
DOTEXE:=
endif


CFLAGS:=-Ofast -Wall -Wextra -Wpedantic
LDFLAGS:=-s
LDLIBS:=


.PHONY: upd765pro-out
upd765pro-out: upd765pro$(DOTEXE)

%$(DOTEXE): %.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS) $(LDLIBS)
