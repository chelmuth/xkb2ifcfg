#
# Check config
#

ifeq ($(filter clean cleanall, $(MAKECMDGOALS)),)
ifeq ($(GENODE_DIR),)
$(error Please set path to Genode sources in GENODE_DIR)
endif
ifeq ($(wildcard $(GENODE_DIR)/VERSION),)
$(error GENODE_DIR=$(GENODE_DIR) does not contain Genode sources)
endif
ifneq ($(shell pkg-config --print-errors --errors-to-stdout --exists xkbcommon),)
$(error Please install libxkbcommon-dev)
endif
endif

#
# Build rules
#

TARGET = xkb2ifcfg

SRC_CC = $(wildcard *.cc)
SRC_H  = $(wildcard *.h)

CFLAGS  = -Werror -Wall -Wextra -Wno-attributes -std=gnu++17 -ggdb
CFLAGS += -I$(GENODE_DIR)/repos/os/include
CFLAGS += -I$(GENODE_DIR)/repos/base/include
CFLAGS += -I$(GENODE_DIR)/repos/base/include/spec/64bit
CFLAGS += -I$(GENODE_DIR)/repos/base/include/spec/x86
CFLAGS += -I$(GENODE_DIR)/repos/base/include/spec/x86_64
CFLAGS += $(shell pkg-config --cflags --libs xkbcommon)

$(TARGET): $(SRC_CC) $(SRC_H) Makefile
	g++ -o $@ $(SRC_CC) $(CFLAGS)

cleanall clean:
	rm -f $(TARGET) *~


.PHONY: cleanall clean
