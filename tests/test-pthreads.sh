# pthreads work

set -e
CODE=pthreadtest
cat > $CODE.c <<EOF
#include <pthread.h>
#include <stddef.h>

void *thread_routine(void *data) {
    return data;
}

int main() {
    pthread_t thd;
    pthread_mutexattr_t mattr;
    pthread_once_t once_init = PTHREAD_ONCE_INIT;
    int data = 1;
    pthread_mutexattr_init(&mattr);
    return pthread_create(&thd, NULL, thread_routine, &data);
}
EOF
gcc $CODE.c -lpthread -o $CODE
#./$CODE
#fakeroot ./$CODE
echo "qemu threads are broken" >2
exit 66
