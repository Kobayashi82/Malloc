<div align="center">

![System & Kernel](https://img.shields.io/badge/System-brown?style=for-the-badge)
![Memory Management](https://img.shields.io/badge/Memory-Management-blue?style=for-the-badge)
![Dynamic Allocation](https://img.shields.io/badge/Dynamic-Allocation-green?style=for-the-badge)
![C Language](https://img.shields.io/badge/Language-C-red?style=for-the-badge)

*A reimplementation of malloc and its associated functions*

</div>

<div align="center">
  <img src="/Malloc.png">
</div>

# Malloc

[README en Español](README_es.md)

Malloc is a 42 School project that implements a complete dynamic memory management system. This implementation goes significantly beyond the basic requirements, incorporating advanced allocation techniques used by production allocators like glibc malloc.

## ✨ Features

### Core Functionality

- **Standard functions**: `malloc()`, `calloc()`, `free()`, `realloc()`
- **Additional functions**: `reallocarray()`, `aligned_alloc()`, `memalign()`, `posix_memalign()`, `malloc_usable_size()`, `valloc()`, `pvalloc()`
- **Debug functions**: `mallopt()`, `show_alloc_history()`, `show_alloc_mem()`, `show_alloc_mem_ex()`
- **Thread safety**: Full support for multithreaded apps and forks without deadlocks
- **Zone management**: TINY, SMALL, and LARGE zones

### Advanced features

#### Arena system
- `Multiple arenas`: Each thread can use separate arenas to reduce contention
- `Load balancing`: Smart distribution across available arenas

#### Memory optimizations
- `Bins`: Management of freed chunks to optimize reuse
- `Coalescing`: Automatic merging of adjacent free blocks
- `Alignment`: Optimal memory alignment
- `Headers`: Efficient use of header space

#### Protection and safety
- `Pointer validation`: Validates addresses within managed space
- `Corruption checks`: Memory integrity verification

## 🔧 Installation

```bash
git clone git@github.com:Kobayashi82/Malloc.git
cd malloc
make

# The library is generated in ./lib as:
# libft_malloc_$(HOSTTYPE).so

# and a symbolic link is created:
# libft_malloc.so -> libft_malloc_$(HOSTTYPE).so
```

## 🖥️ Usage

### Basic usage
```bash
# Preload the library
export LD_LIBRARY_PATH="[malloc_path]/lib:$LD_LIBRARY_PATH"
export LD_PRELOAD="libft_malloc.so"

# or

export LD_PRELOAD="[malloc_path]/lib/libft_malloc.so"

# o

# Run loader
./tester/load.sh

# and then

# Run
./program
```

### Integration code
```c
#include <stdlib.h>
#include "malloc.h"

int main() {
    // Use malloc normally
    void *ptr = malloc(1024);
    
    // Show memory state
    show_alloc_mem();
    
    // Free memory
    free(ptr);
    
    return 0;
}
```

### Compile with the library
```bash
# Compile and link
gcc -o program program.c -I./inc -L./lib -lft_malloc -Wl,-rpath=./lib

# -o program		Executable name
# -I./inc			Searches .h in ./inc (preprocessor)
# -L./lib			Adds ./lib to shared library search path (linker)
# -lft_malloc		Links with libft_malloc.so
# -Wl,-rpath=./lib	Binary will look for shared libraries in ./lib at runtime

# Run
./program
```

## 🧪 Testing

### Test suite
```bash
# Evaluation test
./tester/evaluation.sh

# Full tests
./tester/complete.sh              # All tests
./tester/complete.sh --main       # Main tests
./tester/complete.sh --alignment  # Alignment tests
./tester/complete.sh --extra      # Extra features tests
./tester/complete.sh --stress     # Stress tests
./tester/complete.sh --help       # Show help
```

## 🔧 Environment Variables

The following environment variables can configure malloc behavior:

| Environment variable     | Internal equivalent       | Description                              |
|--------------------------|---------------------------|------------------------------------------|
| **MALLOC_ARENA_MAX**     | `M_ARENA_MAX`             | Maximum number of arenas                 |
| **MALLOC_ARENA_TEST**    | `M_ARENA_TEST`            | Test threshold for dropping arenas       |
| **MALLOC_PERTURB_**      | `M_PERTURB`               | Fills heap with a pattern                |
| **MALLOC_CHECK_**        | `M_CHECK_ACTION`          | Action on memory errors                  |
| **MALLOC_MIN_USAGE_**    | `M_MIN_USAGE`             | Minimum usage threshold for optimization |
| **MALLOC_DEBUG**         | `M_DEBUG`                 | Enables debug mode                       |
| **MALLOC_LOGGING**       | `M_LOGGING`               | Enables logging                          |
| **MALLOC_LOGFILE**       | *(file path)*             | Log file (default: `"auto"`)             |

## 📚 Additional Functions

#### MALLOPT

- Configures memory allocator parameters.

```c
  int mallopt(int param, int value);

  param – option selector (M_* constant).
  value – value assigned to the option.

    • On success: returns 1.
    • On failure: returns 0 and sets errno to:
      – EINVAL: unsupported param or invalid value.

Supported params:
  • M_ARENA_MAX (-8)       (1-64/128):  Maximum number of arenas allowed.
  • M_ARENA_TEST (-7)         (1-160):  Number of arenas at which a hard limit on arenas is computed.
  • M_PERTURB (-6)          (0-32/64):  Sets memory to the PERTURB value on allocation, and to value ^ 255 on free.
  • M_CHECK_ACTION (-5)         (0-2):  Behaviour on abort errors (0: abort, 1: warning, 2: silence).
  • M_MIN_USAGE (3)           (0-100):  Heaps under this usage % are skipped (unless all are under).
  • M_DEBUG (7)                 (0-1):  Enables debug mode (1: errors, 2: system).
  • M_LOGGING (8)               (0-1):  Enables logging mode (1: to file, 2: to stderr).

Notes:
  • Changes are not allowed after the first memory allocation.
  • If both M_DEBUG and M_LOGGING are enabled:
      – uses $MALLOC_LOGFILE if defined, or fallback to "/tmp/malloc_[PID].log"
```

#### SHOW_ALLOC_MEM

- Shows information about current allocated memory state and provides a summary of blocks in use.

**Example output:**
```
————————————
 • Arena #1
———————————————————————————————————————
 • Allocations: 7       • Frees: 1
 • TINY: 1              • SMALL: 1
 • LARGE: 0             • TOTAL: 2
———————————————————————————————————————

 SMALL : 0x70000
— — — — — — — — — — — — — — — — —
 0x70010 - 0x707e0 : 2000 bytes
                    — — — — — — —
                     2000 bytes

 TINY : 0xf0000
— — — — — — — — — — — — — — — — —
 0xf0010 - 0xf0020 : 16 bytes
 0xf0030 - 0xf0040 : 16 bytes
 0xf0050 - 0xf0060 : 16 bytes
 0xf0070 - 0xf0080 : 16 bytes
 0xf0090 - 0xf00a0 : 16 bytes
                    — — — — — — —
                     80 bytes

———————————————————————————————————————
 2080 bytes in arena #1


———————————————————————————————————————————————————————————————
 • 7 allocations, 1 free and 2080 bytes across 1 arena
```


#### SHOW_ALLOC_MEM_EX

- Extended version of show_alloc_mem with more detailed allocation info.

**Example output:**
```
——————————————————————————————————————
 • Pointer: 0x703ab8cbf010 (Arena #1)
————————————————————————————————————————————————————————————————————————————————————
 • Size: 112 bytes      • Offset: 0 bytes      • Length: 112 bytes
————————————————————————————————————————————————————————————————————————————————————
 0x703ab8cbf000  71 00 00 00 00 00 00 00  89 67 45 23 01 ef cd ab  q........gE#....
————————————————————————————————————————————————————————————————————————————————————
 0x703ab8cbf010  48 65 6c 6c 6f 20 57 6f  72 6c 64 21 00 00 00 00  Hello World!....
 0x703ab8cbf020  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  ................
 0x703ab8cbf030  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  ................
 0x703ab8cbf040  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  ................
 0x703ab8cbf050  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  ................
 0x703ab8cbf060  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  ................
 0x703ab8cbf070  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  ................
————————————————————————————————————————————————————————————————————————————————————
```
#### SHOW_ALLOC_HISTORY

- Shows the history of allocations and frees performed by the program.

## 📄 License

This project is licensed under the WTFPL – [Do What the Fuck You Want to Public License](http://www.wtfpl.net/about/).

---

<div align="center">

**💾 Developed as part of the 42 School curriculum 💾**

*"Because glibc is too mainstream"*

</div>
