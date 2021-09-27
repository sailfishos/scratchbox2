# Fakeroot target-side id is root
set -e
cat > idtest.c <<EOF
#include <sys/types.h>
uid_t getuid(void);
int main(){return geteuid ()!=0;}
EOF
gcc idtest.c -o idtest
fakeroot ./idtest
