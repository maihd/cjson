#include "LDtkParser.h"
#include "../../src/Json.h"

LDtkError LDtkParse(const char* content, int32_t contentLength, void* buffer, int32_t bufferSize, LDtkWorld* world)
{
    Json* json;
    JsonError jsonError = JsonParse(content, contentLength, JsonFlags_None, buffer, bufferSize, &json);
    if (jsonError.code != JsonError_None)
    {
        LDtkError error = { LDtkErrorCode_ParseJsonFailed, jsonError.message };
        return error;
    }

    LDtkError error = { LDtkErrorCode_None, "" };
    return error;
}
