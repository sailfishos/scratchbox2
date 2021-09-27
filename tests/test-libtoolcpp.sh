# libtool can link c++ programs
set -e

echo 'main() {return 0;}' | g++ -xc -c - -o ltrun.o
libtool --mode=link g++ -o ltrun ltrun.o

