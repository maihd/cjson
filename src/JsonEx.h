#pragma once

#include "Json.h"
#include <stdio.h>

//JSON_API int  Json_hash(const void* buffer, int length);

JSON_API void Json_print(const Json* value, FILE* out);
JSON_API void Json_write(const Json* value, FILE* out);
