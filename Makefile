
SRC_DIR := src
INC_DIR := src
BUILD_DIR := build
SRC_FILES := $(shell find $(SRC_DIR) -name '*.cpp')
OBJ_FILES := $(patsubst $(SRC_DIR)/%.cpp,$(BUILD_DIR)/%.o,$(SRC_FILES))

GPP_INCLUDES := -I$(INC_DIR)
GPP_LIBS := -lboost_program_options -lboost_log -lpthread -lboost_system -lboost_thread -lboost_log_setup -lboost_filesystem

AGGREGATOR := aggregator
GPP_FLAGS := -g -std=c++11 -Wall -MMD -MP $(GPP_INCLUDES) -DBOOST_LOG_DYN_LINK
LD_FLAGS := $(GPP_LIBS)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp
	@mkdir -p $(@D)
	g++ $(GPP_FLAGS) -c -o $@ $<

$(AGGREGATOR): $(OBJ_FILES)
	g++ $(LD_FLAGS) -o $@ $^

all: $(AGGREGATOR)

clean:
	rm -f $(AGGREGATOR)
	find $(BUILD_DIR) -not -name '.gitignore' -delete

-include $(OBJ_FILES:.o=.d)
