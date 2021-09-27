# Execlp resolves binaries from path correctly
# This might for example fail, when errno is not 
# passed through correctly in exec calls.

set -e
CODE=execpathtest
cat > $CODE.c <<EOF
#include <unistd.h>
#include <stdio.h>
int main() {
	int rc=execlp("true","true",(char*)NULL);
	if(rc!=0) { perror("wOOt"); }
	return rc;
}
EOF
gcc $CODE.c -o $CODE
./$CODE
fakeroot ./$CODE
