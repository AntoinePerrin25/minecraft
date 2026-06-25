CC ?= gcc

# Use pkg-config to pull raylib include/link flags when available
RAYLIB_CFLAGS := $(shell pkg-config --cflags raylib 2>/dev/null)
RAYLIB_LIBS   := $(shell pkg-config --libs raylib 2>/dev/null)

SRCDIR = src
INCDIR = include
BUILDDIR = build

CFLAGS = -Wall -Wextra -O2 -march=native -I$(INCDIR) $(RAYLIB_CFLAGS)
LDFLAGS = $(RAYLIB_LIBS) -lm -pthread -ldl
PROFILING ?= 0
CFLAGS += -DPROFILING=$(PROFILING)

ifeq ($(RAYLIB_LIBS),)
    ifeq ($(shell uname -s),Darwin)
        BREW_PREFIX := $(shell brew --prefix raylib 2>/dev/null)
        ifneq ($(BREW_PREFIX),)
            CFLAGS += -I$(BREW_PREFIX)/include
            LDFLAGS += -L$(BREW_PREFIX)/lib -lraylib -framework OpenGL -framework Cocoa -framework IOKit -framework CoreVideo
        else
            LDFLAGS += -L./lib -lraylib
        endif
    else
        LDFLAGS += -L./lib -lraylib -lopengl32 -lgdi32 -lwinmm -lws2_32
    endif
endif

SOURCES = \
	$(SRCDIR)/main.c \
	$(SRCDIR)/data.c \
	$(SRCDIR)/atlas.c \
	$(SRCDIR)/mesh.c \
	$(SRCDIR)/perlin.c

OBJECTS = $(patsubst $(SRCDIR)/%.c,$(BUILDDIR)/%.o,$(SOURCES))
DEPS = $(OBJECTS:.o=.d)

TARGET = game

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

# Générer les dépendances automatiquement et compiler
$(BUILDDIR)/%.o: $(SRCDIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

# Inclure les fichiers de dépendances générés
-include $(DEPS)

clean:
	rm -rf $(BUILDDIR) $(TARGET)

run: $(TARGET)
	./$(TARGET)

.PHONY: all clean run
