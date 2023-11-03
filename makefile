all: main
run:  
	./main
main: glad
	g++ -o main main.cpp glad.o -lspdlog -lglfw -lfmt -g -ggdb 
glad:
	g++ -o glad.o glad.c -c