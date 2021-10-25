#include <string.h>

#include "LDtkParser.h"
#include "../../src/Json.h"

typedef struct Allocator
{
    uint8_t*        buffer;
    int32_t         bufferSize;

    uint8_t*        lowerMarker;
    uint8_t*        upperMarker;
} Allocator;

static bool InitAllocator(Allocator* allocator, void* buffer, int32_t bufferSize)
{
    if (buffer && bufferSize > 0)
    {
        allocator->buffer       = (uint8_t*)buffer;
        allocator->bufferSize   = bufferSize;
        allocator->lowerMarker  = (uint8_t*)buffer;
        allocator->upperMarker  = (uint8_t*)buffer + bufferSize;

        return true;
    }

    return false;
}

static void* CanAlloc(Allocator* allocator, int32_t size)
{
    int32_t remain = (int32_t)(allocator->upperMarker - allocator->lowerMarker);
    return  remain >= size;
}

static void DeallocLower(Allocator* allocator, void* buffer, int32_t size)
{
    void* lastBuffer = allocator->lowerMarker;
    if (lastBuffer == buffer)
    {
        allocator->lowerMarker -= size;
    }
}

static void* AllocLower(Allocator* allocator, void* oldBuffer, int32_t oldSize, int32_t newSize)
{
    DeallocLower(allocator, oldBuffer, oldSize);

    if (newSize <= 0)
    {
        return NULL;
    }

    if (CanAlloc(allocator, newSize))
    {
        void* result = allocator->lowerMarker;
        allocator->lowerMarker += newSize;
        return result;
    }

    return NULL;
}

static void DeallocUpper(Allocator* allocator, void* buffer, int32_t size)
{
    void* lastBuffer = allocator->upperMarker;
    if (lastBuffer == buffer)
    {
        allocator->upperMarker += size;
    }
}

static void* AllocUpper(Allocator* allocator, void* oldBuffer, int32_t oldSize, int32_t newSize)
{
    DeallocUpper(allocator, oldBuffer, oldSize); 

    if (newSize <= 0)
    {
        return NULL;
    }

    if (CanAlloc(allocator, newSize))
    {
        allocator->upperMarker -= newSize;
        return allocator->upperMarker;
    }

    return NULL;
}

static inline uint8_t HexFromChar(char x)
{
    return (x >= 'a' && x <= 'f') * (x - 'a') + (x >= 'A' && x <= 'F') * (x - 'A') + (x >= '0' && x <= '9') * (x - '0');
}

static LDtkColor LDtkColorFromUInt32(uint32_t value)
{
    const LDtkColor result = { (value >> 16), (value >> 8) & 0xff, value & 0xff, 0xff };
    return result;
}

static LDtkColor LDtkColorFromString(const char* value)
{
    if (!value)
    {
        const LDtkColor result = { 0 };
        return result;
    }

    if (*value == '#')
    {
        value++;
    }

    int32_t length = (int32_t)strlen(value);
    if (length != 6 && length != 8)
    {
        const LDtkColor empty = { 0, 0, 0, 0 };
        return empty;
    }

    LDtkColor color;
    color.r = (HexFromChar(value[0]) << 8) | HexFromChar(value[1]);
    color.g = (HexFromChar(value[2]) << 8) | HexFromChar(value[3]);
    color.b = (HexFromChar(value[4]) << 8) | HexFromChar(value[5]);

    if (length == 8)
    {
        color.a = (HexFromChar(value[6]) << 8) | HexFromChar(value[7]);
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

static LDtkError LDtkReadEnums(const Json jsonDefs, Allocator* allocator, LDtkWorld* world)
{
    const Json jsonEnums;
    if (!JsonFind(jsonDefs, "enums", (Json*)&jsonEnums))
    {
        const LDtkError error = { LDtkErrorCode_MissingWorldProperties, "'defs.enums' is not found" };
        return error;
    }

    const int32_t   enumCount = (int32_t)jsonEnums.length;
    LDtkEnum*       enums = (LDtkEnum*)AllocLower(allocator, NULL, 0, sizeof(LDtkEnum) * enumCount);
    for (int32_t i = 0; i < enumCount; i++)
    {
        LDtkEnum* enumDef = &enums[i];
        const Json jsonEnum = jsonEnums.array[i];

        const Json jsonUid;
        JsonFind(jsonEnum, "uid", (Json*)&jsonUid);
        enumDef->id = (int32_t)jsonUid.number;

        const Json jsonIdentifier;
        JsonFind(jsonEnum, "identifier", (Json*)&jsonIdentifier);
        enumDef->name = jsonIdentifier.string;

        const Json jsonIconTilesetUid;
        JsonFind(jsonEnum, "iconTilesetUid", (Json*)&jsonIconTilesetUid);
        enumDef->tilesetId = (int32_t)jsonIconTilesetUid.number;

        const Json jsonExternalRelPath;
        JsonFind(jsonEnum, "externalRelPath", (Json*)&jsonExternalRelPath);
        enumDef->externalPath = jsonExternalRelPath.string;

        const Json jsonExternalChecksum;
        JsonFind(jsonEnum, "externalChecksum", (Json*)&jsonExternalChecksum);
        enumDef->externalChecksum = jsonExternalChecksum.string;

        const Json jsonValues;
        JsonFind(jsonEnum, "values", (Json*)&jsonValues);

        const int32_t valueCount = (int32_t)jsonValues.length;
        LDtkEnumValue* values = (LDtkEnumValue*)AllocLower(allocator, NULL, 0, sizeof(LDtkEnumValue) * valueCount);
        for (int32_t j = 0; j < valueCount; j++)
        {
            LDtkEnumValue* value = &values[j];
            const Json jsonValue = jsonValues.array[j];

            const Json jsonId;
            JsonFind(jsonValue, "id", (Json*)&jsonId);
            value->name = jsonId.string;

            const Json jsonTileId;
            JsonFind(jsonValue, "tileId", (Json*)&jsonTileId);
            value->tileId = (int32_t)jsonTileId.number;

            const Json jsonColor;
            JsonFind(jsonValue, "color", (Json*)&jsonColor);
            value->color = LDtkColorFromUInt32((uint32_t)jsonColor.number);
        }

        enumDef->valueCount = valueCount;
        enumDef->values = values;
    }

    world->enumCount = enumCount;
    world->enums = enums;

    const LDtkError error = { LDtkErrorCode_None, "" };
    return error;
}

static LDtkError LDtkReadTilesets(const Json jsonDefs, Allocator* allocator, LDtkWorld* world)
{
    const Json jsonTilesets;
    if (!JsonFind(jsonDefs, "tilesets", (Json*)&jsonTilesets))
    {
        const LDtkError error = { LDtkErrorCode_MissingWorldProperties, "'defs.tilesets' is not found" };
        return error;
    }

    const int32_t   tilesetCount = (int32_t)jsonTilesets.length;
    LDtkTileset*    tilesets = (LDtkTileset*)AllocLower(allocator, NULL, 0, sizeof(LDtkTileset) * tilesetCount);
    for (int32_t i = 0; i < tilesetCount; i++)
    {
        LDtkTileset* tileset = &tilesets[i];
        const Json jsonTileset = jsonTilesets.array[i];

        const Json jsonUid;
        JsonFind(jsonTileset, "uid", (Json*)&jsonUid);
        tileset->id = (int32_t)jsonUid.number;

        const Json jsonIdentifier;
        JsonFind(jsonTileset, "identifier", (Json*)&jsonIdentifier);
        tileset->name = jsonIdentifier.string;

        const Json jsonRelPath;
        JsonFind(jsonTileset, "relPath", (Json*)&jsonRelPath);
        tileset->path = jsonRelPath.string;

        const Json jsonPxWid;
        JsonFind(jsonTileset, "pxWid", (Json*)&jsonPxWid);
        tileset->width = (int32_t)jsonPxWid.number;

        const Json jsonPxHei;
        JsonFind(jsonTileset, "pxHei", (Json*)&jsonPxHei);
        tileset->height = (int32_t)jsonPxHei.number;

        const Json jsonTileGridSize;
        JsonFind(jsonTileset, "tileGridSize", (Json*)&jsonTileGridSize);
        tileset->tileSize = (int32_t)jsonTileGridSize.number;

        const Json jsonSpacing;
        JsonFind(jsonTileset, "spacing", (Json*)&jsonSpacing);
        tileset->spacing = (int32_t)jsonSpacing.number;

        const Json jsonPadding;
        JsonFind(jsonTileset, "padding", (Json*)&jsonPadding);
        tileset->padding = (int32_t)jsonPadding.number;

        const Json jsonTagsSourceEnumUid;
        if (JsonFind(jsonTileset, "tagsSourceEnumUid", (Json*)&jsonTagsSourceEnumUid))
        {
            tileset->tagsEnumId = (int32_t)jsonTagsSourceEnumUid.number;
        }
        else
        {
            tileset->tagsEnumId = 0;
        }
    }

    world->tilesetCount = tilesetCount;
    world->tilesets = tilesets;

    const LDtkError error = { LDtkErrorCode_None, "" };
    return error;
}

static LDtkError LDtkReadLayerDefs(const Json jsonDefs, Allocator* allocator, LDtkWorld* world)
{
    const Json jsonLayerDefs;
    if (!JsonFind(jsonDefs, "layers", (Json*)&jsonLayerDefs))
    {
        const LDtkError error = { LDtkErrorCode_MissingWorldProperties, "'defs.layers' is not found" };
        return error;
    }

    const int32_t layerDefCount = jsonLayerDefs.length;
    LDtkLayerDef* layerDefs = AllocLower(allocator, NULL, 0, sizeof(LDtkLayerDef) * layerDefCount);

    for (int32_t i = 0; i < layerDefCount; i++)
    {
        LDtkLayerDef* layerDef = &layerDefs[i];

        const Json jsonLayerDef = jsonLayerDefs.array[i];

        const Json jsonType;
        JsonFind(jsonLayerDef, "type", (Json*)&jsonType);

        const char* typeName = jsonType.string;
        if (strcmp(typeName, "Tiles") == 0)
        {
            layerDef->type = LDtkLayerType_Tiles;
        }
        else if (strcmp(typeName, "IntGrid") == 0)
        {
            layerDef->type = LDtkLayerType_IntGrid;
        }
        else if (strcmp(typeName, "Entities") == 0)
        {
            layerDef->type = LDtkLayerType_Entities;
        }
        else if (strcmp(typeName, "AutoLayer") == 0)
        {
            layerDef->type = LDtkLayerType_AutoLayer;
        }

        const Json jsonIdentifier;
        JsonFind(jsonLayerDef, "identifier", (Json*)&jsonIdentifier);
        layerDef->name = jsonIdentifier.string;

        const Json jsonUid;
        JsonFind(jsonLayerDef, "uid", (Json*)&jsonUid);
        layerDef->id = (int32_t)jsonUid.number;

        const Json jsonGridSize;
        JsonFind(jsonLayerDef, "gridSize", (Json*)&jsonGridSize);
        layerDef->gridSize = (int32_t)jsonGridSize.number;

        const Json jsonDisplayOpacity;
        JsonFind(jsonLayerDef, "displayOpacity", (Json*)&jsonDisplayOpacity);
        layerDef->opacity = (float)jsonDisplayOpacity.number;

        const Json jsonPxOffsetX;
        JsonFind(jsonLayerDef, "pxOffsetX", (Json*)&jsonPxOffsetX);
        layerDef->offsetX = (int32_t)jsonPxOffsetX.number;

        const Json jsonPxOffsetY;
        JsonFind(jsonLayerDef, "pxOffsetY", (Json*)&jsonPxOffsetY);
        layerDef->offsetY = (int32_t)jsonPxOffsetY.number;

        const Json jsonTilePivotX;
        JsonFind(jsonLayerDef, "tilePivotX", (Json*)&jsonTilePivotX);
        layerDef->tilePivotX = (int32_t)jsonTilePivotX.number;

        const Json jsonTilePivotY;
        JsonFind(jsonLayerDef, "tilePivotY", (Json*)&jsonTilePivotY);
        layerDef->tilePivotY = (int32_t)jsonTilePivotY.number;

        int32_t tilesetDefId = -1;
        const Json jsonTilesetDefUid;
        if (JsonFind(jsonLayerDef, "tilesetDefUid", (Json*)&jsonTilesetDefUid))
        {
            tilesetDefId = (int32_t)jsonTilesetDefUid.number;
        }

        const Json jsonAutoTilesetDefUid;
        if (JsonFind(jsonLayerDef, "autoTilesetDefUid", (Json*)&jsonAutoTilesetDefUid))
        {
            tilesetDefId = (int32_t)jsonAutoTilesetDefUid.number;
        }

        if (tilesetDefId == -1)
        {
            const LDtkError error = { LDtkErrorCode_InvalidLayerDefProperties, "'tilesetDefId' is invalid" };
            return error;
        }
    }

    world->layerDefCount = layerDefCount;
    world->layerDefs = layerDefs;

    const LDtkError error = { LDtkErrorCode_None, "" };
    return error;
}

static LDtkError LDtkReadEntityDefs(const Json jsonDefs, Allocator* allocator, LDtkWorld* world)
{
    const Json jsonEntityDefs;
    if (!JsonFind(jsonDefs, "entities", (Json*)&jsonEntityDefs))
    {
        const LDtkError error = { LDtkErrorCode_MissingWorldProperties, "'defs.entities' is not found" };
        return error;
    }

    const int32_t   entityDefCount = (int32_t)jsonEntityDefs.length;
    LDtkEntityDef*  entityDefs = (LDtkEntityDef*)AllocLower(allocator, NULL, 0, sizeof(LDtkEntityDef) * entityDefCount);

    for (int32_t i = 0; i < entityDefCount; i++)
    {
        LDtkEntityDef* entityDef = &entityDefs[i];
        const Json jsonEntityDef = jsonEntityDefs.array[i];

        const Json jsonUid;
        JsonFind(jsonEntityDef, "uid", (Json*)&jsonUid);
        entityDef->id = (int32_t)jsonUid.number;
        
        const Json jsonIdentifier;
        JsonFind(jsonEntityDef, "identifier", (Json*)&jsonIdentifier);
        entityDef->name = jsonIdentifier.string;

        const Json jsonWidth;
        JsonFind(jsonEntityDef, "width", (Json*)&jsonWidth);
        entityDef->width = (int32_t)jsonWidth.number;

        const Json jsonHeight;
        JsonFind(jsonEntityDef, "height", (Json*)&jsonHeight);
        entityDef->width = (int32_t)jsonHeight.number;

        const Json jsonColor;
        JsonFind(jsonEntityDef, "color", (Json*)&jsonColor);
        entityDef->color = LDtkColorFromString(jsonColor.string);

        const Json jsonPivotX;
        JsonFind(jsonEntityDef, "pivotX", (Json*)&jsonPivotX);
        entityDef->pivotX = (int32_t)jsonPivotX.number;

        const Json jsonPivotY;
        JsonFind(jsonEntityDef, "pivotY", (Json*)&jsonPivotY);
        entityDef->pivotY = (int32_t)jsonPivotY.number;

        const Json jsonTilesetId;
        JsonFind(jsonEntityDef, "tilesetId", (Json*)&jsonTilesetId);
        entityDef->tilesetId = (int32_t)jsonTilesetId.number;

        const Json jsonTileId;
        JsonFind(jsonEntityDef, "tileId", (Json*)&jsonTileId);
        entityDef->tileId = (int32_t)jsonTileId.number;

        const Json jsonTags;
        JsonFind(jsonEntityDef, "tags", (Json*)&jsonTags);
        entityDef->tagCount = (int32_t)jsonTags.length;
        entityDef->tags = AllocLower(allocator, NULL, 0, sizeof(const char*) * entityDef->tagCount);
        for (int32_t i = 0; i < entityDef->tagCount; i++)
        {
            entityDef->tags[i] = jsonTags.array[i].string;
        }
    }

    world->entityDefCount = entityDefCount;
    world->entityDefs = entityDefs;

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

    Allocator allocator;
    if (!InitAllocator(&allocator, (uint8_t*)buffer + jsonResult.memoryUsage, bufferSize - jsonResult.memoryUsage))
    {
        const LDtkError error = { LDtkErrorCode_OutOfMemory, "Buffer is too small" };
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

    readDefsError = LDtkReadEnums(jsonDefs, &allocator, world);
    if (readDefsError.code != LDtkErrorCode_None)
    {
        return readDefsError;
    }

    readDefsError = LDtkReadTilesets(jsonDefs, &allocator, world);
    if (readDefsError.code != LDtkErrorCode_None)
    {
        return readDefsError;
    }

    readDefsError = LDtkReadLayerDefs(jsonDefs, &allocator, world);
    if (readDefsError.code != LDtkErrorCode_None)
    {
        return readDefsError;
    }

    readDefsError = LDtkReadEntityDefs(jsonDefs, &allocator, world);
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
