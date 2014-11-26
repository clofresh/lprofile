LUA_INCLUDE= /usr/local/include/lua51

PROFILER_OUTPUT= bin/lprofile.so

INCS= -I$(LUA_INCLUDE)
CC= gcc
WARN= -ansi -W -Wall
EXTRA_LIBS=
CFLAGS= -O2 -DTESTS $(WARN) $(INCS) -I./src

OBJS= src/lprofile.o

profiler: $(OBJS)
	export MACOSX_DEPLOYMENT_TARGET=10.3 && mkdir -p bin && gcc -bundle -undefined dynamic_lookup -o $(PROFILER_OUTPUT) $(OBJS)

clean:
	rm -f $(PROFILER_OUTPUT) src/*.o

install:
	install -m 0755 $(PROFILER_OUTPUT) ~/Library/Application\ Support/LOVE/

