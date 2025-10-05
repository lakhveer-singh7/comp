CXX := clang++
CXXFLAGS := -std=c++20 -O0 -g -Wall -Wextra -Wpedantic
INCLUDE := -Iinclude -Ibuild
LDFLAGS := 

# Tools (may be missing in this environment)
FLEX := flex
BISON := bison

BUILD_DIR := build
SRC_DIR := src
OUT_DIR := outputs

# Generated files (placeholders for now)
LEXER_GEN := $(BUILD_DIR)/lexer.yy.cpp
PARSER_GEN := $(BUILD_DIR)/parser.tab.cpp
PARSER_HDR := $(BUILD_DIR)/parser.tab.hh

SRCS := \
  main.cpp \
  $(SRC_DIR)/utils/error_handler.cpp \
  $(SRC_DIR)/lexer/lexer.cpp \
  $(SRC_DIR)/parser/parser.cpp \
  $(SRC_DIR)/semantic/symbol_table.cpp \
  $(SRC_DIR)/semantic/semantic.cpp \
  $(SRC_DIR)/ir/ir_utils.cpp \
  $(SRC_DIR)/ir/ir_generator.cpp

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

# Flex/Bison rules (not used yet)
$(LEXER_GEN): src/lexer/lexer.l | $(BUILD_DIR)
	@echo "[info] flex not configured yet; creating stub $(LEXER_GEN)";
	@echo "// stub lexer" > $(LEXER_GEN)

$(PARSER_GEN) $(PARSER_HDR): src/parser/parser.y | $(BUILD_DIR)
	@echo "[info] bison not configured yet; creating stub $(PARSER_GEN) and $(PARSER_HDR)";
	@echo "// stub parser" > $(PARSER_GEN)
	@echo "// stub parser hdr" > $(PARSER_HDR)

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
