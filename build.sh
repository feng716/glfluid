g++ -o glad.o glad.c -c&&
g++ -o main main.cpp glad.o -lspdlog -lglfw -lfmt &&
./main