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
    
    // Vérifier que toutes les allocations ont réussi
    if (!mb->vertices || !mb->normals || !mb->texcoords || !mb->colors) {
        fprintf(stderr, "ERREUR: Échec d'allocation mémoire dans InitMeshBuilder\n");
        if (mb->vertices) free(mb->vertices);
        if (mb->normals) free(mb->normals);
        if (mb->texcoords) free(mb->texcoords);
        if (mb->colors) free(mb->colors);
        mb->capacity = 0;
        mb->vertexCount = 0;
        mb->vertices = NULL;
        mb->normals = NULL;
        mb->texcoords = NULL;
        mb->colors = NULL;
    }
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
        int newCapacity = mb->capacity * 2;
        
        // Tenter de réallouer avec vérification d'erreur
        float *newVertices = (float*)realloc(mb->vertices, newCapacity * 3 * sizeof(float));
        float *newNormals = (float*)realloc(mb->normals, newCapacity * 3 * sizeof(float));
        float *newTexcoords = (float*)realloc(mb->texcoords, newCapacity * 2 * sizeof(float));
        unsigned char *newColors = (unsigned char*)realloc(mb->colors, newCapacity * 4 * sizeof(unsigned char));
        
        // Vérifier que toutes les réallocations ont réussi
        if (!newVertices || !newNormals || !newTexcoords || !newColors) {
            fprintf(stderr, "ERREUR: Échec de réallocation dans AddFaceToMesh\n");
            // Libérer ceux qui ont réussi (realloc peut avoir réussi partiellement)
            if (newVertices && newVertices != mb->vertices) free(newVertices);
            if (newNormals && newNormals != mb->normals) free(newNormals);
            if (newTexcoords && newTexcoords != mb->texcoords) free(newTexcoords);
            if (newColors && newColors != mb->colors) free(newColors);
            // Les pointeurs originaux sont toujours valides dans mb
            return; // Abandonner l'ajout de cette face
        }
        
        // Tout a réussi, mettre à jour le MeshBuilder
        mb->vertices = newVertices;
        mb->normals = newNormals;
        mb->texcoords = newTexcoords;
        mb->colors = newColors;
        mb->capacity = newCapacity;
    }

    // Obtenir l'index de texture pour cette face
    int textureIndex = GetBlockFaceTexture(blockType, faceIdx);
    Rectangle uvRect = GetTextureRectFromAtlas(textureIndex);
    
    // Coordonnées UV pour cette texture dans l'atlas
    // Inverser Y car OpenGL a l'origine en bas, mais l'atlas a l'origine en haut
    float uvCoords[4][2] = {
        {uvRect.x, uvRect.y + uvRect.height},           // Coin bas-gauche
        {uvRect.x + uvRect.width, uvRect.y + uvRect.height},  // Coin bas-droit
        {uvRect.x + uvRect.width, uvRect.y},            // Coin haut-droit
        {uvRect.x, uvRect.y}                            // Coin haut-gauche
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
    
    // Libérer l'ancien mesh si existant PROPREMENT
    if (chunk->meshGenerated) {
        if (chunk->model.meshes != NULL) {
            UnloadModel(chunk->model);
        }
        // Réinitialiser complètement la structure
        chunk->model = (Model){0};
        chunk->model.meshes = NULL;
        chunk->model.materials = NULL;
        chunk->model.meshMaterial = NULL;
        chunk->meshGenerated = false;
    }

    MeshBuilder mb;
    InitMeshBuilder(&mb, 10000); // Capacité initiale
    
    // Vérifier que l'initialisation a réussi
    if (mb.capacity == 0 || !mb.vertices) {
        fprintf(stderr, "ERREUR: Échec d'initialisation du MeshBuilder\n");
        chunk->meshGenerated = false;
        return;
    }

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
        
        // Vérifier que les allocations ont réussi
        if (!mesh.vertices || !mesh.normals || !mesh.texcoords || !mesh.colors) {
            fprintf(stderr, "ERREUR: Échec d'allocation mémoire pour le mesh\n");
            if (mesh.vertices) RL_FREE(mesh.vertices);
            if (mesh.normals) RL_FREE(mesh.normals);
            if (mesh.texcoords) RL_FREE(mesh.texcoords);
            if (mesh.colors) RL_FREE(mesh.colors);
            FreeMeshBuilder(&mb);
            chunk->meshGenerated = false;
            return;
        }
        
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
        
        printf("    -> Mesh uploadé (vaoId=%u), création du model...\n", mesh.vaoId);
        fflush(stdout);
        
        // Vérifier que l'upload a réussi
        if (mesh.vaoId == 0) {
            fprintf(stderr, "ERREUR: Échec de l'upload du mesh vers le GPU\n");
            RL_FREE(mesh.vertices);
            RL_FREE(mesh.normals);
            RL_FREE(mesh.texcoords);
            RL_FREE(mesh.colors);
            chunk->meshGenerated = false;
            return;
        }
        
        // Créer le model manuellement
        // Note: chunk->model a déjà été réinitialisé au début de la fonction
        chunk->model.transform = MatrixIdentity();
        chunk->model.meshCount = 1;
        chunk->model.materialCount = 1;
        chunk->model.meshes = (Mesh*)RL_MALLOC(sizeof(Mesh));
        chunk->model.materials = (Material*)RL_MALLOC(sizeof(Material));
        chunk->model.meshMaterial = (int*)RL_MALLOC(sizeof(int));
        
        // Vérifier les allocations du model
        if (!chunk->model.meshes || !chunk->model.materials || !chunk->model.meshMaterial) {
            fprintf(stderr, "ERREUR: Échec d'allocation mémoire pour le model\n");
            if (chunk->model.meshes) RL_FREE(chunk->model.meshes);
            if (chunk->model.materials) RL_FREE(chunk->model.materials);
            if (chunk->model.meshMaterial) RL_FREE(chunk->model.meshMaterial);
            // Le mesh a déjà été uploadé, il faut le décharger
            UnloadMesh(mesh);
            chunk->meshGenerated = false;
            return;
        }
        
        chunk->model.meshes[0] = mesh;
        
        printf("    -> Chargement du material par défaut...\n");
        fflush(stdout);
        
        // Utiliser LoadMaterialDefault qui initialise proprement tous les champs
        chunk->model.materials[0] = LoadMaterialDefault();
        
        printf("    -> Material chargé, assignation de la texture atlas...\n");
        fflush(stdout);
        
        // Assigner directement la texture atlas sans passer par SetMaterialTexture
        // pour éviter les problèmes de synchronisation
        chunk->model.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = atlas;
        
        chunk->model.meshMaterial[0] = 0;
        
        chunk->meshGenerated = true;
        
        printf("    -> Model créé OK\n");
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
        // Vérifier que le model est valide avant de le décharger
        if (chunk->model.meshes != NULL) {
            UnloadModel(chunk->model);
            // Réinitialiser les pointeurs
            chunk->model.meshes = NULL;
            chunk->model.materials = NULL;
            chunk->model.meshMaterial = NULL;
        }
        chunk->meshGenerated = false;
    }
}
