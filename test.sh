gcc -nostdlib -O0 -c -I./lib lib/arena.c -o ./build/arena.o
echo did build arena

gcc -nostdlib -O0 -c -I./lib lib/development.c -o ./build/development.o
echo did build development

gcc -O0 -c -I./lib tests/main.c -o ./build/tests.o

gcc -fsanitize=address ./build/arena.o ./build/development.o ./build/tests.o -o ./build/final

echo "press some key to run the file"
read
clear

./build/final
