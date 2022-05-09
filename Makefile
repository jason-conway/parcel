.POSIX:
CC     = cc
CFLAGS = -Wall -Werror -Wextra -O0 -g3

ifeq ($(OS), Windows_NT)
	LDFLAGS = -s -static
	LDLIBS  = -lpthread -lws2_32 -liphlpapi -ladvapi32 -lkernel32 -lshell32
	PREFIX  = %HOMEPATH%/parcel
	EXE     = .exe
else
	LDFLAGS = 
	LDLIBS  = -pthread
	PREFIX  = /usr/local
endif

src_dir   = ./src/
build_dir = ./build/

ifeq ($(OS), Windows_NT)
	res_dir   := ./etc/resources/
	parcel_resources = $(res_dir)icon.res $(res_dir)parcel.res 
endif

parcel_dir = $(src_dir)parcel/
parcel_source = $(src_dir)*.c $(parcel_dir)*.c
parcel_all = $(src_dir)*.* $(parcel_dir)*.*
parcel_includes = -I$(src_dir) -I$(parcel_dir)

parceld_dir = $(src_dir)parceld/
parceld_source = $(src_dir)*.c $(parceld_dir)*.c
parceld_all = $(src_dir)*.* $(parceld_dir)*.*
parceld_includes = -I$(src_dir) -I$(parceld_dir)


all: parcel$(EXE) parceld$(EXE)

parcel$(EXE): $(parcel_all)
	$(CC) $(CFLAGS) $(parcel_includes) $(parcel_source) $(parcel_resources) -o $(build_dir)$@ $(LDLIBS) $(LDFLAGS)

parceld$(EXE): $(parceld_all)
	$(CC) $(CFLAGS) $(parceld_includes) $(parceld_source) -o $(build_dir)$@ $(LDLIBS) $(LDFLAGS)

install: parcel parceld
	mkdir -p $(PREFIX)/parcel/
	install -m 755 $(build_dir)parcel$(EXE) $(PREFIX)/bin
	install -m 755 $(build_dir)parceld$(EXE) $(PREFIX)/bin

uninstall:
	rm -f $(PREFIX)/bin/parcel$(EXE)
	rm -f $(PREFIX)/bin/parceld$(EXE)