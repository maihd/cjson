#ifndef __JSON_UTILS_H__
#define __JSON_UTILS_H__

#include "Json.h"
#include <stdio.h>

JSON_API void JsonPrint(const Json value, FILE* out);
JSON_API void JsonWrite(const Json value, FILE* out);

#endif // __JSON_UTILS_H__
