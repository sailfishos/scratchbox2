# LD_LIBRARY_PATH is readed by the rt-linker
set -e

cat > libfoo.c <<EOF
int alibcall(int a,int b) { return a+b; }
EOF

cat > foouser.c <<EOF
int alibcall(int a,int b);
main() { return alibcall(3,-3); }
EOF

gcc -fPIC libfoo.c -shared -o libfoo.so
gcc foouser.c -L. -lfoo -o foouser
if ./foouser 2> /dev/null; then
   echo Something wrong in the linking or rt-linker.
else
   LD_LIBRARY_PATH=$LD_LIBRARY_PATH:. ./foouser
fi
