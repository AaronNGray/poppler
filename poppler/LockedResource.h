//========================================================================
//
// LockedResource.h
//
// This file is licensed under the GPLv2 or later
//
// Copyright 2021-2025 Marco Trevisan (Treviño) <mail@3v1n0.net>
//
//========================================================================

#pragma once

#include <mutex>
#include <type_traits>

class LockableResource {
    template <typename T, typename U>
    friend class LockedResource;

    std::recursive_mutex mutex;

    void lock() {
        mutex.lock();
    }

    void unlock() {
        mutex.unlock();
    }
};

template <typename T, typename U = T>
class LockedResource {
 public:
    explicit LockedResource(T* res)
        : resource(static_cast<T*>(res)) {
        static_assert(std::is_base_of_v<LockableResource, T>);
        static_assert(std::is_base_of_v<U, T>);
        resource->lock();
    }
    ~LockedResource() { resource->unlock(); }
    constexpr U* operator->() { return resource; }
    constexpr U* operator->() const { return resource; }
    constexpr U& operator*() { return *resource; }
    constexpr U& operator*() const { return *resource; }

 protected:
    T* resource;
};
