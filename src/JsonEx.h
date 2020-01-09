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

//JSON_API int  JsonHash(const void* buffer, int length);

JSON_API void JsonPrint(const Json* value, FILE* out);
JSON_API void JsonWrite(const Json* value, FILE* out);

typedef struct JsonTempAllocator
{
    JsonAllocator   super;
    void*           buffer;
    int             length;
    int             marker;
} JsonTempAllocator;

JSON_API bool JsonTempAllocator_Init(JsonTempAllocator* allocator, void* buffer, int length);
