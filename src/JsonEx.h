/******************************************************
 * Simple json state written in ANSI C
 *
 * @author: MaiHD
 * @license: Public domain
 * @copyright: MaiHD @ ${HOME}, 2018 - 2020
 ******************************************************/

#pragma once

#include "Json.h"
#include <stdio.h>

//JSON_API int  Json_hash(const void* buffer, int length);

JSON_API void Json_print(const Json* value, FILE* out);
JSON_API void Json_write(const Json* value, FILE* out);

typedef struct JsonTempAllocator
{
    JsonAllocator   super;
    void*           buffer;
    int             length;
    int             marker;
} JsonTempAllocator;

JSON_API bool JsonTempAllocator_init(JsonTempAllocator* allocator, void* buffer, int length);
