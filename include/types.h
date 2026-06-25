#ifndef MC_TYPES_H
#define MC_TYPES_H

typedef struct Vector2Int {
    int x;
    int z;
} Vector2Int;

typedef struct Vector3Int {
    int x;
    int y;
    int z;
} Vector3Int;

typedef struct BlockInWorld {
    Vector3Int blockCoord;
    Vector2Int chunkCoord;
} BlockInWorld;

#endif
