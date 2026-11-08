# Custom Memory Allocator

A small, learning-focused custom memory allocator written in C for Windows.

This project does not wrap `malloc`. It implements its own allocation logic:
it stores metadata for every block, searches for reusable free blocks, splits
large blocks, joins adjacent free blocks, and provides its own versions of
`malloc`, `free`, `calloc`, and `realloc`. `VirtualAlloc` is used only as the
operating-system primitive that supplies raw readable and writable memory.

The implementation is intentionally small enough to read in one sitting,
while still showing the core ideas behind a memory allocator.

## What it provides

The public API is declared in [`src/allocator.h`](src/allocator.h):

```c
void *my_malloc(size_t size);
void  my_free(void *ptr);
void *my_calloc(size_t count, size_t size);
void *my_realloc(void *ptr, size_t size);
```

- `my_malloc` allocates an aligned block, or returns `NULL` if the request
  cannot be satisfied.
- `my_free` releases a block returned by this allocator. Passing `NULL` is
  safe.
- `my_calloc` allocates space for an array and clears it to zero. It rejects
  requests whose `count * size` would overflow.
- `my_realloc` preserves existing contents when it has to move a block.
  `my_realloc(NULL, size)` behaves like `my_malloc(size)`, and
  `my_realloc(ptr, 0)` frees `ptr` and returns `NULL`.

## How it works

Each allocation has a metadata header immediately before the memory returned
to the caller. The allocator keeps those headers in address order, which lets
it:

1. Reuse a free block when it is large enough.
2. Split a larger free block when the remainder is useful.
3. Join adjacent free blocks after a call to `my_free`.
4. Keep returned payloads aligned for any normal C type.

The implementation is in [`src/allocator.c`](src/allocator.c). It uses
`VirtualAlloc` as its backing system call, so this is currently a
Windows-only project. The allocator currently requests backing memory for
each new block rather than managing large arenas or pages itself; that is a
natural next step if the project is extended.

## Building

With MinGW-w64 GCC, compile the allocator as an object file:

```text
gcc -std=c11 -Wall -Wextra -Isrc -c src/allocator.c -o allocator.o
```

To use it in a program, compile that program together with
`src/allocator.c`:

```text
gcc -std=c11 -Wall -Wextra -Isrc src/allocator.c your_program.c -o your_program.exe
```

Then include the allocator header:

```c
#include "allocator.h"

int *values = my_calloc(10, sizeof(*values));
if (values != NULL) {
    values[0] = 42;
    my_free(values);
}
```

## Current limitations

This is a custom allocator, but it is still a learning implementation rather
than a production replacement:

- It is not thread-safe.
- Block lookup is linear, so performance degrades as the block list grows.
- Memory obtained from `VirtualAlloc` is kept for the lifetime of the process;
  free blocks are reused internally but are not returned to Windows.
- It does not yet use arenas, bins, page-level mapping, or a more efficient
  free-block index.
- Callers must only pass pointers returned by this allocator to `my_free` and
  `my_realloc`.

The `tests` directory is reserved for allocator tests as the implementation
develops.
