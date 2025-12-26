// Licensed to the Apache Software Foundation (ASF) under one
// or more contributor license agreements.  See the NOTICE file
// distributed with this work for additional information
// regarding copyright ownership.  The ASF licenses this file
// to you under the Apache License, Version 2.0 (the
// "License"); you may not use this file except in compliance
// with the License.  You may obtain a copy of the License at
//
//   http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing,
// software distributed under the License is distributed on an
// "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
// KIND, either express or implied.  See the License for the
// specific language governing permissions and limitations
// under the License.

#ifndef BUTIL_OBJECT_POOL_STANDALONE_OBJECT_POOL_H
#define BUTIL_OBJECT_POOL_STANDALONE_OBJECT_POOL_H

#include <cstddef>  // size_t

namespace butil_object_pool {

template <typename T> struct ObjectPoolBlockMaxSize {
    static const size_t value = 64 * 1024;  // bytes
};

template <typename T> struct ObjectPoolBlockMaxItem {
    static const size_t value = 256;
};

template <typename T> struct ObjectPoolFreeChunkMaxItem {
    static size_t value() { return 256; }
};

template <typename T> struct ObjectPoolValidator {
    static bool validate(const T*) { return true; }
};

}  // namespace butil_object_pool

#include "butil_object_pool/object_pool_inl.h"

namespace butil_object_pool {

template <typename T> inline T* get_object() {
    return ObjectPool<T>::singleton()->get_object();
}

template <typename T, typename A1>
inline T* get_object(const A1& arg1) {
    return ObjectPool<T>::singleton()->get_object(arg1);
}

template <typename T, typename A1, typename A2>
inline T* get_object(const A1& arg1, const A2& arg2) {
    return ObjectPool<T>::singleton()->get_object(arg1, arg2);
}

template <typename T> inline int return_object(T* ptr) {
    return ObjectPool<T>::singleton()->return_object(ptr);
}

template <typename T> inline void clear_objects() {
    ObjectPool<T>::singleton()->clear_objects();
}

template <typename T> ObjectPoolInfo describe_objects() {
    return ObjectPool<T>::singleton()->describe_objects();
}

}  // namespace butil_object_pool

#endif  // BUTIL_OBJECT_POOL_STANDALONE_OBJECT_POOL_H
