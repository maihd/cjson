#pragma once

#include <stdint.h>

#ifndef __cplusplus
#include <stdbool.h>
#endif

typedef enum LDtkLayerType
{
    LDtkLayerType_IntGrid,
    LDtkLayerType_Entities,
    LDtkLayerType_Tiles,
    LDtkLayerType_AutoLayer
} LDtkLayerType;

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
    LDtkColor   color;
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
    const char* name;
    const char* path;
} LDtkTileset;

typedef struct LDtkLayer
{
    LDtkTileset tileset;
} LDtkLayer;

typedef struct LDtkIntGrid
{
    int cols;
    int rows;
    int data[];
} LDtkIntGrid;

typedef struct LDtkWorld
{
    LDtkTileset*    tilesets;
    LDtkLayer*      layers;
    LDtkIntGrid*    grids;
    LDtkEntity*     entities;
} LDtkWorld;

typedef enum LDtkErrorCode
{
    LDtkErrorCode_None,
    LDtkErrorCode_ParseJsonFailed,
} LDtkErrorCode;

typedef struct LDtkError
{
    LDtkErrorCode   code;
    const char*     message;
} LDtkError;

LDtkError LDtkParse(const char* json, int32_t jsonLength, void* buffer, int32_t bufferSize, LDtkWorld* world);
