Chromium EC runtime -- dynamic memory allocation
------------------------------------------------

'shmalloc.c' and 'shared_mem.h' are the dynamic memory allocator from the Chromium
EC runtime enabled when the CONFIG_MALLOC is set.
It's a slightly modified version from the one submitted on the public master
branch:
https://chromium.googlesource.com/chromiumos/platform/ec/+/master/common/shmalloc.c
in order to build outside the the Chromium EC environment.

The 'malloc_pal.h' header should provide an interface close to what your code is
using provided you call FpcMallocInit(void *chunk, size_t size) at startup to
pass a contiguous chunk of memory to back the allocator.

'host_malloc_test2.c' runs a simple test on the allocation sequences provided by
FPC as stored in 'fpc_malloc_sequences.h'

e.g.
gcc -Wall -g -O2 shmalloc.c host_malloc_test2.c -o malloc_test
gcc -m32 -Wall -g -O2 shmalloc.c host_malloc_test2.c -o malloc_test32

$ ./malloc_test
472 steps -- max mem used: 306902 -- max chunks: 146
2002 steps -- max mem used: 352584 -- max chunks: 117
