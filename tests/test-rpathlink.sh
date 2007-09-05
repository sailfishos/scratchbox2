# LD_LIBRARY_PATH is readed by the normal linker
# Especially the recursive library lookup is tested
set -e


cat > liblo.c <<EOF
int alowcall(int a,int b) {return a+b;}
EOF

cat > libhi.c <<EOF
int alowcall(int a,int b);
int ahicall(int a) {return alowcall(a,-a);}
EOF

cat > hiuser.c <<EOF
int ahicall(int a);
int main(void) {return ahicall(27);}
EOF

mkdir lo hi
gcc -fPIC liblo.c -shared -o lo/liblo.so
gcc -fPIC libhi.c -shared -o hi/libhi.so -Llo -llo
LD_LIBRARY_PATH=lo gcc hiuser.c -Lhi -lhi -o foouser
LD_LIBRARY_PATH=hi:lo ./foouser

