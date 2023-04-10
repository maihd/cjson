#define _CRT_SECURE_NO_WARNINGS

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

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
	const int32_t alignment = 16;
	const int32_t mask = alignment - 1;

    if (buffer && bufferSize > 0)
    {
		const uintptr_t address = (uintptr_t)buffer;
		const int32_t	misalign = address & mask;
		const int32_t	adjustment = alignment - misalign;

		buffer = (void*)(address + adjustment);
		bufferSize = bufferSize - adjustment;
		bufferSize = bufferSize - (alignment - (bufferSize & mask));
		
        allocator->buffer       = (uint8_t*)buffer;
        allocator->bufferSize   = bufferSize;
        allocator->lowerMarker  = (uint8_t*)buffer;
        allocator->upperMarker  = (uint8_t*)buffer + bufferSize;

        return true;
    }

    return false;
}

static int32_t AllocatorRemainSize(Allocator* allocator)
{
	int32_t remain = (int32_t)(allocator->upperMarker - allocator->lowerMarker);
	return  remain;
}

static bool CanAlloc(Allocator* allocator, int32_t size)
{
    return AllocatorRemainSize(allocator) >= size;
}

static void DeallocLower(Allocator* allocator, void* buffer, int32_t size)
{
    void* lastBuffer = allocator->lowerMarker - size;
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
		tileset->index = i;

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

		const Json jsonIntGridValues;
		if (JsonFindWithType(jsonLayerDef, "intGridValues", JsonType_Array, (Json*)&jsonIntGridValues) != JsonError_None)
		{
			const LDtkError error = { LDtkErrorCode_InvalidLayerDefProperties, "'intGridValues' is invalid" };
			return error;
		}

		int32_t intGridValueCount = jsonIntGridValues.length;
		LDtkIntGridValue* intGridValues = (LDtkIntGridValue*)AllocLower(allocator, NULL, 0, intGridValueCount * sizeof(LDtkIntGridValue));
		for (int32_t i = 0; i < intGridValueCount; i++)
		{
			Json jsonIntGridValue = jsonIntGridValues.array[i];

			Json jsonIdentifier;
			if (!JsonFind(jsonIntGridValue, "identifier", &jsonIdentifier))
			{
				const LDtkError error = { LDtkErrorCode_InvalidLayerDefProperties, "'identifier' is invalid" };
				return error;
			}

			Json jsonColor;
			if (JsonFindWithType(jsonIntGridValue, "color", JsonType_String, &jsonColor) != JsonError_None)
			{
				const LDtkError error = { LDtkErrorCode_InvalidLayerDefProperties, "'color' is invalid" };
				return error;
			}

			Json jsonValue;
			if (JsonFindWithType(jsonIntGridValue, "value", JsonType_Number, &jsonValue) != JsonError_None)
			{
				const LDtkError error = { LDtkErrorCode_InvalidLayerDefProperties, "'color' is invalid" };
				return error;
			}

			LDtkIntGridValue* intGridValue = &intGridValues[i];

			intGridValue->name = jsonIdentifier.string;
			intGridValue->value = (int32_t)jsonValue.number;
			intGridValue->color = LDtkColorFromString(jsonColor.string);
		}

		layerDef->intGridValueCount = intGridValueCount;
		layerDef->intGridValues = intGridValues;
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
        entityDef->height = (int32_t)jsonHeight.number;

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

static LDtkError LDtkReadLayer(const Json json, Allocator* allocator, LDtkLevel* level, LDtkWorld* world)
{
	LDtkLayer* layer = &level->layers[level->layerCount++];

	// Name & type

	Json jsonIdentifier;
	if (JsonFindWithType(json, "__identifier", JsonType_String, &jsonIdentifier) != JsonError_None)
	{
		const LDtkError error = { LDtkErrorCode_UnnameError, "" };
		return error;
	}
	layer->name = jsonIdentifier.string;

	Json type;
	if (JsonFindWithType(json, "__type", JsonType_String, &type) != JsonError_None)
	{
		const LDtkError error = { LDtkErrorCode_UnnameError, "" };
		return error;
	}

	if (strcmp(type.string, "Tiles") == 0)
	{
		layer->type = LDtkLayerType_Tiles;
	}
	else if (strcmp(type.string, "Entities") == 0)
	{
		layer->type = LDtkLayerType_Entities;
	}
	else if (strcmp(type.string, "IntGrid") == 0)
	{
		layer->type = LDtkLayerType_IntGrid;
	}
	else if (strcmp(type.string, "AutoLayer") == 0)
	{
		layer->type = LDtkLayerType_AutoLayer;
	}
	else
	{
		const LDtkError error = { LDtkErrorCode_UnknownLayerType, "" };
		return error;
	}

	// Meta ids

	Json jsonLevelId;
	if (JsonFindWithType(json, "levelId", JsonType_Number, &jsonLevelId) != JsonError_None)
	{
		const LDtkError error = { LDtkErrorCode_UnnameError, "" };
		return error;
	}
	layer->levelId = (int32_t)jsonLevelId.number;

	Json jsonLayerDefUid;
	if (JsonFindWithType(json, "layerDefUid", JsonType_Number, &jsonLayerDefUid) != JsonError_None)
	{
		const LDtkError error = { LDtkErrorCode_UnnameError, "" };
		return error;
	}
	layer->layerDefId = (int32_t)jsonLayerDefUid.number;

	// Base properties

	Json jsonCWid;
	if (JsonFindWithType(json, "__cWid", JsonType_Number, &jsonCWid) != JsonError_None)
	{
		const LDtkError error = { LDtkErrorCode_UnnameError, "" };
		return error;
	}
	layer->rows = (int32_t)jsonCWid.number;

	Json jsonCHei;
	if (JsonFindWithType(json, "__cHei", JsonType_Number, &jsonCHei) != JsonError_None)
	{
		const LDtkError error = { LDtkErrorCode_UnnameError, "" };
		return error;
	}
	layer->cols = (int32_t)jsonCHei.number;

	Json jsonGridSize;
	if (JsonFindWithType(json, "__gridSize", JsonType_Number, &jsonGridSize) != JsonError_None)
	{
		const LDtkError error = { LDtkErrorCode_UnnameError, "" };
		return error;
	}
	layer->tileSize = (int32_t)jsonGridSize.number;

	Json jsonOpacity;
	if (JsonFindWithType(json, "__opacity", JsonType_Number, &jsonOpacity) != JsonError_None)
	{
		const LDtkError error = { LDtkErrorCode_UnnameError, "" };
		return error;
	}
	layer->opacity = (float)jsonOpacity.number;

	Json jsonPxTotalOffsetX;
	if (JsonFindWithType(json, "__pxTotalOffsetX", JsonType_Number, &jsonPxTotalOffsetX) != JsonError_None)
	{
		const LDtkError error = { LDtkErrorCode_UnnameError, "" };
		return error;
	}
	layer->offsetX = (int32_t)jsonPxTotalOffsetX.number;

	Json jsonPxTotalOffsetY;
	if (JsonFindWithType(json, "__pxTotalOffsetY", JsonType_Number, &jsonPxTotalOffsetY) != JsonError_None)
	{
		const LDtkError error = { LDtkErrorCode_UnnameError, "" };
		return error;
	}
	layer->offsetY = (int32_t)jsonPxTotalOffsetY.number;

	Json jsonVisible;
	if (JsonFindWithType(json, "visible", JsonType_Boolean, &jsonVisible) != JsonError_None)
	{
		const LDtkError error = { LDtkErrorCode_UnnameError, "" };
		return error;
	}
	layer->visible = jsonVisible.boolean;

	if (layer->type != LDtkLayerType_Entities)
	{
		Json jsonTilesetDefUid;
		if (JsonFindWithType(json, "__tilesetDefUid", JsonType_Number, &jsonTilesetDefUid) != JsonError_None)
		{
			const LDtkError error = { LDtkErrorCode_UnnameError, "" };
			return error;
		}

		Json jsonTilesetRelPath;
		if (JsonFindWithType(json, "__tilesetRelPath", JsonType_String, &jsonTilesetRelPath) != JsonError_None)
		{
			const LDtkError error = { LDtkErrorCode_UnnameError, "" };
			return error;
		}

		int32_t tilesetIndex = -1;
        const uint32_t tilesetId = (int32_t)jsonTilesetDefUid.number;
        for (int32_t i = 0; i < world->tilesetCount; i++)
        {
            if (world->tilesets[i].id == tilesetId)
            {
                tilesetIndex = i;
                break;
            }
        }

        if (tilesetIndex == -1)
        {
            const LDtkError error = { LDtkErrorCode_UnnameError, "" };
            return error;
        }

        const LDtkTileset tileset = world->tilesets[tilesetIndex];
		assert(strcmp(tileset.path, jsonTilesetRelPath.string) == 0);
		layer->tileset = tileset;
	}
    else
    {
        layer->tileset = (LDtkTileset){ 0 };
    }

    // Read Tiles

	const char* gridTilesFieldName = "gridTiles";
	if (layer->type == LDtkLayerType_IntGrid || layer->type == LDtkLayerType_AutoLayer)
	{
		gridTilesFieldName = "autoLayerTiles";
	}

	const int32_t coordIdIndex = layer->type == LDtkLayerType_IntGrid || layer->type == LDtkLayerType_AutoLayer;

	Json jsonGridTiles;
	if (JsonFindWithType(json, gridTilesFieldName, JsonType_Array, &jsonGridTiles) == JsonError_None)
	{
        int32_t tileCount = jsonGridTiles.length;
	    LDtkTile* tiles = (LDtkTile*)AllocLower(allocator, NULL, 0, tileCount * sizeof(LDtkTile));
	    for (int32_t i = 0; i < tileCount; i++)
	    {
		    const Json jsonTile = jsonGridTiles.array[i];

            // Read Tile fields

		    Json jsonTileId;
		    if (JsonFindWithType(jsonTile, "t", JsonType_Number, &jsonTileId) != JsonError_None)
		    {
			    const LDtkError error = { LDtkErrorCode_UnnameError, "" };
			    return error;
		    }
		    int32_t tileId = (int32_t)jsonTileId.number;

		    Json jsonD;
		    if (JsonFindWithType(jsonTile, "d", JsonType_Array, &jsonD) != JsonError_None)
		    {
			    const LDtkError error = { LDtkErrorCode_UnnameError, "" };
			    return error;
		    }

		    Json jsonCoordId = jsonD.array[coordIdIndex];
		    if (jsonCoordId.type != JsonType_Number)
		    {
			    const LDtkError error = { LDtkErrorCode_UnnameError, "" };
			    return error;
		    }
		    int32_t coordId = (int32_t)jsonCoordId.number;

		    Json jsonPx;
		    if (JsonFindWithType(jsonTile, "px", JsonType_Array, &jsonPx) != JsonError_None)
		    {
			    const LDtkError error = { LDtkErrorCode_UnnameError, "" };
			    return error;
		    }

            // Parse fields to create LDtkTile

		    Json jsonX = jsonPx.array[0];
		    Json jsonY = jsonPx.array[1];
		    if (jsonX.type != JsonType_Number || jsonY.type != JsonType_Number)
		    {
			    const LDtkError error = { LDtkErrorCode_UnnameError, "" };
			    return error;
		    }
		    int32_t x = (int32_t)jsonX.number;
		    int32_t y = (int32_t)jsonY.number;
		    int32_t worldX = level->worldX + x;
		    int32_t worldY = level->worldY + y;

		    Json jsonSrc;
		    if (JsonFindWithType(jsonTile, "src", JsonType_Array, &jsonSrc) != JsonError_None)
		    {
			    const LDtkError error = { LDtkErrorCode_UnnameError, "" };
			    return error;
		    }

		    Json jsonTextureX = jsonSrc.array[0];
		    Json jsonTextureY = jsonSrc.array[1];
		    if (jsonTextureX.type != JsonType_Number || jsonTextureY.type != JsonType_Number)
		    {
			    const LDtkError error = { LDtkErrorCode_UnnameError, "" };
			    return error;
		    }
		    int32_t textureX = (int32_t)jsonTextureX.number;
		    int32_t textureY = (int32_t)jsonTextureY.number;

		    Json jsonF;
		    if (JsonFindWithType(jsonTile, "f", JsonType_Number, &jsonF) != JsonError_None)
		    {
			    const LDtkError error = { LDtkErrorCode_UnnameError, "" };
			    return error;
		    }
		    uint32_t flip = (uint32_t)jsonF.number;
		    bool flipX = (flip  & 1u);
		    bool flipY = (flip >> 1u) & 1u;

		    const LDtkTile tile = {
			    .id         = tileId,
			    .coordId    = coordId,
			    .x          = x,
			    .y          = y,
			    .worldX     = worldX,
			    .worldY     = worldY,
			    .textureX   = textureX,
			    .textureY   = textureY,
			    .flipX      = flipX,
			    .flipY      = flipY
		    };
		    tiles[i] = tile;
	    }
	    layer->tileCount = tileCount;
	    layer->tiles = tiles;
	}
    else
    {
        layer->tileCount = 0;
        layer->tiles = NULL;
    }

    // Read IntGrid

    LDtkLayerDef layerDef;
    for (int32_t i = 0; i < world->layerDefCount; i++)
    {
        if (world->layerDefs[i].id == layer->layerDefId)
        {
            layerDef = world->layerDefs[i];
            break;
        }
    }

	Json jsonIntGrid;
	if (JsonFindWithType(json, "intGridCsv", JsonType_Array, &jsonIntGrid) == JsonError_None)
	{
        int32_t intGridCount = jsonIntGrid.length;
        LDtkIntGridValue* intGridValues = (LDtkIntGridValue*)AllocLower(allocator, NULL, 0, intGridCount * sizeof(LDtkIntGridValue));
        for (int32_t i = 0; i < intGridCount; i++)
        {
            const Json jsonIntGridValueIndex = jsonIntGrid.array[i];
            if (jsonIntGridValueIndex.type != JsonType_Number)
            {
                const LDtkError error = { LDtkErrorCode_UnnameError, "" };
                return error;
            }

            intGridValues[i] = layerDef.intGridValues[(int32_t)jsonIntGridValueIndex.number - 1];
        }
        layer->valueCount = intGridCount;
        layer->values = intGridValues;
	}
    else if (JsonFindWithType(json, "intGrid", JsonType_Array, &jsonIntGrid) == JsonError_None)
    {
        int32_t intGridCount = jsonIntGrid.length;
        LDtkIntGridValue* intGridValues = (LDtkIntGridValue*)AllocLower(allocator, NULL, 0, intGridCount * sizeof(LDtkIntGridValue));
        for (int32_t i = 0; i < intGridCount; i++)
        {
            const Json jsonValuePair = jsonIntGrid.array[i];

            Json jsonCoordId, jsonV;
            if (JsonFindWithType(jsonValuePair, "coordId", JsonType_Number, &jsonCoordId) != JsonError_None
               || JsonFindWithType(jsonValuePair, "v", JsonType_Number, &jsonV) != JsonError_None)
            {
                const LDtkError error = { LDtkErrorCode_UnnameError, "" };
                return error;
            }

            intGridValues[(int32_t)jsonCoordId.number] = layerDef.intGridValues[(int32_t)jsonV.number];
        }
        layer->valueCount = intGridCount;
        layer->values = intGridValues;
    }
    else
    {
        layer->valueCount = 0;
        layer->values = NULL;
    }

    // Read Entities

	Json jsonEntityInstances;
	if (JsonFindWithType(json, "entityInstances", JsonType_Array, &jsonEntityInstances) == JsonError_None)
	{
        int32_t entityCount = jsonEntityInstances.length;
        LDtkEntity* entities = (LDtkEntity*)AllocLower(allocator, NULL, 0, entityCount * sizeof(*entities));
        for (int32_t i = 0; i < entityCount; i++)
        {
            Json jsonEntity = jsonEntityInstances.array[i];
            if (jsonEntity.type != JsonType_Object)
            {
                const LDtkError error = { LDtkErrorCode_UnnameError, "" };
                return error;
            }

            LDtkEntity* entity = &entities[i];

            Json jsonIdentifier;
            if (JsonFindWithType(jsonEntity, "__identifier", JsonType_String, &jsonIdentifier) != JsonError_None)
            {
                const LDtkError error = { LDtkErrorCode_UnnameError, "" };
                return error;
            }
            entity->name = jsonIdentifier.string;

            Json jsonDefUid;
            if (JsonFindWithType(jsonEntity, "defUid", JsonType_Number, &jsonDefUid) != JsonError_None)
            {
                const LDtkError error = { LDtkErrorCode_UnnameError, "" };
                return error;
            }
            entity->defId = (int32_t)jsonDefUid.number;

            Json jsonWidth;
            if (JsonFindWithType(jsonEntity, "width", JsonType_Number, &jsonWidth) != JsonError_None)
            {
                const LDtkError error = { LDtkErrorCode_UnnameError, "" };
                return error;
            }
            entity->width = (int32_t)jsonWidth.number;

            Json jsonHeight;
            if (JsonFindWithType(jsonEntity, "height", JsonType_Number, &jsonHeight) != JsonError_None)
            {
                const LDtkError error = { LDtkErrorCode_UnnameError, "" };
                return error;
            }
            entity->height = (int32_t)jsonHeight.number;

            Json jsonPx;
            if (JsonFindWithType(jsonEntity, "px", JsonType_Array, &jsonPx) != JsonError_None)
            {
                const LDtkError error = { LDtkErrorCode_UnnameError, "" };
                return error;
            }

            Json jsonX = jsonPx.array[0];
            Json jsonY = jsonPx.array[1];
            if (jsonX.type != JsonType_Number || jsonY.type != JsonType_Number)
            {
                const LDtkError error = { LDtkErrorCode_UnnameError, "" };
                return error;
            }
            entity->x = (int32_t)jsonX.number;
            entity->y = (int32_t)jsonX.number;
            entity->worldX = level->worldX + entity->x;
            entity->worldY = level->worldY + entity->y;

            Json jsonGrid;
            if (JsonFindWithType(jsonEntity, "__grid", JsonType_Array, &jsonGrid) != JsonError_None)
            {
                const LDtkError error = { LDtkErrorCode_UnnameError, "" };
                return error;
            }

            Json jsonGridX = jsonGrid.array[0];
            Json jsonGridY = jsonGrid.array[1];
            if (jsonGridX.type != JsonType_Number || jsonGridY.type != JsonType_Number)
            {
                const LDtkError error = { LDtkErrorCode_UnnameError, "" };
                return error;
            }
            entity->gridX = (int32_t)jsonGridX.number;
            entity->gridY = (int32_t)jsonGridX.number;

            Json jsonPivot;
            if (JsonFindWithType(jsonEntity, "__pivot", JsonType_Array, &jsonPivot) != JsonError_None)
            {
                const LDtkError error = { LDtkErrorCode_UnnameError, "" };
                return error;
            }

            Json jsonPivotX = jsonPivot.array[0];
            Json jsonPivotY = jsonPivot.array[1];
            if (jsonPivotX.type != JsonType_Number || jsonPivotY.type != JsonType_Number)
            {
                const LDtkError error = { LDtkErrorCode_UnnameError, "" };
                return error;
            }
            entity->pivotX = (int32_t)jsonPivotX.number;
            entity->pivotY = (int32_t)jsonPivotX.number;
        }

        layer->entityCount = entityCount;
        layer->entities = entities;
	}
    else
    {
        layer->entityCount = 0;
        layer->entities = NULL;
    }

	const LDtkError error = { LDtkErrorCode_None, "" };
	return error;
}

static LDtkError LDtkReadLevel(const Json json, const char* levelDirectory, Allocator* allocator, LDtkReadFileFn* readFileFn, LDtkLevel* level, LDtkParseFlags flags, LDtkWorld* world)
{
    const Json jsonUid;
    JsonFind(json, "uid", (Json*)&jsonUid);
    level->id = (int32_t)jsonUid.number;

    const Json jsonIdentifier;
    JsonFind(json, "identifier", (Json*)&jsonIdentifier);
    level->name = jsonIdentifier.string;

    const Json jsonWorldX;
    JsonFind(json, "worldX", (Json*)&jsonWorldX);
    level->worldX = (int32_t)jsonWorldX.number;

    const Json jsonWorldY;
    JsonFind(json, "worldY", (Json*)&jsonWorldY);
    level->worldY = (int32_t)jsonWorldY.number;

    const Json jsonPxWid;
    JsonFind(json, "pxWid", (Json*)&jsonPxWid);
    level->width = (int32_t)jsonPxWid.number;

    const Json jsonPxHei;
    JsonFind(json, "pxHei", (Json*)&jsonPxHei);
    level->height = (int32_t)jsonPxHei.number;

    // Reading background fields

    const Json jsonColor;
    JsonFind(json, "__bgColor", (Json*)&jsonColor);
    level->bgColor = LDtkColorFromString(jsonColor.string);

    const Json jsonBgRelPath;
    JsonFind(json, "bgRelPath", (Json*)&jsonBgRelPath);
    level->bgPath = jsonBgRelPath.string;

    const Json jsonBgPivotX;
    JsonFind(json, "bgPivotX", (Json*)&jsonBgPivotX);
    level->bgPivotX = (int32_t)jsonBgPivotX.number;

    const Json jsonBgPivotY;
    JsonFind(json, "bgPivotY", (Json*)&jsonBgPivotY);
    level->bgPivotY = (int32_t)jsonBgPivotY.number;

    const Json jsonBgPosMeta;
    if (JsonFind(json, "__bgPos", (Json*)&jsonBgPosMeta))
    {
        const Json jsonTopLeftPx;
        JsonFind(jsonBgPosMeta, "topLeftPx", (Json*)&jsonTopLeftPx);
        level->bgPosX = (int32_t)jsonTopLeftPx.array[0].number;
        level->bgPosY = (int32_t)jsonTopLeftPx.array[1].number;

        const Json jsonScale;
        JsonFind(jsonBgPosMeta, "scale", (Json*)&jsonScale);
        level->bgScaleX = (float)jsonScale.array[0].number;
        level->bgScaleY = (float)jsonScale.array[1].number;

        const Json jsonCropRect;
        JsonFind(jsonBgPosMeta, "cropRect", (Json*)&jsonCropRect);
        level->bgCropX      = (float)jsonCropRect.array[0].number;
        level->bgCropY      = (float)jsonCropRect.array[1].number;
        level->bgCropWidth  = (float)jsonCropRect.array[2].number;
        level->bgCropHeight = (float)jsonCropRect.array[3].number;
    }

    // Reading neighbours

    memset(&level->neigbourCount, 0, sizeof(level->neigbourCount));
    memset(&level->neigbourIds, 0, sizeof(level->neigbourIds));

    const Json jsonNeighbours;
    if (JsonFind(json, "__neighbours", (Json*)&jsonNeighbours))
    {
        for (int32_t i = 0; i < jsonNeighbours.length; i++)
        {
            const Json jsonNeighbour = jsonNeighbours.array[i];

            const Json jsonDir;
            if (!JsonFind(jsonNeighbour, "dir", (Json*)&jsonDir) || jsonDir.type != JsonType_String)
            {
                const LDtkError error = { LDtkErrorCode_MissingWorldProperties, "'neighbour.dir' is missing" };
                return error;
            }

            const char* directionName = jsonDir.string;

            LDtkDirection direction;
            if (strcmp(directionName, "e") == 0)
            {
                direction = LDtkDirection_East;
            }
            else if (strcmp(directionName, "w") == 0)
            {
                direction = LDtkDirection_West;
            }
            else if (strcmp(directionName, "s") == 0)
            {
                direction = LDtkDirection_South;
            }
            else if (strcmp(directionName, "n") == 0)
            {
                direction = LDtkDirection_North;
            }
            else
            {
                const LDtkError error = { LDtkErrorCode_InvalidWorldProperties, "'neighbour.dir' value is unknown" };
                return error;
            }

            const Json jsonLevelUid;
            if (!JsonFind(jsonNeighbour, "levelUid", (Json*)&jsonLevelUid))
            {
                const LDtkError error = { LDtkErrorCode_InvalidWorldProperties, "'neighbour.layerUid' value is unknown" };
                return error;
            }

            level->neigbourIds[(int32_t)direction][level->neigbourCount[(int32_t)direction]++] = (int32_t)jsonLevelUid.number;
        }
    }

    // Reading field instances
    const Json jsonFieldInstances;
    if (!JsonFind(json, "fieldInstances", (Json*)&jsonFieldInstances))
    {
        const LDtkError error = { LDtkErrorCode_InvalidWorldProperties, "'fieldInstances' is missing" };
        return error;
    }

    const int32_t fieldCount = jsonFieldInstances.length;
    for (int32_t i = 0; i < fieldCount; i++)
    {
        
    }

    // Reading layer instances
    Json jsonLayerInstances;
    if (!JsonFind(json, "layerInstances", &jsonLayerInstances))
    {
        const LDtkError error = { LDtkErrorCode_InvalidWorldProperties, "'layerInstances' is missing" };
        return error;
    }

	// Something unclear here
	// Why we donot have a field to specify that
	// Layer instances are define in other files
	if (jsonLayerInstances.type == JsonType_Null)
	{
		const Json jsonExternalRelPath;
		if (!JsonFind(json, "externalRelPath", (Json*)&jsonExternalRelPath))
		{
			const LDtkError error = { LDtkErrorCode_MissingLayerDefProperties, "'externalRelPath' is missing" };
			return error;
		}

		if (jsonExternalRelPath.type != JsonType_String)
		{
			const LDtkError error = { LDtkErrorCode_InvalidLayerDefProperties, "'externalRelPath' must be string" };
			return error;
		}

		char filePath[1024];
		sprintf(filePath, "%s/%s", levelDirectory, jsonExternalRelPath.string);
		
		int32_t fileSize;
		if (!readFileFn(filePath, NULL, &fileSize))
		{
			const LDtkError error = { LDtkErrorCode_MissingLevelExternalFile, "cannot read file from ..." };
			return error;
		}

		void* buffer = AllocUpper(allocator, NULL, 0, fileSize + 1);
		if (!buffer)
		{
			const LDtkError error = { LDtkErrorCode_OutOfMemory, "Cannot read more memory" };
			return error;
		}

		if (!readFileFn(filePath, buffer, &fileSize))
		{
			const LDtkError error = { LDtkErrorCode_InternalError, "Cannot read more memory" };
			return error;
		}

		int32_t contentLength = fileSize;
		char* content = (char*)buffer;
		content[contentLength] = 0;

		Json jsonLevelFile;
		const JsonResult result = JsonParse(content, contentLength, JsonParseFlags_Default, allocator->lowerMarker, AllocatorRemainSize(allocator), &jsonLevelFile);
		if (result.error != JsonError_None)
		{
			const LDtkError error = { LDtkErrorCode_InternalError, "Cannot read more memory" };
			return error;
		}
		
		// Move forward
		AllocLower(allocator, NULL, 0, result.memoryUsage);

		// File layer instances
		if (!JsonFind(jsonLevelFile, "layerInstances", &jsonLayerInstances))
		{
			const LDtkError error = { LDtkErrorCode_InvalidWorldProperties, "'layerInstances' is missing" };
			return error;
		}
	}

	const int32_t layerCount = jsonLayerInstances.length;

	level->layerCount = 0;
	level->layers = (LDtkLayer*)AllocLower(allocator, NULL, 0, layerCount * sizeof(LDtkLayer));

    for (int32_t i = 0; i < layerCount; i++)
    {
		const Json layerJson = jsonLayerInstances.array[i];
		const LDtkError error = LDtkReadLayer(layerJson, allocator, level, world);
        if (error.code != LDtkErrorCode_None)
        {
            return error;
        }
    }

    if (flags & LDtkParseFlags_LayerReverseOrder)
    {
        for (int32_t i = 0, n = layerCount >> 1; i < n; i++)
        {
            const LDtkLayer tmp = level->layers[i];
            level->layers[i] = level->layers[layerCount - i - 1];
            level->layers[layerCount - i - 1] = tmp;
        }
    }

    const LDtkError error = { LDtkErrorCode_None, "" };
    return error;
}

static LDtkError LDtkReadLevels(const Json json, const char* ldtkPath, Allocator* allocator, LDtkReadFileFn* readFileFn, LDtkParseFlags flags, LDtkWorld* world)
{
    const Json jsonLevels;
    if (!JsonFind(json, "levels", (Json*)&jsonLevels))
    {
        const LDtkError error = { LDtkErrorCode_MissingWorldProperties, "'levels' is not found" };
        return error;
    }

	char levelDirectory[1024];
	int32_t levelDirectoryLength = strlen(ldtkPath);
	while (ldtkPath[levelDirectoryLength] != '/' && levelDirectoryLength > 0) { levelDirectoryLength--; };
	memcpy(levelDirectory, ldtkPath, levelDirectoryLength);
	levelDirectory[levelDirectoryLength] = 0;

    const int32_t   levelCount = jsonLevels.length;
    LDtkLevel*      levels = (LDtkLevel*)AllocLower(allocator, NULL, 0, sizeof(LDtkLevel) * levelCount);
    for (int32_t i = 0; i < levelCount; i++)
    {
        LDtkLevel* level        = &levels[i];
        const Json jsonLevel    = jsonLevels.array[i];

        const LDtkError readError = LDtkReadLevel(jsonLevel, levelDirectory, allocator, readFileFn, level, flags, world);
        if (readError.code != LDtkErrorCode_None)
        {
            return readError;
        }
    }

    world->levelCount = levelCount;
    world->levels = levels;

    const LDtkError error = { LDtkErrorCode_None, "" };
    return error;
}

LDtkError LDtkParse(const char* ldtkPath, LDtkContext context, LDtkParseFlags flags, LDtkWorld* world)
{
	LDtkReadFileFn* readFileFn = context.readFileFn;

	char* content = (char*)context.buffer;
	int32_t contentLength = context.bufferSize;
	if (!readFileFn(ldtkPath, content, &contentLength))
	{
		const LDtkError error = { LDtkErrorCode_ParseJsonFailed, "" };
		return error;
	}
	content[contentLength] = 0;

	void* buffer = content + contentLength + 1;
	int32_t bufferSize = context.bufferSize - contentLength - 1;

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

    const LDtkError readLevelsError = LDtkReadLevels(json, ldtkPath, &allocator, readFileFn, flags, world);
    if (readLevelsError.code != LDtkErrorCode_None)
    {
        return readLevelsError;
    }

    const LDtkError error = { LDtkErrorCode_None, "" };
    return error;
}

LDtkContext LDtkContextDefault(void* buffer, int32_t bufferSize)
{
#if defined(__GNUC__)
	return LDtkContextLinux(buffer, bufferSize);
#elif defined(_WIN32)
	return LDtkContextWindows(buffer, bufferSize);
#else
	return LDtkContextStdC(buffer, bufferSize);
#endif
}

static bool LDtkReadFileStdC(const char* fileName, void* buffer, int32_t* bufferSize)
{
	FILE* file = fopen(fileName, "r");
	if (!file)
	{
		return false;
	}

	fseek(file, 0, SEEK_END);
	size_t fileSize = (size_t)ftell(file);
	fseek(file, 0, SEEK_SET);

	if (buffer)
	{
		if (*bufferSize < (int32_t)fileSize)
		{
			fclose(file);
			return false;
		}

		fileSize = fread(buffer, 1, fileSize, file);
	}
	
	*bufferSize = (int32_t)fileSize;

	fclose(file);
	return true;
}

LDtkContext LDtkContextStdC(void* buffer, int32_t bufferSize)
{
	LDtkContext result = {
		.buffer = buffer,
		.bufferSize = bufferSize,
		.readFileFn = LDtkReadFileStdC
	};

	return result;
}

#if defined(__GNUC__)
#include <fcntl.h>
#include <unistd.h>

static bool LDtkReadFileLinux(const char* fileName, void* buffer, int32_t* bufferSize)
{
	bool result = false;

	int file = open(fileName, O_RDONLY);
	if (file == -1)
	{
		return false;
	}

	lseek(file, 0, SEEK_END);
	off_t fileSize = tell(file);
	lseek(file, 0, SEEK_SET);

	if (buffer)
	{
		if (*bufferSize < (int32_t)fileSize)
		{
			close(file);
			return false;
		}

		ssize_t readBytes = read(file, buffer, (size_t)fileSize);
		result = (readBytes != -1);
	}

	*bufferSize = (int32_t)fileSize;

	close(file);
	return result;
}

LDtkContext LDtkContextLinux(void* buffer, int32_t bufferSize)
{
	LDtkContext result = {
		.buffer = buffer,
		.bufferSize = bufferSize,
		.readFileFn = LDtkReadFileLinux
	};

	return result;
}
#elif defined(_WIN32)
#define VC_EXTRA_CLEAN
#define WIN32_CLEAN_AND_MEAN
#include <Windows.h>

static bool LDtkReadFileWindows(const char* fileName, void* buffer, int32_t* bufferSize)
{
	//TCHAR tFileName[MAX_PATH];
	//WideCharToMultiByte(CP_UTF8, 0, tFileName, sizeof(tFileName), fileName, 0, NULL, NULL);

	HANDLE file = CreateFileA(fileName, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (file == INVALID_HANDLE_VALUE)
	{
		return false;
	}

	DWORD fileSize = GetFileSize(file, NULL);
	if (buffer)
	{
		if (*bufferSize < fileSize)
		{
			CloseHandle(file);
			return false;
		}

		ReadFile(file, buffer, fileSize, NULL, NULL);
	}

	*bufferSize = (int32_t)fileSize;

	CloseHandle(file);
	return true;
}

LDtkContext LDtkContextWindows(void* buffer, int32_t bufferSize)
{
	LDtkContext result = {
		.buffer = buffer,
		.bufferSize = bufferSize,
		.readFileFn = LDtkReadFileWindows
	};

	return result;
}
#endif

//! LEAVE AN EMPTY LINE HERE, REQUIRE BY GCC/G++
