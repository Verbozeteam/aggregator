
SRC_DIR := src
INC_DIR := src
BUILD_DIR := build
SRC_FILES := $(shell find $(SRC_DIR) -name '*.cpp')
OBJ_FILES := $(patsubst $(SRC_DIR)/%.cpp,$(BUILD_DIR)/%.o,$(SRC_FILES))

GPP_INC_DIRS := -I$(INC_DIR) -I/usr/local/opt/openssl/include
GPP_LIB_DIRS := -L/usr/local/opt/openssl/lib
GPP_LIBS := -lpthread -lssl -lcrypto -lboost_program_options -lboost_log -lboost_system -lboost_thread -lboost_chrono -lboost_log_setup -lboost_filesystem -lwebsockets

AGGREGATOR := aggregator
GPP := g++
GPP_FLAGS := -g -std=c++14 -Wall -Werror -DBOOST_LOG_DYN_LINK
LD_FLAGS :=

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp
	@mkdir -p $(@D)
	$(GPP) $(GPP_FLAGS) $(GPP_INC_DIRS) -MMD -MP -c -o $@ $<

$(AGGREGATOR): $(OBJ_FILES)
	$(GPP) $(LD_FLAGS) $(GPP_LIB_DIRS) $(GPP_LIBS) -o $@ $^

all: $(AGGREGATOR)

rbp: GPP = /Volumes/xtools/armv8-rpi3-linux-gnueabihf/bin/armv8-rpi3-linux-gnueabihf-g++
rbp: GPP_LIB_DIRS += -L/usr/local/lib
rbp: GPP_INC_DIRS += -I/usr/local/include
rbp: LD_FLAGS += -dynamiclib --verbose
rbp: $(AGGREGATOR)

clean:
	rm -f $(AGGREGATOR)
	find $(BUILD_DIR) -not -name '.gitignore' -delete

-include $(OBJ_FILES:.o=.d)
