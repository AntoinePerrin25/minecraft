#include "data.h"
#include "atlas.h"
#include "raymath.h"
#include "rlgl.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

typedef struct {
    Vector3 normal;     // Normale de la face (vers l'extérieur)
    int offset[3];      // Décalage pour le voisin
    Color color;        // Couleur de la face (peut varier si texture)
} BlockFace;

static const BlockFace blockFaces[6] = {
    {{ 1,  0,  0}, { 1,  0,  0}, {0}}, // Droite
    {{-1,  0,  0}, {-1,  0,  0}, {0}}, // Gauche
    {{ 0,  1,  0}, { 0,  1,  0}, {0}}, // Haut
    {{ 0, -1,  0}, { 0, -1,  0}, {0}}, // Bas
    {{ 0,  0,  1}, { 0,  0,  1}, {0}}, // Avant
    {{ 0,  0, -1}, { 0,  0, -1}, {0}},  // Arrière
};

BlockData createBlock(BlockType type)
{
    BlockData blockData;
    blockData.Type = type;
    switch (type)
    {
    case BLOCK_NONE:
    case BLOCK_AIR:
        blockData.lightLevel = 0;
        blockData.gravity = 0;
        blockData.solid = 0;
        blockData.visible = 0;
        break;

    case BLOCK_BEDROCK:
        blockData.lightLevel = 0;
        blockData.gravity = 0;
        blockData.solid = 1;
        blockData.visible = 1;
        break;

    case BLOCK_DIRT:
    case BLOCK_GRASS:
    case BLOCK_STONE:
    case BLOCK_WOOD:
        blockData.lightLevel = 0;
        blockData.gravity = 0;
        blockData.solid = 1;
        blockData.visible = 1;
        break;

    case BLOCK_WATER:
        blockData.lightLevel = 0;
        blockData.gravity = 0;
        blockData.solid = 0;
        blockData.visible = 1;
        break;

    case BLOCK_SAND:
        blockData.lightLevel = 0;
        blockData.gravity = 1; // sand falls
        blockData.solid = 1;
        blockData.visible = 1;
        break;

    default:
        fprintf(stderr, "createBlock: Unknown block type %d\n", type);
        exit(1);
        break;
    }
    return blockData;
}

void generateChunk(Chunk *chunk, int chunkX, int chunkZ)
{
    chunk->x = chunkX;
    chunk->z = chunkZ;
    // Pour l'instant, générons un terrain plat simple
    for (int x = 0; x < 16; x++)
    {
        for (int z = 0; z < 16; z++)
        {
            for (int y = 0; y < 128; y++)
            {
                if (y > 64)
                {
                    chunk->data.blocks[x][y][z] = createBlock(BLOCK_AIR);
                }
                else if (y == 64)
                {
                    chunk->data.blocks[x][y][z] = createBlock(BLOCK_GRASS);
                }
                else if (y >= 60)
                {
                    chunk->data.blocks[x][y][z] = createBlock(BLOCK_DIRT);
                }
                else if (y >= 4)
                {
                    chunk->data.blocks[x][y][z] = createBlock(BLOCK_STONE);
                }
                else
                {
                    chunk->data.blocks[x][y][z] = createBlock(BLOCK_BEDROCK);
                }
            }
        }
    }
}

// Fonction pour convertir des coordonnées monde en coordonnées de bloc
BlockInWorld worldToBlockCoords(Vector3 worldPos)
{
    BlockInWorld biw;
    biw.blockCoord.x = (int)floorf(worldPos.x) & 15; // Modulo 16 pour obtenir la coordonnée dans le chunk
    biw.blockCoord.y = (int)floorf(worldPos.y);
    biw.blockCoord.z = (int)floorf(worldPos.z) & 15; // Modulo 16 pour obtenir la coordonnée dans le chunk
    biw.chunkCoord.x = (int)floorf(worldPos.x) >> 4; // Division par 16 pour obtenir la coordonnée du chunk
    biw.chunkCoord.z = (int)floorf(worldPos.z) >> 4; // Division par 16 pour obtenir la coordonnée du chunk
    return biw;
}

BlockData getBlockAt(Chunk *chunks, int worldX, int worldY, int worldZ)
{
    // Guard Y bounds
    if (worldY < 0 || worldY >= WORLD_HEIGHT)
    {
        return createBlock(BLOCK_AIR);
    }

    // Calcul correct des coordonnées de chunk (gestion des coordonnées négatives)
    int chunkX = (worldX >= 0) ? (worldX >> 4) : ((worldX + 1) / 16 - 1);
    int chunkZ = (worldZ >= 0) ? (worldZ >> 4) : ((worldZ + 1) / 16 - 1);
    
    // Calcul correct des coordonnées locales (gestion des coordonnées négatives)
    int localX = worldX - (chunkX * 16);
    int localZ = worldZ - (chunkZ * 16);
    
    // Sécurité : vérifier que les coordonnées locales sont dans les limites
    if (localX < 0 || localX >= 16 || localZ < 0 || localZ >= 16)
    {
        return createBlock(BLOCK_AIR);
    }

    // Trouver le chunk correspondant
    for (int i = 0; i < (2 * RENDER_DISTANCE + 1) * (2 * RENDER_DISTANCE + 1); i++)
    {
        if (chunks[i].x == chunkX && chunks[i].z == chunkZ)
        {
            return chunks[i].data.blocks[localX][worldY][localZ];
        }
    }

    // Si le chunk n'est pas trouvé, retourner un bloc AIR
    return createBlock(BLOCK_AIR);
}

// Get neighboring block positions
int isBlockExposed(Chunk *chunks, int x, int y, int z)
{
    // Check all 6 neighboring blocks
    int exposed = 0;
    int offsets[6][3] = {
        {1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}};

    for (int i = 0; i < 6; i++)
    {
        int nx = x + offsets[i][0];
        int ny = y + offsets[i][1];
        int nz = z + offsets[i][2];
        BlockData neighbor = getBlockAt(chunks, nx, ny, nz);
        if (neighbor.Type == BLOCK_AIR || neighbor.Type == BLOCK_NONE)
        {
            exposed = 1;
            break;
        }
    }

    return exposed;
}

static bool IsFaceVisible(Vector3 blockCenter, Vector3 faceNormal, Vector3 cameraPos)
{
    Vector3 toCamera = Vector3Subtract(cameraPos, blockCenter);
    return Vector3DotProduct(faceNormal, toCamera) > 0.0f;
}

static void DrawBlockFace(Vector3 pos, int faceIdx, float size, Color color)
{
    float s = size * 0.5f;
    Vector3 center = { pos.x + 0.5f, pos.y + 0.5f, pos.z + 0.5f };
    Vector3 v[4];

    // L'ordre des vertices doit suivre le sens anti-horaire (CCW) vu de l'extérieur
    // pour que le back-face culling fonctionne correctement
    switch (faceIdx) {
        case 0: // Droite (+X) - normale vers +X
            v[0] = (Vector3){center.x + s, center.y - s, center.z + s};
            v[1] = (Vector3){center.x + s, center.y - s, center.z - s};
            v[2] = (Vector3){center.x + s, center.y + s, center.z - s};
            v[3] = (Vector3){center.x + s, center.y + s, center.z + s};
            break;
        case 1: // Gauche (-X) - normale vers -X
            v[0] = (Vector3){center.x - s, center.y - s, center.z - s};
            v[1] = (Vector3){center.x - s, center.y - s, center.z + s};
            v[2] = (Vector3){center.x - s, center.y + s, center.z + s};
            v[3] = (Vector3){center.x - s, center.y + s, center.z - s};
            break;
        case 2: // Haut (+Y) - normale vers +Y
            v[0] = (Vector3){center.x - s, center.y + s, center.z + s};
            v[1] = (Vector3){center.x + s, center.y + s, center.z + s};
            v[2] = (Vector3){center.x + s, center.y + s, center.z - s};
            v[3] = (Vector3){center.x - s, center.y + s, center.z - s};
            break;
        case 3: // Bas (-Y) - normale vers -Y
            v[0] = (Vector3){center.x - s, center.y - s, center.z - s};
            v[1] = (Vector3){center.x + s, center.y - s, center.z - s};
            v[2] = (Vector3){center.x + s, center.y - s, center.z + s};
            v[3] = (Vector3){center.x - s, center.y - s, center.z + s};
            break;
        case 4: // Avant (+Z) - normale vers +Z
            v[0] = (Vector3){center.x - s, center.y - s, center.z + s};
            v[1] = (Vector3){center.x + s, center.y - s, center.z + s};
            v[2] = (Vector3){center.x + s, center.y + s, center.z + s};
            v[3] = (Vector3){center.x - s, center.y + s, center.z + s};
            break;
        case 5: // Arrière (-Z) - normale vers -Z
            v[0] = (Vector3){center.x + s, center.y - s, center.z - s};
            v[1] = (Vector3){center.x - s, center.y - s, center.z - s};
            v[2] = (Vector3){center.x - s, center.y + s, center.z - s};
            v[3] = (Vector3){center.x + s, center.y + s, center.z - s};
            break;
    }

    // Deux triangles par face avec ordre CCW (Counter-ClockWise)
    DrawTriangle3D(v[0], v[1], v[2], color);
    DrawTriangle3D(v[0], v[2], v[3], color);
}

void DrawChunks(Chunk *chunks, Vector3 cameraDir, Vector3 playerPos)
{
    (void)cameraDir; // Unused parameter - kept for API compatibility
    const float blockSize = 1.0f;
    
    // Activer le culling GPU (back-face culling)
    rlEnableBackfaceCulling();

    for (int i = 0; i < (2 * RENDER_DISTANCE + 1) * (2 * RENDER_DISTANCE + 1); i++)
    {
        Chunk *chunk = &chunks[i];  // Pointeur pour éviter copie

        for (int x = 0; x < CHUNK_SIZE; x++)
        {
            for (int z = 0; z < CHUNK_SIZE; z++)
            {
                for (int y = 0; y < WORLD_HEIGHT; y++)
                {
                    BlockData block = chunk->data.blocks[x][y][z];
                    if (!block.visible || block.Type == BLOCK_AIR || block.Type == BLOCK_NONE)
                        continue;

                    // Position monde du bloc
                    int worldX = chunk->x * CHUNK_SIZE + x;
                    int worldY = y;
                    int worldZ = chunk->z * CHUNK_SIZE + z;
                    
                    Vector3 blockPos = {
                        (float)worldX,
                        (float)worldY,
                        (float)worldZ
                    };

                    // Centre du bloc pour le face culling CPU
                    Vector3 blockCenter = {
                        blockPos.x + 0.5f,
                        blockPos.y + 0.5f,
                        blockPos.z + 0.5f
                    };

                    // Couleur du bloc
                    Color blockColor;
                    switch (block.Type)
                    {
                        case BLOCK_BEDROCK: blockColor = DARKGRAY; break;
                        case BLOCK_DIRT:    blockColor = BROWN;    break;
                        case BLOCK_GRASS:   blockColor = GREEN;    break;
                        case BLOCK_STONE:   blockColor = GRAY;     break;
                        case BLOCK_WATER:   blockColor = BLUE;     break;
                        case BLOCK_SAND:    blockColor = YELLOW;   break;
                        case BLOCK_WOOD:    blockColor = ORANGE;   break;
                        default:            blockColor = WHITE;    break;
                    }

                    // Vérifier chaque face
                    for (int f = 0; f < 6; f++)
                    {
                        int nx = worldX + blockFaces[f].offset[0];
                        int ny = worldY + blockFaces[f].offset[1];
                        int nz = worldZ + blockFaces[f].offset[2];

                        BlockData neighbor = getBlockAt(chunks, nx, ny, nz);

                        // Face exposée ?
                        bool exposed = (neighbor.Type == BLOCK_AIR || neighbor.Type == BLOCK_NONE ||
                                       (neighbor.Type == BLOCK_WATER && block.Type != BLOCK_WATER));

                        if (!exposed) continue;

                        // Face tournée vers la caméra ? (Face culling CPU)
                        if (!IsFaceVisible(blockCenter, blockFaces[f].normal, playerPos))
                            continue;

                        // Option : couleur légèrement différente par face (pour l'ambiance)
                        int hash = (worldX) + (worldY) + (worldZ) % 256;
                        Color faceColor = (Color) {blockColor.r, blockColor.g, blockColor.b, 
                                                  (unsigned char)((blockColor.a * (200 + hash % 56)) / 255)};
                        DrawBlockFace(blockPos, f, blockSize, faceColor);
                    }
                }
            }
        }
    }
    rlDisableBackfaceCulling();
}

// ============================================================================
// SYSTÈME DE MESH OPTIMISÉ POUR MEILLEURE PERFORMANCE
// ============================================================================

// Structure temporaire pour construire le mesh
typedef struct {
    float *vertices;    // positions XYZ
    float *normals;     // normales XYZ
    float *texcoords;   // coordonnées UV (requis par Raylib)
    unsigned char *colors; // couleurs RGBA
    int vertexCount;
    int capacity;
} MeshBuilder;

static void InitMeshBuilder(MeshBuilder *mb, int initialCapacity)
{
    mb->capacity = initialCapacity;
    mb->vertexCount = 0;
    mb->vertices = (float*)malloc(initialCapacity * 3 * sizeof(float));
    mb->normals = (float*)malloc(initialCapacity * 3 * sizeof(float));
    mb->texcoords = (float*)malloc(initialCapacity * 2 * sizeof(float));
    mb->colors = (unsigned char*)malloc(initialCapacity * 4 * sizeof(unsigned char));
}

static void FreeMeshBuilder(MeshBuilder *mb)
{
    free(mb->vertices);
    free(mb->normals);
    free(mb->texcoords);
    free(mb->colors);
}

static void AddFaceToMesh(MeshBuilder *mb, Vector3 pos, int faceIdx, BlockType blockType, Color color)
{
    float s = 0.5f;
    Vector3 center = { pos.x + 0.5f, pos.y + 0.5f, pos.z + 0.5f };
    Vector3 v[4];
    Vector3 normal = blockFaces[faceIdx].normal;

    // Définir les vertices selon la face
    switch (faceIdx) {
        case 0: // Droite (+X)
            v[0] = (Vector3){center.x + s, center.y - s, center.z + s};
            v[1] = (Vector3){center.x + s, center.y - s, center.z - s};
            v[2] = (Vector3){center.x + s, center.y + s, center.z - s};
            v[3] = (Vector3){center.x + s, center.y + s, center.z + s};
            break;
        case 1: // Gauche (-X)
            v[0] = (Vector3){center.x - s, center.y - s, center.z - s};
            v[1] = (Vector3){center.x - s, center.y - s, center.z + s};
            v[2] = (Vector3){center.x - s, center.y + s, center.z + s};
            v[3] = (Vector3){center.x - s, center.y + s, center.z - s};
            break;
        case 2: // Haut (+Y)
            v[0] = (Vector3){center.x - s, center.y + s, center.z + s};
            v[1] = (Vector3){center.x + s, center.y + s, center.z + s};
            v[2] = (Vector3){center.x + s, center.y + s, center.z - s};
            v[3] = (Vector3){center.x - s, center.y + s, center.z - s};
            break;
        case 3: // Bas (-Y)
            v[0] = (Vector3){center.x - s, center.y - s, center.z - s};
            v[1] = (Vector3){center.x + s, center.y - s, center.z - s};
            v[2] = (Vector3){center.x + s, center.y - s, center.z + s};
            v[3] = (Vector3){center.x - s, center.y - s, center.z + s};
            break;
        case 4: // Avant (+Z)
            v[0] = (Vector3){center.x - s, center.y - s, center.z + s};
            v[1] = (Vector3){center.x + s, center.y - s, center.z + s};
            v[2] = (Vector3){center.x + s, center.y + s, center.z + s};
            v[3] = (Vector3){center.x - s, center.y + s, center.z + s};
            break;
        case 5: // Arrière (-Z)
            v[0] = (Vector3){center.x + s, center.y - s, center.z - s};
            v[1] = (Vector3){center.x - s, center.y - s, center.z - s};
            v[2] = (Vector3){center.x - s, center.y + s, center.z - s};
            v[3] = (Vector3){center.x + s, center.y + s, center.z - s};
            break;
    }

    // Agrandir si nécessaire
    if (mb->vertexCount + 6 > mb->capacity) {
        mb->capacity *= 2;
        mb->vertices = (float*)realloc(mb->vertices, mb->capacity * 3 * sizeof(float));
        mb->normals = (float*)realloc(mb->normals, mb->capacity * 3 * sizeof(float));
        mb->texcoords = (float*)realloc(mb->texcoords, mb->capacity * 2 * sizeof(float));
        mb->colors = (unsigned char*)realloc(mb->colors, mb->capacity * 4 * sizeof(unsigned char));
    }

    // Obtenir l'index de texture pour cette face
    int textureIndex = GetBlockFaceTexture(blockType, faceIdx);
    Rectangle uvRect = GetTextureRectFromAtlas(textureIndex);
    
    // Coordonnées UV pour cette texture dans l'atlas
    // uvRect contient déjà les coordonnées normalisées (0.0-1.0)
    float uvCoords[4][2] = {
        {uvRect.x, uvRect.y},                           // Coin haut-gauche
        {uvRect.x + uvRect.width, uvRect.y},            // Coin haut-droit
        {uvRect.x + uvRect.width, uvRect.y + uvRect.height},  // Coin bas-droit
        {uvRect.x, uvRect.y + uvRect.height}            // Coin bas-gauche
    };

    // Premier triangle (v0, v1, v2)
    for (int i = 0; i < 3; i++) {
        Vector3 vtx = (i == 0) ? v[0] : (i == 1) ? v[1] : v[2];
        int uvIdx = (i == 0) ? 0 : (i == 1) ? 1 : 2;
        
        int idx = mb->vertexCount * 3;
        mb->vertices[idx + 0] = vtx.x;
        mb->vertices[idx + 1] = vtx.y;
        mb->vertices[idx + 2] = vtx.z;
        mb->normals[idx + 0] = normal.x;
        mb->normals[idx + 1] = normal.y;
        mb->normals[idx + 2] = normal.z;
        
        int tidx = mb->vertexCount * 2;
        mb->texcoords[tidx + 0] = uvCoords[uvIdx][0];
        mb->texcoords[tidx + 1] = uvCoords[uvIdx][1];
        
        int cidx = mb->vertexCount * 4;
        mb->colors[cidx + 0] = color.r;
        mb->colors[cidx + 1] = color.g;
        mb->colors[cidx + 2] = color.b;
        mb->colors[cidx + 3] = color.a;
        mb->vertexCount++;
    }

    // Deuxième triangle (v0, v2, v3)
    for (int i = 0; i < 3; i++) {
        Vector3 vtx = (i == 0) ? v[0] : (i == 1) ? v[2] : v[3];
        int uvIdx = (i == 0) ? 0 : (i == 1) ? 2 : 3;
        
        int idx = mb->vertexCount * 3;
        mb->vertices[idx + 0] = vtx.x;
        mb->vertices[idx + 1] = vtx.y;
        mb->vertices[idx + 2] = vtx.z;
        mb->normals[idx + 0] = normal.x;
        mb->normals[idx + 1] = normal.y;
        mb->normals[idx + 2] = normal.z;
        
        int tidx = mb->vertexCount * 2;
        mb->texcoords[tidx + 0] = uvCoords[uvIdx][0];
        mb->texcoords[tidx + 1] = uvCoords[uvIdx][1];
        
        int cidx = mb->vertexCount * 4;
        mb->colors[cidx + 0] = color.r;
        mb->colors[cidx + 1] = color.g;
        mb->colors[cidx + 2] = color.b;
        mb->colors[cidx + 3] = color.a;
        mb->vertexCount++;
    }
}

// Générer le mesh pour un chunk
void GenerateChunkMesh(Chunk *chunk, Chunk *chunks, Texture2D atlas)
{
    if (!chunk || !chunks) {
        fprintf(stderr, "ERREUR: GenerateChunkMesh appelé avec chunk ou chunks NULL\n");
        return;
    }
    
    (void)atlas; // Sera utilisé pour appliquer la texture au material

    // Libérer l'ancien mesh si existant
    if (chunk->meshGenerated) {
        UnloadModel(chunk->model);
        // Note: UnloadModel libère aussi le mesh automatiquement
        chunk->meshGenerated = false;
    }

    MeshBuilder mb;
    InitMeshBuilder(&mb, 10000); // Capacité initiale

    // Parcourir tous les blocs du chunk
    for (int x = 0; x < CHUNK_SIZE; x++) {
        for (int z = 0; z < CHUNK_SIZE; z++) {
            for (int y = 0; y < WORLD_HEIGHT; y++) {
                BlockData block = chunk->data.blocks[x][y][z];
                if (!block.visible || block.Type == BLOCK_AIR || block.Type == BLOCK_NONE)
                    continue;

                int worldX = chunk->x * CHUNK_SIZE + x;
                int worldY = y;
                int worldZ = chunk->z * CHUNK_SIZE + z;
                
                Vector3 blockPos = {(float)worldX, (float)worldY, (float)worldZ};

                // Vérifier chaque face
                for (int f = 0; f < 6; f++) {
                    int nx = x + blockFaces[f].offset[0];
                    int ny = y + blockFaces[f].offset[1];
                    int nz = z + blockFaces[f].offset[2];

                    BlockData neighbor;
                    
                    // Vérifier si le voisin est dans le même chunk
                    if (nx >= 0 && nx < CHUNK_SIZE && 
                        ny >= 0 && ny < WORLD_HEIGHT && 
                        nz >= 0 && nz < CHUNK_SIZE) {
                        neighbor = chunk->data.blocks[nx][ny][nz];
                    } else {
                        // Voisin dans un autre chunk ou hors limites
                        int worldNX = worldX + blockFaces[f].offset[0];
                        int worldNY = worldY + blockFaces[f].offset[1];
                        int worldNZ = worldZ + blockFaces[f].offset[2];
                        neighbor = getBlockAt(chunks, worldNX, worldNY, worldNZ);
                    }

                    bool exposed = (neighbor.Type == BLOCK_AIR || neighbor.Type == BLOCK_NONE ||
                                   (neighbor.Type == BLOCK_WATER && block.Type != BLOCK_WATER));

                    if (!exposed) continue;

                    // Les couleurs sont maintenant dans les textures
                    // On utilise WHITE pour ne pas modifier les couleurs de texture
                    Color faceColor = WHITE;

                    AddFaceToMesh(&mb, blockPos, f, block.Type, faceColor);
                }
            }
        }
    }

    // Créer le mesh Raylib
    if (mb.vertexCount > 0) {
        printf("    -> %d vertices générés\n", mb.vertexCount);
        
        // Créer un mesh vide et l'initialiser
        Mesh mesh = {0};
        mesh.vertexCount = mb.vertexCount;
        mesh.triangleCount = mb.vertexCount / 3;
        
        // Allouer de nouveaux buffers (LoadModelFromMesh va les libérer)
        mesh.vertices = (float*)RL_MALLOC(mb.vertexCount * 3 * sizeof(float));
        mesh.normals = (float*)RL_MALLOC(mb.vertexCount * 3 * sizeof(float));
        mesh.texcoords = (float*)RL_MALLOC(mb.vertexCount * 2 * sizeof(float));
        mesh.colors = (unsigned char*)RL_MALLOC(mb.vertexCount * 4 * sizeof(unsigned char));
        
        // Copier les données
        memcpy(mesh.vertices, mb.vertices, mb.vertexCount * 3 * sizeof(float));
        memcpy(mesh.normals, mb.normals, mb.vertexCount * 3 * sizeof(float));
        memcpy(mesh.texcoords, mb.texcoords, mb.vertexCount * 2 * sizeof(float));
        memcpy(mesh.colors, mb.colors, mb.vertexCount * 4 * sizeof(unsigned char));
        
        // Initialiser explicitement les autres champs à NULL
        mesh.texcoords2 = NULL;
        mesh.tangents = NULL;
        mesh.animVertices = NULL;
        mesh.animNormals = NULL;
        mesh.boneIds = NULL;
        mesh.boneWeights = NULL;
        mesh.indices = NULL;
        mesh.vaoId = 0;
        mesh.vboId = NULL;
        
        // Libérer les buffers temporaires
        FreeMeshBuilder(&mb);
        
        printf("    -> Upload mesh vers GPU...\n");
        fflush(stdout);
        
        // Uploader le mesh manuellement vers le GPU
        UploadMesh(&mesh, false);
        
        printf("    -> Mesh uploadé, création du model...\n");
        fflush(stdout);
        
        // Créer le model manuellement au lieu d'utiliser LoadModelFromMesh
        chunk->model = (Model){0};
        chunk->model.transform = MatrixIdentity();
        chunk->model.meshCount = 1;
        chunk->model.materialCount = 1;
        chunk->model.meshes = (Mesh*)RL_MALLOC(sizeof(Mesh));
        chunk->model.materials = (Material*)RL_MALLOC(sizeof(Material));
        chunk->model.meshMaterial = (int*)RL_MALLOC(sizeof(int));
        
        chunk->model.meshes[0] = mesh;
        chunk->model.materials[0] = LoadMaterialDefault();
        // Appliquer la texture atlas au material
        chunk->model.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = atlas;
        chunk->model.meshMaterial[0] = 0;
        
        chunk->meshGenerated = true;
        
        printf("    -> Model créé OK\n");
        
        // Debug: vérifier si le mesh a été uploadé
        printf("    -> Model créé, mesh vaoId=%u, vboId[0]=%u\n", 
               chunk->model.meshes[0].vaoId, 
               chunk->model.meshes[0].vboId[0]);
    } else {
        printf("    -> Aucun vertex (chunk vide)\n");
        FreeMeshBuilder(&mb);
        chunk->meshGenerated = false;
    }
}

// Libérer le mesh d'un chunk
void FreeChunkMesh(Chunk *chunk)
{
    if (chunk->meshGenerated) {
        UnloadModel(chunk->model);
        // Note: UnloadModel libère aussi le mesh, pas besoin de UnloadMesh
        chunk->meshGenerated = false;
    }
}
