#pragma once

#include "Json.h"
#include <stdio.h>

JSON_API void JsonPrint(const JsonValue* value, FILE* out);
JSON_API void JsonWrite(const JsonValue* value, FILE* out);

JSON_API int JsonTempAllocator(JsonAllocator* allocator, void* buffer, int length);