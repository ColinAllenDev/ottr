BIN = ottr
CC = clang

SRC_DIR = src
BUILD_DIR = build

SRCS := $(shell find $(SRC_DIR) -name '*.c')
OBJS = $(SRCS:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)
DEPS := $(OBJS:.o=.d)
PKGS = "wlroots-0.18" wayland-server xkbcommon
CF_PKGS != pkg-config --cflags $(PKGS)
LD_PKGS != pkg-config --libs $(PKGS)

CFLAGS =  $(CF_PKGS) -std=c11 -g -Wall -DWLR_USE_UNSTABLE
CPPFLAGS = -I$(SRC_DIR)
LDFLAGS = $(LD_PKGS) -lc

### Logger #############################################
date = $(shell date -u +%H:%M:%S)
log = @echo -e $(date) "\033[38;5;12m[MAKE]\033[0m" $(1) 
########################################################

.PHONY: all
all: $(BUILD_DIR)/$(BIN)

.PHONY: clean
clean:
	@rm -rf build 

.PHONY: bear
bear: clean
	@bear -- $(MAKE) -s all

.PHONY: run
run:
	@if [ -f $(BUILD_DIR)/$(BIN) ]; then \
		./$(BUILD_DIR)/$(BIN); \
	else \
		$(MAKE); \
		./$(BUILD_DIR)/$(BIN); \
	fi
	
# Compilation target
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(CPPFLAGS) -c $< -o $@ 

# Linker target
$(BUILD_DIR)/$(BIN): $(OBJS)
	$(CC) $(OBJS) -o $@ $(LDFLAGS)

-include $(DEPS)
