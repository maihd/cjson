#pragma once

#include "Json.h"
#include <stdio.h>

JSON_API void JsonPrint(const JsonValue* value, FILE* out);
JSON_API void JsonWrite(const JsonValue* value, FILE* out);

typedef struct JsonTempAllocator
{
    JsonAllocator   super;
    void*           buffer;
    int             length;
    int             marker;
} JsonTempAllocator;

JSON_API bool JsonTempAllocator_Init(JsonTempAllocator* allocator, void* buffer, int length);