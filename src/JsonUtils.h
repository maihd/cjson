#pragma once

#include "Json.h"
#include <stdio.h>

JSON_API void JsonPrint(const Json value, FILE* out);
JSON_API void JsonWrite(const Json value, FILE* out);
