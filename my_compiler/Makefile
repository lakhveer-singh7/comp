CXX := clang++
CXXFLAGS := -std=c++20 -O0 -g -Wall -Wextra -Wpedantic
INCLUDE := -Iinclude -Ibuild
LDFLAGS := 

# Tools
FLEX := flex
BISON := bison
FLEXFLAGS :=
BISONFLAGS := -d

BUILD_DIR := build
SRC_DIR := src
OUT_DIR := outputs

# Always use Flex/Bison (user environment provides them)

# Generated files
LEXER_GEN := $(BUILD_DIR)/lexer.yy.cpp
PARSER_GEN := $(BUILD_DIR)/parser.tab.cpp
PARSER_HDR := $(BUILD_DIR)/parser.tab.hh

# Base sources
SRCS := \
  main.cpp \
  $(SRC_DIR)/utils/error_handler.cpp \
  $(SRC_DIR)/semantic/symbol_table.cpp \
  $(SRC_DIR)/semantic/semantic.cpp \
  $(SRC_DIR)/ir/ir_utils.cpp \
  $(SRC_DIR)/ir/ir_generator.cpp \
  $(LEXER_GEN) \
  $(PARSER_GEN)
 CXXFLAGS += -DUSE_FLEX_BISON=1

OBJS := $(SRCS:.cpp=.o)

.PHONY: all clean run test

all: $(OUT_DIR)/output.ll

# Minimal pipeline: build executable and emit a stub IR file
$(OUT_DIR)/output.ll: mycc
	@mkdir -p $(OUT_DIR)
	./mycc -o $(OUT_DIR)/output.ll inputs/example.mc || true

mycc: $(OBJS)
	$(CXX) $(CXXFLAGS) $(INCLUDE) -o $@ $(OBJS) $(LDFLAGS)

# Object compilation pattern
%.o: %.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDE) -c $< -o $@

# Ensure parser header exists before compiling sources that include it
main.o: $(PARSER_HDR)
$(BUILD_DIR)/lexer.yy.o: $(PARSER_HDR)

# Flex/Bison rules
$(LEXER_GEN): src/lexer/lexer.l $(PARSER_HDR) | $(BUILD_DIR)
	$(FLEX) $(FLEXFLAGS) -o $@ $<

$(PARSER_GEN) $(PARSER_HDR): src/parser/parser.y | $(BUILD_DIR)
	$(BISON) $(BISONFLAGS) --defines=$(PARSER_HDR) -o $(PARSER_GEN) $<

$(BUILD_DIR):
	@mkdir -p $(BUILD_DIR)

clean:
	rm -rf $(BUILD_DIR) mycc $(OUT_DIR)

run: all
	@echo "---- output.ll ----"
	@sed -n '1,120p' $(OUT_DIR)/output.ll || true

# basic test placeholder
test: mycc
	@echo "[tests] not implemented yet"
