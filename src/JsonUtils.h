#pragma once

#include "Json.h"
#include <stdio.h>

//JSON_API int  Json_hash(const void* buffer, int length);

JSON_API void JsonPrint(const Json value, FILE* out);
JSON_API void JsonWrite(const Json value, FILE* out);
