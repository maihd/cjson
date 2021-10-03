#pragma once

#include <stdint.h>

#ifndef __cplusplus
#include <stdbool.h>
#endif

typedef enum LDtkDirection
{
    LDtkDirection_None,
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

typedef struct LDtkTileset
{
    const char* name;   // Name of tileset
    const char* path;   // Relative path to image file
} LDtkTileset;

typedef struct LDtkLayer
{
    const char* name;

    int32_t     order;

    LDtkTileset tileset;
} LDtkLayer;

typedef struct LDtkIntGrid
{
    const char* name;

    int32_t     order;

    int32_t     cols;
    int32_t     rows;
    int32_t     data[];
} LDtkIntGrid;

typedef struct LDtkEntityGroup
{
    const char* name;

    int32_t     order;

    int32_t     entityCount;
    LDtkEntity* entities;
} LDtkEntityGroup;

typedef struct LDtkLevel
{
    const char*         name;

    int32_t             layerCount;
    LDtkLayer*          layers;

    int32_t             gridCount;
    LDtkIntGrid*        grids;

    int32_t             entityGroupCount;
    LDtkEntityGroup*    entityGroups;
} LDtkLevel;

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

    int32_t         layerCount;
    LDtkLayer*      layers;

    int32_t         enityCount;
    LDtkEntity*     entities;

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
} LDtkErrorCode;

typedef struct LDtkError
{
    LDtkErrorCode   code;
    const char*     message;
} LDtkError;

LDtkError LDtkParse(const char* json, int32_t jsonLength, void* buffer, int32_t bufferSize, LDtkWorld* world);
