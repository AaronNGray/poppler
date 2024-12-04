//========================================================================
//
// CacheRef.cc
//
// This file is licensed under the GPLv2 or later
//
// Copyright 2024 Lucas Baudin
//========================================================================

#include "config.h"
#include <tuple>
#include <map>
#include <list>
#include "Stream.h"

#pragma once

/* A simple least recently used (LRU) cache, indexed by Refs. A standard
 * map is used to associate Refs to values, and a list (priorities) holds
 * the history of used values, so as when freeing space is needed, least
 * accessed values may be freed.
 * Values may have a custom size (this must be an approximation of the memory
 * used), and the sum of these sizes is guaranteed to be lower than maxCacheSize */
template<typename V>
class POPPLER_PRIVATE_EXPORT GenericCache
{
public:
    bool contains(Ref &c);
    bool contains(Object *c);

    void set(Object *o, V v);
    void set(Ref c, V v);
    void set(Object *o, V v, size_t s);
    void set(Ref c, V v, size_t s);

    void remove(Ref c);

    V &get(Ref c);

    GenericCache(const GenericCache &) = delete;
    GenericCache &operator=(const GenericCache &) = delete;

    GenericCache() { }

    explicit GenericCache(size_t size) : maxCacheSize(size) { }

private:
    void forget();
    size_t maxCacheSize = 1000;
    std::map<Ref, std::pair<V, std::list<Ref>::iterator>> cacheMap;
    std::list<Ref> priorities;
    size_t currentSize = 0;
    std::map<Ref, size_t> sizeItem;
};

// Image information: bits, color space, has alpha. See Gfx
typedef std::tuple<int, StreamColorSpaceMode, bool> imgInfo;



class POPPLER_PRIVATE_EXPORT CacheRef
{
public:
    void invalidateRef(Ref r);
    // This cache is used in Gfx.cc and holds some image information
    GenericCache<imgInfo> imgInfoCache;
};