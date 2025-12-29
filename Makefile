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
	PARCEL_RES = $(BUILD_DIR)/parcel.res
	PARCELD_RES =$(BUILD_DIR)/parceld.res
	RESCMD_PARCEL  = windres.exe $(RES_DIR)/parcel.rc -O coff -o $(PARCEL_RES)
	RESCMD_PARCELD = windres.exe $(RES_DIR)/parceld.rc -O coff -o $(PARCELD_RES)
else
	LDFLAGS =
	LDLIBS  = -pthread
	PREFIX  = /usr/local
endif

PARCEL_DIR  = ./src/parcel
PARCELD_DIR = ./src/parceld
CRYPTO_DIR  = ./src/crypto
COMMON_DIR  = ./src/common
BIN_DIR     = ./bin
BUILD_DIR   = ./build
RES_DIR     = ./etc/resources

PARCEL_FILES  = $(wildcard $(PARCEL_DIR)/*)
PARCELD_FILES = $(wildcard $(PARCELD_DIR)/*)
CRYPTO_FILES  = $(wildcard $(CRYPTO_DIR)/*)
COMMON_FILES  = $(wildcard $(COMMON_DIR)/*)

PARCEL_SRCS  = $(filter %.c, $(PARCEL_FILES))
PARCELD_SRCS = $(filter %.c, $(PARCELD_FILES))
CRYPTO_SRCS  = $(filter %.c, $(CRYPTO_FILES))
COMMON_SRCS  = $(filter %.c, $(COMMON_FILES))
SHARED_SRCS  = $(CRYPTO_SRCS) $(COMMON_SRCS)

all: resources parcel$(EXE) parceld$(EXE)

resources:
	@mkdir -p $(BUILD_DIR)
	$(RESCMD_PARCEL)
	$(RESCMD_PARCELD)

parcel$(EXE): $(PARCEL_SRCS) $(SHARED_SRCS)
	@mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) -I$(CRYPTO_DIR) -I$(COMMON_DIR) $(PARCEL_RES) -o $(BIN_DIR)/$@ $^ $(LDLIBS) $(LDFLAGS) $(FFLAGS)

parceld$(EXE): $(PARCELD_SRCS) $(SHARED_SRCS)
	@mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) -I$(CRYPTO_DIR) -I$(COMMON_DIR) $(PARCELD_RES) -o $(BIN_DIR)/$@ $^ $(LDLIBS) $(LDFLAGS) $(FFLAGS)

install: parcel$(EXE) parceld$(EXE)
	install -m 755 $(BIN_DIR)parcel$(EXE) $(PREFIX)/bin
	install -m 755 $(BIN_DIR)parceld$(EXE) $(PREFIX)/bin

clean:
	rm -f $(BUILD_DIR)* $(BIN_DIR)

uninstall:
	rm -f $(PREFIX)/bin/parcel$(EXE)
	rm -f $(PREFIX)/bin/parceld$(EXE)
