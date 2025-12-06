
# Auto-detect OS and export for sub-makefiles
ifeq ($(OS),Windows_NT)
    export OS_TYPE := WINDOWS
else
    UNAME_S := $(shell uname -s)
    ifeq ($(UNAME_S),Linux)
        export OS_TYPE := LINUX
    endif
endif

BUILD=make
CLEAN=make clean
BUILD_DIR=make build_dir
#THIRDP_LIB_PATH=/usr/local/mylibs/

all:
	@echo "###### BUILDING  ########"
	+(cd ./srcs; $(BUILD))
#	+(cd src/employee; $(BUILD) install)
#	+(cd src/db_mgr; $(BUILD))
#	+(cd src/ui_controller; $(BUILD))

3rd-party-libs:
#	(cd src/3rd_party/libzmq; $(BUILD) install)

build_dir:
	@echo "#######BUILDING DIRECTORIES FOR OUTPUT BINARIES#######"
	(cd ./srcs; $(BUILD_DIR))
#	(cd src/db_mgr; $(BUILD_DIR))
#	(cd src/ui_controller; $(BUILD_DIR))

clean:
	@echo "#######BUILDING DIRECTORIES FOR OUTPUT BINARIES#######"
	(cd ./srcs; $(CLEAN))
#	(cd src/db_mgr; $(CLEAN))
#	(cd src/ui_controller; $(CLEAN))

install:
	(cd ./srcs; $(BUILD) install)
#	(cd src/db_mgr; $(BUILD) install)
#	(cd src/ui_controller; $(BUILD) install)

.PHONY: all 3rd-party-libs build_dir clean install