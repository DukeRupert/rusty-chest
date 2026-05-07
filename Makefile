CC      := gcc
CFLAGS  := -std=c11 -Wall -Wextra -Wpedantic -O0 -g
SRCDIR  := src
BUILDDIR := build
TARGET  := $(BUILDDIR)/kvstore

SRCS := $(wildcard $(SRCDIR)/*.c)
OBJS := $(patsubst $(SRCDIR)/%.c,$(BUILDDIR)/%.o,$(SRCS))

.PHONY: all clean run

all: $(TARGET)

$(TARGET): $(OBJS) | $(BUILDDIR)
	$(CC) $(CFLAGS) -o $@ $^

$(BUILDDIR)/%.o: $(SRCDIR)/%.c | $(BUILDDIR)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILDDIR):
	mkdir -p $(BUILDDIR)

clean:
	rm -rf $(BUILDDIR)

run: $(TARGET)
	./$(TARGET)