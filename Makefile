CXX=g++
CXXFLAGS=-std=c++17 -O2
LDFLAGS=-lglfw -lGL -lGLU -lm

all: glass_ball

glass_ball: main.cpp stb_image_write.h
	$(CXX) $(CXXFLAGS) main.cpp -o glass_ball $(LDFLAGS)

clean:
	rm -f glass_ball screenshot_step*.png
