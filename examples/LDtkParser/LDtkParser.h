#pragma once

#include <stdint.h>

#ifndef __cplusplus
#include <stdbool.h>
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

typedef struct LDtkEnum
{
    const char* name;
    const char* value;
    LDtkColor   color;
    int32_t     index;
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
    const char* name;   // Name of tileset
    const char* path;   // Relative path to image file
} LDtkTileset;

typedef struct LDtkTileLayer
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
    const char*         name;

    const char*         bgImagePath;
    int32_t             bgImageX;
    int32_t             bgImageY;
    int32_t             bgImageCropX;
    int32_t             bgImageCropY;
    float               bgImageScaleX;
    float               bgImageScaleY;

    int32_t             tileLayerCount;
    LDtkTileLayer*      tileLayers;

    int32_t             intGridLayerCount;
    LDtkIntGridLayer*   intGridLayers;

    int32_t             entityLayerCount;
    LDtkEntityLayer*    entityLayers;

    int32_t             neigbourIds;
    struct LDtkLevel*   neigbours;
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
    
    int32_t         rows;
    int32_t         cols;

    LDtkColor       color;
    
    float           pivotX;
    float           pivotY;
    
    int32_t         tileId;
    LDtkTileset*    tileset;

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

    LDtkErrorCode_OutOfMemory
} LDtkErrorCode;

typedef struct LDtkError
{
    LDtkErrorCode   code;
    const char*     message;
} LDtkError;

LDtkError LDtkParse(const char* json, int32_t jsonLength, void* buffer, int32_t bufferSize, LDtkWorld* world);
