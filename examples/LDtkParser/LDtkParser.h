#pragma once

typedef enum LDtkLayerType
{

} LDtkLayerType;

typedef struct LDtkEntity
{
    const char* name;
} LDtkEntity;

typedef struct LDtkTile
{
    int x;
    int y;
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

