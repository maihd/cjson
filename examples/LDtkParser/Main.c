#define _CRT_SECURE_NO_WARNINGS

#include "raylib.h"
#include "LDtkParser.h"

#include <stdio.h>
#include <stdlib.h>

#if defined(_MSC_VER)
#pragma comment(lib, "winmm.lib")
#endif

#define LDTK_ASSETS_PATH "../../examples/LDtkParser/assets/"

int main(void)
{
    InitWindow(800, 600, "LDtk parser example");

    SetTargetFPS(60);

	const char* ldtkPath = LDTK_ASSETS_PATH"sample.ldtk";

    int32_t tempBufferSize = 20 * 1024 * 1024;
    void* tempBuffer = malloc((size_t)tempBufferSize);

	LDtkContext context = LDtkContextDefault(tempBuffer, tempBufferSize);

    LDtkWorld world;
    LDtkError error = LDtkParse(ldtkPath, context, LDtkParseFlags_LayerReverseOrder, &world);
    if (error.code != LDtkErrorCode_None)
    {
        fprintf(stderr, "Parse ldtk sample content failed!: %s\n", error.message);
        return -1;
    }

    Texture texture = LoadTexture(LDTK_ASSETS_PATH"N2D - SpaceWallpaper1280x448.png");
    SetWindowSize(texture.width * 2, texture.height * 2);

    Texture levelBgTextures[32];
    for (int32_t i = 0; i < world.levelCount; i++)
    {
        char texturePath[2048];
        sprintf(texturePath, LDTK_ASSETS_PATH"%s", world.levels[i].bgPath);
        //printf("texturePath: %s\n", texturePath);
        levelBgTextures[i] = LoadTexture(texturePath);
    }

	Texture tilesetTextures[32];
	for (int32_t i = 0; i < world.tilesetCount; i++)
	{
		char texturePath[2048];
		sprintf(texturePath, LDTK_ASSETS_PATH"%s", world.tilesets[i].path);
		//printf("texturePath: %s\n", texturePath);
		tilesetTextures[i] = LoadTexture(texturePath);
	}

	int32_t currentLevelIndex = 0;
	LDtkLevel currentLevel = world.levels[currentLevelIndex];

    //SetWindowSize(currentLevel.width, currentLevel.height);

    while (!WindowShouldClose())
    {
        BeginDrawing();
        {
            ClearBackground((Color){ world.backgroundColor.r, world.backgroundColor.g, world.backgroundColor.b, world.backgroundColor.a });

            DrawTexturePro(
                levelBgTextures[currentLevelIndex], 
                (Rectangle){ currentLevel.bgCropX, currentLevel.bgCropY, currentLevel.bgCropWidth, currentLevel.bgCropHeight },
                (Rectangle){ currentLevel.bgPosX, currentLevel.bgPosY, currentLevel.bgScaleX * currentLevel.bgCropWidth, currentLevel.bgScaleY * currentLevel.bgCropHeight },
                (Vector2){ currentLevel.bgPivotX, currentLevel.bgPivotY },
                0.0f,
                WHITE);

			for (int32_t i = 0; i < currentLevel.layerCount; i++)
			{
				LDtkLayer layer = currentLevel.layers[i];
                if (!layer.visible)
                {
                    //continue;
                }

				if (layer.tileCount > 0)
				{
					LDtkTileset tileset = layer.tileset;
					Texture tilesetTexture = tilesetTextures[tileset.index];

					for (int32_t tileIndex = 0; tileIndex < layer.tileCount; tileIndex++)
					{
						LDtkTile tile = layer.tiles[tileIndex];
                        
                        const float scaleX = tile.flipX ? -1.0f : 1.0f;
                        const float scaleY = tile.flipY ? -1.0f : 1.0f;

						DrawTexturePro(
							tilesetTexture,
							(Rectangle) { tile.textureX, tile.textureY, tileset.tileSize, tileset.tileSize },
							(Rectangle) { tile.x, tile.y, tileset.tileSize * scaleX, tileset.tileSize * scaleY },
							(Vector2) { 0.0f, 0.0f },
                            0.0f,
							WHITE
						);
					}
				}

				if (layer.entityCount > 0)
				{
					for (int32_t entityIndex = 0; entityIndex < layer.entityCount; entityIndex++)
					{
						LDtkEntity entity = layer.entities[entityIndex];
						//DrawTexture(entityTextures[entit])
					}
				}
			}
        }
        EndDrawing();
    }

    free(tempBuffer);

    CloseWindow();
    return 0;
}