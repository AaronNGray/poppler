//========================================================================
//
// HashAlgorithm.h
//
// This file is licensed under the GPLv2 or later
//
// Copyright 2023 g10 Code GmbH, Author: Sune Stolborg Vuorela <sune@vuorela.dk>
// Copyright 2023 Albert Astals Cid <aacid@kde.org>
//========================================================================

#ifndef HASH_ALGORITHM_H
#define HASH_ALGORITHM_H

enum class HashAlgorithm
{
    Unknown,
    Md2,
    Md5,
    Sha1,
    Sha256,
    Sha384,
    Sha512,
    Sha224,
    Sha3_224,
    Sha3_256,
    Sha3_384,
    Sha3_512,
};

#endif // HASH_ALGORITHM_H
