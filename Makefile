.POSIX:
CC     = cc
CFLAGS = -Wall -Werror -Wextra -O0 -g3
MKDIR  = mkdir -p
FFLAGS =

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
	WINDRES = windres.exe
	res_dir = ./etc/resources/
	parcel_res = $(build_dir)parcel.res
	parceld_res = $(build_dir)parceld.res
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

resources:
	$(WINDRES) $(res_dir)parcel.rc -O coff -o $(parcel_res)
	$(WINDRES) $(res_dir)parceld.rc -O coff -o $(parceld_res)

parcel$(EXE): $(parcel_all)
	$(MKDIR) $(build_dir)
	$(CC) $(CFLAGS) $(parcel_includes) $(parcel_source) $(parcel_res) -o $(build_dir)$@ $(LDLIBS) $(LDFLAGS) $(FFLAGS)

parceld$(EXE): $(parceld_all)
	$(MKDIR) $(build_dir)
	$(CC) $(CFLAGS) $(parceld_includes) $(parceld_source) $(parceld_res) -o $(build_dir)$@ $(LDLIBS) $(LDFLAGS) $(FFLAGS)

install: parcel parceld
	mkdir -p $(PREFIX)/parcel/
	install -m 755 $(build_dir)parcel$(EXE) $(PREFIX)/bin
	install -m 755 $(build_dir)parceld$(EXE) $(PREFIX)/bin

uninstall:
	rm -f $(PREFIX)/bin/parcel$(EXE)
	rm -f $(PREFIX)/bin/parceld$(EXE)