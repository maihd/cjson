#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C"
{
#endif

typedef enum LDtkDirection
{
    LDtkDirection_North,
    LDtkDirection_East,
    LDtkDirection_South,
    LDtkDirection_West
} LDtkDirection;

typedef enum LDtkWorldLayout
{
    LDtkWorldLayout_Free,
    LDtkWorldLayout_GridVania,
    LDtkWorldLayout_LinearHorizontal,
    LDtkWorldLayout_LinearVertical
} LDtkWorldLayout;

typedef struct LDtkColor
{
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t a;
} LDtkColor;

typedef struct LDtkEnumValue
{
    const char*     name;
    LDtkColor       color;
    int32_t         tileId;
} LDtkEnumValue;

typedef struct LDtkEnum
{
    int32_t         id;
    const char*     name;
    int32_t         tilesetId;

    const char*     externalPath;
    const char*     externalChecksum;

    int32_t         valueCount;
    LDtkEnumValue*  values;
} LDtkEnum;

typedef struct LDtkEntity
{
    const char* name;
} LDtkEntity;

typedef struct LDtkTile
{
    int32_t     id;
    int32_t     coordId;

    int32_t     x;
    int32_t     y;

    int32_t     worldX;
    int32_t     worldY;
    
    int32_t     textureX;
    int32_t     textureY;
    
    bool        flipX;
    bool        flipY;
} LDtkTile;

typedef struct LDtkIntGridValue
{
    const char* name;
    int32_t     value;
    LDtkColor   color;
} LDtkIntGridValue;

typedef struct LDtkTileset
{
    int32_t     id;

    const char* name;   // Name of tileset
    const char* path;   // Relative path to image file

    int32_t     width;
    int32_t     height;

    int32_t     tileSize;
    int32_t     spacing;
    int32_t     padding;

    int32_t     tagsEnumId;
} LDtkTileset;

typedef struct LDtkTileLayer
{
    int32_t     id;
    const char* name;

    int32_t     cols;
    int32_t     rows;
    int32_t     tileSize;

    int32_t     offsetX;
    int32_t     offsetY;

    float       tilePivotX;
    float       tilePivotY;

    int32_t     order;
    int32_t     visible : 1;
    int32_t     opacity : 31;

    LDtkTileset tileset;
    
    int32_t     tileCount;
    LDtkTile*   tiles;
} LDtkTileLayer;

typedef struct LDtkIntGridLayer
{
    int32_t             id;
    const char*         name;

    int32_t             cols;
    int32_t             rows;
    int32_t             cellSize;

    int32_t             offsetX;
    int32_t             offsetY;

    float               tilePivotX;
    float               tilePivotY;

    int32_t             order;
    int32_t             visible : 1;
    int32_t             opacity : 31;

    int32_t             valueCount;
    LDtkIntGridValue    values[];
} LDtkIntGridLayer;

typedef struct LDtkEntityLayer
{
    int32_t     id;
    const char* name;

    int32_t     cols;
    int32_t     rows;
    int32_t     cellSize;

    int32_t     offsetX;
    int32_t     offsetY;

    float       tilePivotX;
    float       tilePivotY;

    int32_t     order;
    int32_t     visible : 1;
    int32_t     opacity : 31;

    int32_t     entityCount;
    LDtkEntity* entities;
} LDtkEntityLayer;

typedef struct LDtkLevel
{
    int32_t             id;
    const char*         name;

    int32_t             width;
    int32_t             height;

    int32_t             worldX;
    int32_t             worldY;

    LDtkColor           bgColor;
    const char*         bgPath;
    int32_t             bgPosX;
    int32_t             bgPosY;
    float               bgCropX;
    float               bgCropY;
    float               bgCropWidth;
    float               bgCropHeight;
    float               bgScaleX;
    float               bgScaleY;
    float               bgPivotX;
    float               bgPivotY;

    int32_t             tileLayerCount;
    LDtkTileLayer*      tileLayers;

    int32_t             intGridLayerCount;
    LDtkIntGridLayer*   intGridLayers;

    int32_t             entityLayerCount;
    LDtkEntityLayer*    entityLayers;

    int32_t             neigbourCount[4];
    int32_t             neigbourIds[4][16];
} LDtkLevel;

typedef enum LDtkLayerType
{
    LDtkLayerType_Tiles,
    LDtkLayerType_IntGrid,
    LDtkLayerType_Entities,
    LDtkLayerType_AutoLayer,
} LDtkLayerType;

typedef struct LDtkLayerDef
{
    int32_t             id;
    const char*         name;

    LDtkLayerType       type;

    int32_t             gridSize;
    float               opacity;

    int32_t             offsetX;
    int32_t             offsetY;

    float               tilePivotX;
    float               tilePivotY;
    int32_t             tilesetDefId;

    int32_t             intGridValueCount;
    LDtkIntGridValue*   intGridValues;
} LDtkLayerDef;

typedef struct LDtkEntityDef
{
    int32_t         id;
    const char*     name;
    
    int32_t         width;
    int32_t         height;

    LDtkColor       color;
    
    float           pivotX;
    float           pivotY;
    
    int32_t         tileId;
    int32_t         tilesetId;

    int32_t         tagCount;
    const char**    tags;
} LDtkEntityDef;

typedef struct LDtkWorld
{
    LDtkWorldLayout layout;
    LDtkColor       backgroundColor;

    float           defaultPivotX;
    float           defaultPivotY;
    int32_t         defaultGridSize;

    int32_t         tilesetCount;
    LDtkTileset*    tilesets;

    int32_t         enumCount;
    LDtkEnum*       enums;

    int32_t         layerDefCount;
    LDtkLayerDef*   layerDefs;

    int32_t         entityDefCount;
    LDtkEntityDef*  entityDefs;

    int32_t         levelCount;
    LDtkLevel*      levels;
} LDtkWorld;

typedef enum LDtkErrorCode
{
    LDtkErrorCode_None,
    LDtkErrorCode_ParseJsonFailed,
    LDtkErrorCode_MissingLevels,

    LDtkErrorCode_MissingWorldProperties,
    LDtkErrorCode_InvalidWorldProperties,

    LDtkErrorCode_MissingLayerDefProperties,
    LDtkErrorCode_InvalidLayerDefProperties,

	LDtkErrorCode_MissingLevelExternalFile,
	LDtkErrorCode_InvalidLevelExternalFile,

    LDtkErrorCode_OutOfMemory,
	LDtkErrorCode_InternalError,
} LDtkErrorCode;

typedef struct LDtkError
{
    LDtkErrorCode   code;
    const char*     message;
} LDtkError;

typedef bool LDtkReadFileFn(const char* fileName, void* buffer, int32_t* bufferSize);

typedef struct LDtkContext
{
	void*			buffer;
	int32_t			bufferSize;

	LDtkReadFileFn*	readFileFn;
} LDtkContext;

LDtkContext		LDtkContextStdC(void* buffer, int32_t bufferSize);
LDtkContext		LDtkContextDefault(void* buffer, int32_t bufferSize);

#if defined(__GNUC__)
LDtkContext		LDtkContextLinux(void* buffer, int32_t bufferSize);
#elif defined(_WIN32)
LDtkContext		LDtkContextWindows(void* buffer, int32_t bufferSize);
#endif

LDtkError		LDtkParse(const char* ldtkPath, LDtkContext context, LDtkWorld* world);

#ifdef __cplusplus
}
#endif

//! LEAVE AN EMPTY LINE HERE, REQUIRE BY GCC/G++
