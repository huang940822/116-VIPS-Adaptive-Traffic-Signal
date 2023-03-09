CC = gcc
CFLAGS = -g
# WARN_OPT = -Wall
WARN_OPT =
LIB_PATH := $(realpath j2735lib)
LDFLAGS = -g -L$(LIB_PATH) 
LIB_FILES := $(wildcard $(LIB_PATH)/*.so)
LIBS =  -lus_v2xcast -pthread -lrt -lm -lzmq

BUILD    := ./build
OBJ_DIR  := $(BUILD)/objects
EXEC_DIR := $(BUILD)/exec

TARGET  := middleware
APP_DIR := application

INCLUDE_DIR	 := include j2735inc $(wildcard $(APP_DIR)/*/include)
INCLUDE_PATH := $(foreach dir, $(INCLUDE_DIR), -I $(dir))
INCLUDE_FILE := $(foreach dir, $(INCLUDE_DIR), $(wildcard $(dir)/*.h))

linker_opt = -Wl,-rpath,'$$ORIGIN/../../j2735lib'

SRC_DIR  := src $(wildcard $(APP_DIR)/*/src)
SRC_FILE := $(foreach dir, $(SRC_DIR), $(wildcard $(dir)/*.c))
OBJECTS  := $(SRC_FILE:%.c=$(OBJ_DIR)/%.o)

TEST_SRC_FILE := $(filter-out src/main.c, $(SRC_FILE)) test/signal_packet_test.c
TEST_OBJECTS  := $(TEST_SRC_FILE:%.c=$(OBJ_DIR)/%.o)

TEST_DIR  := test
TEST_FILE := $(wildcard $(TEST_DIR)/*.c)

all: build $(EXEC_DIR)/$(TARGET) 

$(OBJ_DIR)/%.o: %.c
	@mkdir -p $(@D)
	$(CC) $(WARN_OPT) $(CFLAGS) $(INCLUDE_PATH) -c $< -o $@ $(LIBS)

$(EXEC_DIR)/$(TARGET): $(OBJECTS)
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -o $(EXEC_DIR)/$(TARGET) $^ $(LDFLAGS) $(LIBS) $(linker_opt)

PASS_COLOR = \e[32;01m
NO_COLOR = \e[0m

.PHONY:clean build check
build:
	@mkdir -p $(EXEC_DIR)
	@mkdir -p $(OBJ_DIR)

check:
	@$(foreach test_file,$(TEST_FILE),\
		echo "Compile "$(test_file);\
		$(CC) $(INCLUDE) -I $(INCLUDE_DIR) $(SRC_DIR)/msg_queue.c  $(LIBS) -o $(TEST_DIR)/testfile $(test_file); \
		echo "Execute "$(test_file);\
		./$(TEST_DIR)/testfile && printf "[ $(PASS_COLOR)Passed$(NO_COLOR) ]\n";\
	)
	@rm $(TEST_DIR)/testfile

clang-format:
	find -iname *.h -o -iname *.c | xargs clang-format -i

clean:
	-@rm -rvf $(OBJ_DIR)/*
	-@rm -rvf $(EXEC_DIR)/*

recompile:
	make clean
	make

DEPLOT_DIR := RSU_Controller

deploy:
	make clean
	make all CFLAGS:="-D DEBUG_MOD"
	script/deploy.sh
