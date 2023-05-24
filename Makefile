# tool macros
CC ?=  clang # FILL: the compiler
CCFLAGS := -O2 -Wall -Werror -lreadline# FILL: compile flags
DBGFLAGS := -g -fsanitize=address
CCOBJFLAGS := -O2 -Wall -Werror $(CCOBJFLAGS) -c

# path macros
BIN_PATH := bin
OBJ_PATH := obj
SRC_PATH := src
DBG_PATH := debug

WORK_DIR  = $(shell pwd)

INC_PATH := $(WORK_DIR)/include $(INC_PATH)
INCLUDES = $(addprefix -I, $(INC_PATH))
CCOBJFLAGS := $(INCLUDES) $(CCOBJFLAGS)

# compile macros
TARGET_NAME := main
ifeq ($(OS),Windows_NT)
	TARGET_NAME := $(addsuffix .exe,$(TARGET_NAME))
endif
TARGET := $(BIN_PATH)/$(TARGET_NAME)
TARGET_DEBUG := $(DBG_PATH)/$(TARGET_NAME)

# src files & obj files
SRC := $(foreach x, $(SRC_PATH), $(wildcard $(addprefix $(x)/*,.c*)))
OBJ := $(addprefix $(OBJ_PATH)/, $(addsuffix .o, $(notdir $(basename $(SRC)))))
OBJ_DEBUG := $(addprefix $(DBG_PATH)/, $(addsuffix .o, $(notdir $(basename $(SRC)))))

# clean files list
DISTCLEAN_LIST := $(OBJ) \
                  $(OBJ_DEBUG)
CLEAN_LIST := $(TARGET) \
			  $(TARGET_DEBUG) \
			  $(DISTCLEAN_LIST)

# default rule
default: makedir all

# non-phony targets
$(TARGET): $(OBJ)
	$(CC) $(CCFLAGS) -o $@ $(OBJ)

$(OBJ_PATH)/%.o: $(SRC_PATH)/%.c*
	@echo + CC $<
	$(CC) $(CCOBJFLAGS) -o $@ $<

$(DBG_PATH)/%.o: $(SRC_PATH)/%.c*
	@echo + CC $<
	$(CC) $(CCOBJFLAGS) $(DBGFLAGS) -o $@ $<
	$(CC) -E $(INCLUDES) $< | \
		grep -ve '^#' | \
		clang-format - > $(basename $@).i

$(TARGET_DEBUG): $(OBJ_DEBUG)
	@echo + LD $@
	$(CC) $(CCFLAGS) $(DBGFLAGS) $(OBJ_DEBUG) -o $@

ARGS :=
MAIN_EXEC := $(TARGET)

# phony rules
.PHONY: makedir
makedir:
	@mkdir -p $(BIN_PATH) $(OBJ_PATH) $(DBG_PATH)

.PHONY: all
all: $(TARGET)

.PHONY: run
run: all
	$(MAIN_EXEC)

.PHONY: debug
debug: $(TARGET_DEBUG)
	gdb $(TARGET_DEBUG) --args $(TARGET_DEBUG) $(ARGS)

.PHONY: clean
clean:
	@echo CLEAN $(CLEAN_LIST)
	@rm -f $(CLEAN_LIST)

.PHONY: distclean
distclean:
	@echo CLEAN $(CLEAN_LIST)
	@rm -f $(DISTCLEAN_LIST)
