#include "LDtkParser.h"
#include "../../src/Json.h"

LDtkError LDtkParse(const char* content, int32_t contentLength, void* buffer, int32_t bufferSize, LDtkWorld* world)
{
    const Json json;
    const JsonError jsonError = JsonParse(content, contentLength, JsonParseFlags_Default, buffer, bufferSize, (Json*)&json);
    if (jsonError.code != JsonError_None)
    {
        const LDtkError error = { LDtkErrorCode_ParseJsonFailed, jsonError.message };
        return error;
    }

    const Json jsonLevel;
    if (!JsonFind(json, "levels", (Json*)&jsonLevel))
    {
        const LDtkError error = { LDtkErrorCode_MissingLevels, "'levels' is not found" };
        return error;
    }

    const LDtkError error = { LDtkErrorCode_None, "" };
    return error;
}
