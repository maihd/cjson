#include <string.h>

#include "LDtkParser.h"
#include "../../src/Json.h"

static inline uint8_t LDtkHexFromChar(char x)
{
    return (x >= 'a' && x <= 'f') * (x - 'a') + (x >= 'A' && x <= 'F') * (x - 'A') + (x >= '0' && x <= '9') * (x - '0');
}

static LDtkColor LDtkColorFromString(const char* value)
{
    if (*value == '#')
    {
        value++;
    }

    LDtkColor color;
    color.r = (LDtkHexFromChar(value[0]) << 8) | LDtkHexFromChar(value[1]);
    color.g = (LDtkHexFromChar(value[2]) << 8) | LDtkHexFromChar(value[3]);
    color.b = (LDtkHexFromChar(value[4]) << 8) | LDtkHexFromChar(value[5]);

    if (strlen(value) == 8)
    {
        color.a = (LDtkHexFromChar(value[6]) << 8) | LDtkHexFromChar(value[7]);
    }
    else
    {
        color.a = 0xff;
    }

    return color;
}

static LDtkError LDtkReadWorldProperties(const Json json, LDtkWorld* world)
{
    const Json jsonDefaultPivotX;
    if (!JsonFind(json, "defaultPivotX", (Json*)&jsonDefaultPivotX))
    {
        const LDtkError error = { LDtkErrorCode_MissingWorldProperties, "'defaultPivotX' is not found" };
        return error;
    }
    world->defaultPivotX = (float)jsonDefaultPivotX.number;

    const Json jsonDefaultPivotY;
    if (!JsonFind(json, "defaultPivotY", (Json*)&jsonDefaultPivotY))
    {
        const LDtkError error = { LDtkErrorCode_MissingWorldProperties, "'defaultPivotY' is not found" };
        return error;
    }
    world->defaultPivotY = (float)jsonDefaultPivotY.number;

    const Json jsonDefaultGridSize;
    if (!JsonFind(json, "defaultGridSize", (Json*)&jsonDefaultGridSize))
    {
        const LDtkError error = { LDtkErrorCode_MissingWorldProperties, "'defaultGridSize' is not found" };
        return error;
    }
    world->defaultGridSize = (int32_t)jsonDefaultGridSize.number;

    const Json jsonBackgroundColor;
    if (!JsonFind(json, "bgColor", (Json*)&jsonBackgroundColor))
    {
        const LDtkError error = { LDtkErrorCode_MissingWorldProperties, "'bgColor' is not found" };
        return error;
    }
    world->backgroundColor = LDtkColorFromString(jsonBackgroundColor.string);

    const Json jsonWorldLayoutName;
    if (!JsonFind(json, "worldLayout", (Json*)&jsonWorldLayoutName))
    {
        const LDtkError error = { LDtkErrorCode_MissingWorldProperties, "'worldLayout' is not found" };
        return error;
    }
    const char* worldLayoutName = jsonWorldLayoutName.string;
    if (strcmp(worldLayoutName, "Free") == 0)
    {
        world->layout = LDtkWorldLayout_Free;
    }
    else if (strcmp(worldLayoutName, "GridVania") == 0)
    {
        world->layout = LDtkWorldLayout_GridVania;
    }
    else if (strcmp(worldLayoutName, "LinearHorizontal") == 0)
    {
        world->layout = LDtkWorldLayout_LinearHorizontal;
    }
    else if (strcmp(worldLayoutName, "LinearVertical") == 0)
    {
        world->layout = LDtkWorldLayout_LinearVertical;
    }
    else
    {
        const LDtkError error = { LDtkErrorCode_InvalidWorldProperties, "Unknown GridLayout" };
        return error;
    }

    const LDtkError error = { LDtkErrorCode_None, "" };
    return error;
}

static LDtkError LDtkReadEnums(const Json jsonDefs, LDtkWorld* world)
{
    const Json jsonEnumDefs;
    if (!JsonFind(jsonDefs, "enums", (Json*)&jsonEnumDefs))
    {
        const LDtkError error = { LDtkErrorCode_MissingWorldProperties, "'defs.enums' is not found" };
        return error;
    }

    const LDtkError error = { LDtkErrorCode_None, "" };
    return error;
}

static LDtkError LDtkReadTilesets(const Json jsonDefs, LDtkWorld* world)
{
    const Json jsonTilesets;
    if (!JsonFind(jsonDefs, "tilesets", (Json*)&jsonTilesets))
    {
        const LDtkError error = { LDtkErrorCode_MissingWorldProperties, "'defs.tilesets' is not found" };
        return error;
    }

    const LDtkError error = { LDtkErrorCode_None, "" };
    return error;
}

static LDtkError LDtkReadLayerDefs(const Json jsonDefs, LDtkWorld* world)
{
    const Json jsonLayerDefs;
    if (!JsonFind(jsonDefs, "layers", (Json*)&jsonLayerDefs))
    {
        const LDtkError error = { LDtkErrorCode_MissingWorldProperties, "'defs.layers' is not found" };
        return error;
    }

    const int32_t layerDefCount = jsonLayerDefs.length;
    LDtkLayerDef* layerDefs = malloc();

    const LDtkError error = { LDtkErrorCode_None, "" };
    return error;
}

static LDtkError LDtkReadEntityDefs(const Json jsonDefs, LDtkWorld* world)
{
    const Json jsonEntityDefs;
    if (!JsonFind(jsonDefs, "entities", (Json*)&jsonEntityDefs))
    {
        const LDtkError error = { LDtkErrorCode_MissingWorldProperties, "'defs.entities' is not found" };
        return error;
    }

    const LDtkError error = { LDtkErrorCode_None, "" };
    return error;
}

LDtkError LDtkParse(const char* content, int32_t contentLength, void* buffer, int32_t bufferSize, LDtkWorld* world)
{
    const Json json;
    const JsonResult jsonResult = JsonParse(content, contentLength, JsonParseFlags_Default, buffer, bufferSize, (Json*)&json);
    if (jsonResult.error != JsonError_None)
    {
        const LDtkError error = { LDtkErrorCode_ParseJsonFailed, jsonResult.message };
        return error;
    }

    const LDtkError readPropertiesError = LDtkReadWorldProperties(json, world);
    if (readPropertiesError.code != LDtkErrorCode_None)
    {
        return readPropertiesError;
    }

    const Json jsonDefs;
    if (!JsonFind(json, "defs", (Json*)&jsonDefs))
    {
        const LDtkError error = { LDtkErrorCode_MissingWorldProperties, "'defs' is not found" };
        return error;
    }

    LDtkError readDefsError;

    readDefsError = LDtkReadEnums(jsonDefs, world);
    if (readDefsError.code != LDtkErrorCode_None)
    {
        return readDefsError;
    }

    readDefsError = LDtkReadTilesets(jsonDefs, world);
    if (readDefsError.code != LDtkErrorCode_None)
    {
        return readDefsError;
    }

    readDefsError = LDtkReadLayerDefs(jsonDefs, world);
    if (readDefsError.code != LDtkErrorCode_None)
    {
        return readDefsError;
    }

    readDefsError = LDtkReadEntityDefs(jsonDefs, world);
    if (readDefsError.code != LDtkErrorCode_None)
    {
        return readDefsError;
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
