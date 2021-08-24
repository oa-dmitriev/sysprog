all: generate compile run

generate: gen/generator.py
	python3 gen/generator.py -f "test1" -c 10000 -m 10000
	python3 gen/generator.py -f "test2" -c 10000 -m 10000
	python3 gen/generator.py -f "test3" -c 10000 -m 10000
	python3 gen/generator.py -f "test4" -c 10000 -m 10000
	python3 gen/generator.py -f "test5" -c 10000 -m 10000
	python3 gen/generator.py -f "test6" -c 100000 -m 10000

compile: main.c ./file/myfile.h ./file/myfile.c ./sort/mysort.h global.h coro.h
	clang -fsanitize=address -Wall -g3 -o0 -o main main.c ./sort/mysort.c ./file/myfile.c -lrt

run: 
	ASAN_OPTIONS=detect_leaks=1 ./main 1000 test1 test2 test3 test4 test5 test6

test:
	python3 ./gen/checker.py -f mergedFile

clean: 
	rm -rf test* main *.o mergedFile
