TARGET    := client

SRC_DIR   := src/
OBJ_DIR   := build/
SRC_FILES := $(wildcard $(SRC_DIR)*.cpp)

# obj files
OBJ_FILES := $(patsubst $(SRC_DIR)%.cpp,$(OBJ_DIR)%.o,$(SRC_FILES))
DEP_FILES := $(patsubst $(SRC_DIR)%.cpp,$(OBJ_DIR)%.d,$(SRC_FILES))


# compiler options
CPPFLAGS += -O2 -std=c++17 -Wall 

# link options
LDFLAGS += -l ssl -lpthread 


all: $(TARGET)

$(TARGET): $(OBJ_FILES)
	g++ -o $@ $^ $(LDFLAGS)
	
$(OBJ_DIR)%.o: $(SRC_DIR)%.cpp
	g++ -MMD -MP $(CPPFLAGS) -c $< -o $@

clean:
	rm -f $(TARGET) $(OBJ_FILES) $(DEP_FILES)
	

-include $(DEP_FILES)
