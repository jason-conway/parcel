.POSIX:
CC     = cc
CFLAGS = -Wall -Werror -Wextra -Os
MKDIR  = mkdir -p
FFLAGS =

ifeq ($(OS), Windows_NT)
	LDFLAGS = -s -static
	LDLIBS  = -lpthread -lws2_32 -liphlpapi -ladvapi32 -lkernel32 -lshell32
	PREFIX  = %HOMEPATH%/parcel
	EXE     = .exe
	parcel_res = $(build_dir)parcel.res
	parceld_res =$(build_dir)parceld.res
	RESCMD_PARCEL  = windres.exe $(res_dir)parcel.rc -O coff -o $(parcel_res)
	RESCMD_PARCELD = windres.exe $(res_dir)parceld.rc -O coff -o $(parceld_res)
else
	LDFLAGS =
	LDLIBS  = -pthread
	PREFIX  = /usr/local
endif

src_dir   = ./src/
build_dir = ./build/
res_dir   = ./etc/resources/

parcel_dir       = $(src_dir)parcel/
parceld_dir      = $(src_dir)parceld/

parcel_source    = $(src_dir)*.c $(parcel_dir)*.c
parceld_source   = $(src_dir)*.c $(parceld_dir)*.c

parcel_all       = $(src_dir)*.* $(parcel_dir)*.*
parceld_all      = $(src_dir)*.* $(parceld_dir)*.*

parcel_includes  = -I$(src_dir) -I$(parcel_dir)
parceld_includes = -I$(src_dir) -I$(parceld_dir)

all: resources parcel$(EXE) parceld$(EXE)

resources:
	$(MKDIR) $(build_dir)
	$(RESCMD_PARCEL)
	$(RESCMD_PARCELD)

parcel$(EXE): $(parcel_all)
	$(CC) $(CFLAGS) $(parcel_includes) $(parcel_source) $(parcel_res) -o $(build_dir)$@ $(LDLIBS) $(LDFLAGS) $(FFLAGS)

parceld$(EXE): $(parceld_all)
	$(CC) $(CFLAGS) $(parceld_includes) $(parceld_source) $(parceld_res) -o $(build_dir)$@ $(LDLIBS) $(LDFLAGS) $(FFLAGS)

install: parcel parceld
	install -m 755 $(build_dir)parcel$(EXE) $(PREFIX)/bin
	install -m 755 $(build_dir)parceld$(EXE) $(PREFIX)/bin

clean:
	rm -f $(build_dir)*

uninstall:
	rm -f $(PREFIX)/bin/parcel$(EXE)
	rm -f $(PREFIX)/bin/parceld$(EXE)
