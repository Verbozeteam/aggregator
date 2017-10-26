SRC_DIR := src
INC_DIR=src
BUILD_DIR := build
SRC_FILES := $(shell find $(SRC_DIR) -name '*.cpp')
OBJ_FILES := $(patsubst $(SRC_DIR)/%.cpp,$(BUILD_DIR)/%.o,$(SRC_FILES))

AGGREGATOR := aggregator
GPP_FLAGS := -Wall -MMD -MP -I$(INC_DIR)
LD_FLAGS :=

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
