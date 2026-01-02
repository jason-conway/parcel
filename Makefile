.POSIX:
CC     = cc
CFLAGS = -Wall -Werror -Wextra -O0
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

SRC_DIR     = ./src
PARCEL_DIR  = $(SRC_DIR)/parcel
CONSOLE_DIR = $(PARCEL_DIR)/console
PARCELD_DIR = $(SRC_DIR)/parceld
CRYPTO_DIR  = $(SRC_DIR)/crypto
COMMON_DIR  = $(SRC_DIR)/common
WIRE_DIR    = $(COMMON_DIR)/wire
BIN_DIR     = ./bin
BUILD_DIR   = ./build
RES_DIR     = ./etc/resources

PARCEL_FILES  = $(wildcard $(PARCEL_DIR)/*)
CONSOLE_FILES = $(wildcard $(CONSOLE_DIR)/*)
PARCELD_FILES = $(wildcard $(PARCELD_DIR)/*)
CRYPTO_FILES  = $(wildcard $(CRYPTO_DIR)/*)
COMMON_FILES  = $(wildcard $(COMMON_DIR)/*)
WIRE_FILES    = $(wildcard $(WIRE_DIR)/*)

PARCEL_SRCS   = $(filter %.c, $(PARCEL_FILES))
CONSOLE_SRCS  = $(filter %.c, $(CONSOLE_FILES))

PARCELD_SRCS = $(filter %.c, $(PARCELD_FILES))
CRYPTO_SRCS  = $(filter %.c, $(CRYPTO_FILES))
COMMON_SRCS  = $(filter %.c, $(COMMON_FILES))
WIRE_SRCS    = $(filter %.c, $(WIRE_FILES))

SHARED_SRCS  = $(CRYPTO_SRCS) $(COMMON_SRCS) $(WIRE_SRCS)
INCLUDE_DIRS = -I$(CRYPTO_DIR) -I$(COMMON_DIR) -I$(WIRE_DIR) -I$(CONSOLE_DIR)

all: resources parcel$(EXE) parceld$(EXE)

resources:
	@mkdir -p $(BUILD_DIR)
	$(RESCMD_PARCEL)
	$(RESCMD_PARCELD)

parcel$(EXE): $(PARCEL_SRCS) $(CONSOLE_SRCS) $(SHARED_SRCS)
	@mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) $(INCLUDE_DIRS) $(PARCEL_RES) -o $(BIN_DIR)/$@ $^ $(LDLIBS) $(LDFLAGS) $(FFLAGS)

parceld$(EXE): $(PARCELD_SRCS) $(SHARED_SRCS)
	@mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) $(INCLUDE_DIRS) $(PARCELD_RES) -o $(BIN_DIR)/$@ $^ $(LDLIBS) $(LDFLAGS) $(FFLAGS)

install: parcel$(EXE) parceld$(EXE)
	install -m 755 $(BIN_DIR)parcel$(EXE) $(PREFIX)/bin
	install -m 755 $(BIN_DIR)parceld$(EXE) $(PREFIX)/bin

clean:
	rm -f $(BUILD_DIR)* $(BIN_DIR)

uninstall:
	rm -f $(PREFIX)/bin/parcel$(EXE)
	rm -f $(PREFIX)/bin/parceld$(EXE)
