GTEST_CFLAGS = $(shell pkg-config --cflags gtest)
GTEST_LIBS   = $(shell pkg-config --libs gtest_main) -lpthread

.PHONY: setup lib cli test

setup:
	git config core.hooksPath .githooks

cli:
		make lib
		g++ --std=c++17 -o dist/dorothy cli/main.cpp Lib/libdorothy.a
		sudo cp dist/dorothy /usr/local/bin/

test:
		g++ -c --std=c++17 src/code.cpp -o obj/code.o
		g++ -c --std=c++17 src/cpu.cpp -o obj/cpu.o
		g++ -c --std=c++17 src/lexer.cpp -o obj/lexer.o
		g++ -c --std=c++17 src/ast.cpp -o obj/ast.o
		g++ -c --std=c++17 src/parser.cpp -o obj/parser.o
		g++ -c --std=c++17 src/llvm_gen.cpp -o obj/llvm_gen.o
		g++ --std=c++17 $(GTEST_CFLAGS) \
			tests/gtest_code.cpp \
			tests/gtest_lexer.cpp \
			tests/gtest_parser.cpp \
			tests/gtest_integration.cpp \
			obj/*.o $(GTEST_LIBS) -o dist/test_runner
		./dist/test_runner

lib:
		g++ -c --std=c++17 src/code.cpp -o obj/code.o
		g++ -c --std=c++17 src/cpu.cpp -o obj/cpu.o
		g++ -c --std=c++17 src/lexer.cpp -o obj/lexer.o
		g++ -c --std=c++17 src/ast.cpp -o obj/ast.o
		g++ -c --std=c++17 src/parser.cpp -o obj/parser.o
		g++ -c --std=c++17 src/llvm_gen.cpp -o obj/llvm_gen.o
		ar -rcs Lib/libdorothy.a obj/*.o

clean:
		rm -rf Lib obj dist
		mkdir Lib
		mkdir obj
		mkdir dist
