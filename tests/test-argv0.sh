# argv[0] is passed correctly to scripts and executables
set -e


ASH=test-argvsh
ABIN=test-argvbin
GREP='grep -q'

cat > $ABIN.c <<EOF
#include <stdio.h>
int main(int argc,char **argv) {
printf("%s\n",argv[0]);
}
EOF

cat > $ASH <<EOF
#!/bin/sh
echo "\$0"
EOF

function failwith {
echo Failure in: $*
return 1
}

chmod +x ./$ASH
gcc -o $ABIN $ABIN.c

# Relative path
./$ASH | $GREP ^\./$ASH$ || failwith 'Relative invoking of shell'
./$ABIN | $GREP ^\./$ABIN$ || failwith 'Relative invoking of binary'

# Absolute path
$PWD/$ASH | $GREP ^$PWD/$ASH$ || failwith 'Absolute invoking of shell'
$PWD/$ABIN | $GREP ^$PWD/$ABIN$ || failwith 'Absolute invoking of binary'

mkdir -p argv0
DIR=$PWD
cd argv0
# Lookup from path with ..
PATH=$PATH:.. $ASH | $GREP ^\../$ASH$ || failwith 'Relative paths in PATH with shell'
PATH=$PATH:.. $ABIN | $GREP ^$ABIN$ || failwith 'Relative paths in PATH with binary'

# Lookup from path with absolute path
PATH=$PATH:$DIR $ASH | $GREP ^$DIR/$ASH$ || failwith 'Absolute paths in PATH with shell'
PATH=$PATH:$DIR $ABIN | $GREP ^$ABIN$ || failwith 'Absolute paths in PATH with binary'
