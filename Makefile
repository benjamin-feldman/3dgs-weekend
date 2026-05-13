CXX ?= clang++
CC ?= clang
APP := app

CPPFLAGS := -Iinclude -Iexternal/glm $(shell pkg-config --cflags glfw3 2>/dev/null)
CXXFLAGS := -std=c++17 -Wall -Wextra -Wpedantic -O2
CFLAGS := -std=c99 -Wall -Wextra -Wpedantic -O2
LDLIBS := $(shell pkg-config --libs glfw3 2>/dev/null)

UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Darwin)
LDLIBS += -framework OpenGL
else
LDLIBS += -lGL
endif

OBJECTS := main.o src/glad.o

.PHONY: all run clean

all: $(APP)

$(APP): $(OBJECTS)
	$(CXX) $(OBJECTS) $(LDLIBS) -o $@

main.o: main.cpp include/gaussian_splat.h include/ply_loader.h include/shader.h
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

src/glad.o: src/glad.c include/glad/glad.h include/KHR/khrplatform.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

run: $(APP)
	./$(APP) $(or $(PLY),scene.ply)

clean:
	rm -f $(APP) $(OBJECTS)
