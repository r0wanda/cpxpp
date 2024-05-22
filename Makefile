main: *.cpp *.hpp
	g++ -o cpx index.cpp pkgs/*.cpp -std=c++20 -Wall -g3