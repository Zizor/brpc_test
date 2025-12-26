cmake_minimum_required(VERSION 3.11)
project(butil_object_pool LANGUAGES CXX)

include(GNUInstallDirs)

option(BUTIL_OBJECT_POOL_BUILD_TESTS "Build butil_object_pool tests" ON)
option(BUTIL_OBJECT_POOL_BUILD_EXAMPLES "Build butil_object_pool examples" ON)

add_library(butil_object_pool INTERFACE)
add_library(butil_object_pool::butil_object_pool ALIAS butil_object_pool)

target_compile_features(butil_object_pool INTERFACE cxx_std_17)
target_include_directories(butil_object_pool INTERFACE
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

find_package(Threads REQUIRED)
target_link_libraries(butil_object_pool INTERFACE Threads::Threads)

if(BUTIL_OBJECT_POOL_BUILD_EXAMPLES)
  add_executable(butil_object_pool_basic examples/basic.cpp)
  target_link_libraries(butil_object_pool_basic PRIVATE butil_object_pool)
endif()

if(BUTIL_OBJECT_POOL_BUILD_TESTS)
  include(CTest)
  add_executable(butil_object_pool_smoke_test tests/smoke_test.cpp)
  target_link_libraries(butil_object_pool_smoke_test PRIVATE butil_object_pool)
  add_test(NAME butil_object_pool_smoke_test COMMAND butil_object_pool_smoke_test)
endif()

install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(TARGETS butil_object_pool EXPORT butil_object_poolTargets)
install(EXPORT butil_object_poolTargets
  NAMESPACE butil_object_pool::
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/butil_object_pool
)

