CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -pedantic

all: shell

shell: shell.cpp
	$(CXX) $(CXXFLAGS) -o shell shell.cpp

clean:
	rm -f shell
