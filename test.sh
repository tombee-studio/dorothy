function try () {
    ./a.out $3 > test.ll
    clang -w -o test test.ll
    ./test
    if [ $? = $2 ]; then
        echo "($1) OK"
    else
        printf "\e[31m($1) NG: $? != $2\e[m\n"
        return
    fi
}
make test
./a.out
`rm -rf a.out`

make compile
try 1 11 "11"
try 2 12 "12"
try 3 10 "10"
try 4 0 "0"
try 5 101 "101"
`rm -rf test a.out test.ll`
