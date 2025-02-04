//========================================================================
//
// CacheRef.cc
//
// This file is licensed under the GPLv2 or later
//
// Copyright 2024 Lucas Baudin
//========================================================================

#include "CacheRef.h"

template<typename V>
bool GenericCache<V>::contains(Ref &c)
{
    return cacheMap.contains(c);
}

template<typename V>
bool GenericCache<V>::contains(Object *c)
{
    if (c) {
        return cacheMap.contains(c->getRef());
    } else {
        return false;
    }
}

template<typename V>
void GenericCache<V>::set(Object *o, V v)
{
    set(o, std::move(v), 1);
}

template<typename V>
void GenericCache<V>::set(Ref c, V v)
{
    set(c, std::move(v), 1);
}

template<typename V>
void GenericCache<V>::set(Object *o, V v, size_t s)
{
    if (o) {
        Ref c = o->getRef();
        set(c, std::move(v), s);
    }
}

template<typename V>
void GenericCache<V>::set(Ref c, V v, size_t s)
{
    if (s >= maxCacheSize) {
        return;
    }
    if (s + currentSize >= maxCacheSize) {
        forget();
        set(c, std::move(v), s);
    } else {
        auto &e = cacheMap[c];
        e.first = std::move(v);
        priorities.push_front(c);
        e.second = priorities.begin();

        sizeItem[c] = s;
        currentSize += s;
    }
}

template<typename V>
void GenericCache<V>::forget()
{
    auto last = priorities.end();
    last--;
    cacheMap.erase(*last);
    currentSize -= sizeItem[*last];
    sizeItem.erase(*last);
    priorities.erase(last);
}

template<typename V>
void GenericCache<V>::remove(Ref c)
{
    if (contains(c)) {
        auto &n = cacheMap[c];
        priorities.erase(n.second);
        cacheMap.erase(c);
        currentSize -= sizeItem[c];
        sizeItem.erase(c);
    }
}

template<typename V>
V &GenericCache<V>::get(Ref c)
{
    auto &n = cacheMap[c];
    priorities.erase(n.second);
    priorities.push_front(c);
    n.second = priorities.begin();
    return n.first;
}

void CacheRef::invalidateRef(Ref r)
{
#ifdef HAVE_CAIRO
    if (cairoSurface) {
        cairoSurface->remove(r);
    }
#endif
    imgInfoCache.remove(r);
}

template class GenericCache<imgInfo>;

#ifdef HAVE_CAIRO
FreeableObject::~FreeableObject() { }
template class GenericCache<std::unique_ptr<FreeableObject>>;
#endif