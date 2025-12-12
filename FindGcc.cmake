# auto-gcc-toolchain.cmake

# -------------------------------------------------------------------
# 1. 查找 C 编译器（gcc）
# -------------------------------------------------------------------
# 尝试从环境变量 CC 中读取
if(DEFINED ENV{CC} AND NOT DEFINED CMAKE_C_COMPILER)
  set(CMAKE_C_COMPILER $ENV{CC})
endif()

# 如果还没定义，则在 PATH 中查找 gcc
if(NOT DEFINED CMAKE_C_COMPILER)
  find_program(GCC_EXECUTABLE
    NAMES gcc
    HINTS
      ENV PATH
  )
  if(NOT GCC_EXECUTABLE)
    message(FATAL_ERROR "Cannot find gcc in PATH or via \$CC")
  endif()
  set(CMAKE_C_COMPILER ${GCC_EXECUTABLE} CACHE FILEPATH "C compiler" FORCE)
endif()

# -------------------------------------------------------------------
# 2. 查找 C++ 编译器（g++）
# -------------------------------------------------------------------
# 尝试从环境变量 CXX 中读取
if(DEFINED ENV{CXX} AND NOT DEFINED CMAKE_CXX_COMPILER)
  set(CMAKE_CXX_COMPILER $ENV{CXX})
endif()

# 如果还没定义，则基于 gcc 路径推断 g++
if(NOT DEFINED CMAKE_CXX_COMPILER)
  # 将 /usr/bin/gcc 替换为 /usr/bin/g++
  get_filename_component(GCC_DIR "${CMAKE_C_COMPILER}" DIRECTORY)
  set(GPLUSPLUS_EXECUTABLE "${GCC_DIR}/g++")
  if(NOT EXISTS ${GPLUSPLUS_EXECUTABLE})
    # 如果推断失败，再次在 PATH 中查找
    find_program(GPLUSPLUS_EXECUTABLE
      NAMES g++
      HINTS
        ENV PATH
    )
  endif()
  if(NOT GPLUSPLUS_EXECUTABLE)
    message(FATAL_ERROR "Cannot find g++ in PATH or via \$CXX")
  endif()
  set(CMAKE_CXX_COMPILER ${GPLUSPLUS_EXECUTABLE} CACHE FILEPATH "C++ compiler" FORCE)
endif()
