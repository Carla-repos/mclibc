gcc -nostdlib -Wall -Wextra -O0 -c -I./lib lib/arena.c -o ./build/arena.o
echo did build arena
gcc -Wall -Wextra -O0 -c -I./lib tests/main.c -o ./build/tests.o
gcc -fsanitize=address ./build/arena.o ./build/tests.o -o ./build/final
