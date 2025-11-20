CC ?= gcc
SRC = src/main.c src/data.c src/atlas.c src/mesh.c
OUT = game

PKG_CFLAGS := $(shell pkg-config --cflags raylib 2>/dev/null)
PKG_LIBS := $(shell pkg-config --libs raylib 2>/dev/null)

ifeq ($(PKG_LIBS),)
    ifeq ($(shell uname -s),Darwin)
        BREW_PREFIX := $(shell brew --prefix raylib 2>/dev/null)
        ifneq ($(BREW_PREFIX),)
            CFLAGS += -I$(BREW_PREFIX)/include
            LDFLAGS += -L$(BREW_PREFIX)/lib -lraylib -framework OpenGL -framework Cocoa -framework IOKit -framework CoreVideo
        else
            CFLAGS += -I./include
            LDFLAGS += -L./lib -lraylib
        endif
    else
        CFLAGS += -I./include
        LDFLAGS += -L./lib -lraylib -lopengl32 -lgdi32 -lwinmm -lws2_32
    endif
else
    CFLAGS += $(PKG_CFLAGS)
    LDFLAGS += $(PKG_LIBS)
endif

CFLAGS += -Wall -Wextra -O3 -march=native -I./include
LDFLAGS += -lm -pthread -ldl

all: $(OUT)

$(OUT): $(SRC)
	$(CC) $(CFLAGS) $(SRC) -o $(OUT) $(LDFLAGS)

clean:
	rm -f $(OUT)

.PHONY: all clean
