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
		g++ --std=c++17 unittest/code.cpp obj/*.o
		./a.out
		g++ --std=c++17 unittest/lexer.cpp obj/*.o
		./a.out
		g++ --std=c++17 unittest/parser.cpp obj/*.o
		./a.out
		g++ --std=c++17 -o cpu unittest/cpu.cpp obj/*.o
		g++ --std=c++17 -o compiler unittest/compile.cpp obj/*.o

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
