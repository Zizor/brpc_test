# butil_object_pool (standalone)

Standalone extraction of `butil::ObjectPool<T>` from brpc/butil with minimal dependencies (C++17 + pthreads).

## Build (CMake)

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build
```

## Use (CMake)

```cmake
add_subdirectory(path/to/standalone/butil_object_pool)
target_link_libraries(your_target PRIVATE butil_object_pool)
```

Then include:

```cpp
#include "butil_object_pool/object_pool.h"
```

## Compile (CentOS 8)

```bash
g++ -std=c++17 -O3 -Wall -Wextra -pthread -Istandalone/butil_object_pool/include your.cc -o your_bin
```
