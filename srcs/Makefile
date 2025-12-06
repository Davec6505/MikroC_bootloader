# ======================================================================#
# AUTHOR: David Coetzee													                        #
# DATE: 16/08/2024														                          #
# EMAIL: davec6505@gmail.com                                            #
#-----------------------------------------------------------------------#
# NOTES:																                                #
# This is a generic makefile for any c/c++ file/files with a complex or	#
# simple folder structure.												                      #
# un-comment the OS if using Linux as rsync does not work on windows	  #
# BUILD_TYPE for various outcomes for DEBUG / RELEASE					          #
#                                                                       #
#=======================================================================#
# Auto-detect OS
ifeq ($(OS),Windows_NT)
    OS_TYPE := WINDOWS
else
    UNAME_S := $(shell uname -s)
    ifeq ($(UNAME_S),Linux)
        OS_TYPE := LINUX
    endif
endif

BUILD_TYPE  = $(RELEASE)
#BUILD_TYPE  = $(DEBUG)
MODULE_NAME = mikro_hb
TARGET_NAME = $(MODULE_NAME)

ROOT_DIR := ..
OBJ_DIR  := $(ROOT_DIR)/objs
SRC_DIR  := 
INC_DIR  := $(ROOT_DIR)/incs

#*****DONT MODIFY BELOW ATTRIBUTES****#
BUILD_TIMESTAMP = $(shell date +'%d-%m-%Y.%H-%M-%S.%N')

INSTALLATION_PATH = $(shell echo $$INSTALLATION_PATH)
ifeq ($(INSTALLATION_PATH),)
				INSTALLATION_PATH = $(ROOT_DIR)
#       INSTALLATION_PATH = /usr/local/mycustombuild

endif

ifeq ($(OS_TYPE),LINUX)
SYNC := make install
INST_DIR := ~/bin
else
SYNC :=
endif

# C++ / C compiler
CXX = g++
CC  = gcc
COMPILER := c
ifeq ($(COMPILER),c)
 CMP = $(CC)
else 
 CMP = $(CXX)
endif


# Library / binary
CMP_TYPE := 
ifeq ($(CMP_TYPE),)
	TARGET_DIR = $(INSTALLATION_PATH)/bins
	TARGET = $(TARGET_DIR)/$(TARGET_NAME)
	LDXX := $(CMP)
else
 ifeq ($(CMP_TYPE),a)
	 TARGET_DIR = $(INSTALLATION_PATH)/libs
	 TARGET = $(TARGET_DIR)/lib$(TARGET_NAME).a
	 LDXX := ar rcs 
 else ifeq ($(CMP_TYPE),so)
	 TARGET_DIR = $(INSTALLATION_PATH)/libs
	 TARGET = $(TARGET_DIR)/lib$(TARGET_NAME).so
	 LDXX := -shared
	 endif
endif


#*****DONT MODIFY ABOVE ATTRIBUTES****#

ifeq ($(COMPILER),c)
 #SRCS := $(wildcard *.c)
 SRCS := USB.c Utils.c HexFile.c MikroHB.c
 OBJS := $(SRCS:%.c=$(OBJ_DIR)/%.o)
 STDFLAG := -std=c99
else
 SRCS := $(wildcard *.cpp)
 OBJS := $(SRCS:%.cpp=$(OBJ_DIR)/%.o)
 STDFLAG := -std=c++17 
endif

ifeq ($(OS_TYPE),LINUX)
INC := -I/usr/include/libusb-1.0
LDFLAGS := -lusb-1.0
else ifeq ($(OS_TYPE),WINDOWS)
    # On Windows, user should have MinGW64 bin and include in PATH
    # Or set LIBUSB_INCLUDE and LIBUSB_LIB environment variables
    LIBUSB_INCLUDE ?= /mingw64/include/libusb-1.0
    LIBUSB_LIB ?= /mingw64/lib
    INC = -I$(LIBUSB_INCLUDE)
    LDFLAGS := -L$(LIBUSB_LIB) -lusb-1.0 -lws2_32
endif
INC_LOCAL = -I$(ROOT_DIR)/incs

#choose release/debug
DEBUG   = -pipe -g -Wall -W -fPIC
RELEASE = -pipe -O2 -Wall -W -fPIC

# -D stands for DEFINE. If want to define any macro which is used in code for\
		#  timestamp or git revision etc, can be used in this way.
CC_OPT = -DBUILD_TIMESTAMP_STR=\"$(BUILD_TIMESTAMP)\" \
			 -DINSTALLATION_PATH_STR=\"$(INSTALLATION_PATH)\"

#UNCOMMENT IF LIKE TO SEE FOLLOWING WARNINGS. ATLEAST ONCE THIS NEEDS TO BE RUN\
		FOR EACH MODULE
WARN=-Wall -Wextra -Werror -Wwrite-strings -Wno-parentheses -pedantic  -Wno-pointer-sign\
		 -Warray-bounds -Wno-unused-variable -Wno-unused-function \
		 -Wno-unused-parameter -Wno-unused-result
#WARN = -g -Wall

CCFLAGS =  $(STDFLAG) $(BUILD_TYPE) $(CC_OPT) $(WARN) $(INC) $(INC_LOCAL)

all: $(TARGET)
	@echo $(SRCS) '=' $(OBJS)

$(TARGET): $(OBJS)
	$(LDXX) -o $@  $^ $(LDFLAGS)

$(OBJ_DIR)/%.o: %.c
	$(CMP) $(CCFLAGS) -c $< -o $@  

build_dir:
	@echo Creating object directory if not exist
	mkdir -p $(OBJ_DIR)
	mkdir -p $(TARGET_DIR) 
# It also can be written as $(TARGET_DIR)

clean:
	@echo Clean Build
	-rm -rf $(OBJS) $(TARGET)

install:
#rsync -avz *.h $(ROOT_DIR)/$(INC_DIR)
	rsync -vEp $(TARGET_DIR)/$(MODULE_NAME) $(INST_DIR)

.PHONY: clean build_dir all install

test:
		@echo $(SOURCE_1) $(OBJS)