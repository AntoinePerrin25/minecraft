#ifndef CHUNK_MESH_H
#define CHUNK_MESH_H

#include "raylib.h"
#include "raymath.h"
#include "network.h"
#include "rlgl.h"

typedef struct {
    Vector3* vertices;
    Vector3* normals;  // Added normal vectors
    Color* colors;
    int* indices;
    int vertexCount;
    int indexCount;
    int capacity;
    Mesh mesh;
    bool dirty;
    bool initialized;
} ChunkMesh;

// Vérifie si un bloc a une face visible dans la direction donnée
static bool isFaceVisible(ChunkData* chunk, int x, int y, int z, int dx, int dy, int dz) {
    // Si on est en bordure de chunk, on considère la face comme visible
    // Le chunk voisin s'occupera de cacher cette face si nécessaire
    if (x + dx < 0 || x + dx >= CHUNK_SIZE || 
        y + dy < 0 || y + dy >= WORLD_HEIGHT || 
        z + dz < 0 || z + dz >= CHUNK_SIZE) {
        return true;
    }
    
    return chunk->blocks[x + dx][y + dy][z + dz] == BLOCK_AIR;
}

// Ajoute une face au mesh
static void addFace(ChunkMesh* mesh, Vector3 pos, Vector3 normal, Color color) {
    // Vérifier si on a besoin de plus d'espace
    if (mesh->vertexCount + 4 > mesh->capacity) {
        mesh->capacity = mesh->capacity ? mesh->capacity * 2 : 1024;
        mesh->vertices = realloc(mesh->vertices, mesh->capacity * sizeof(Vector3));
        mesh->normals = realloc(mesh->normals, mesh->capacity * sizeof(Vector3));  // Allouer les normales
        mesh->colors = realloc(mesh->colors, mesh->capacity * sizeof(Color));
        mesh->indices = realloc(mesh->indices, (mesh->capacity * 6) * sizeof(int));
        
        if (!mesh->vertices || !mesh->normals || !mesh->colors || !mesh->indices) {
            // Gestion d'erreur d'allocation
            mesh->capacity = 0;
            mesh->vertexCount = 0;
            mesh->indexCount = 0;
            return;
        }
    }
    
    // Les 4 sommets de la face - dans un ordre clockwise consistant
    Vector3 v[4];
    if (normal.x != 0) {
        // Face X (gauche/droite)
        float x = pos.x + (normal.x > 0 ? 1.0f : 0.0f);
        if (normal.x > 0) {
            v[0] = (Vector3){x, pos.y, pos.z};          // Bas gauche
            v[1] = (Vector3){x, pos.y + 1, pos.z};      // Haut gauche
            v[2] = (Vector3){x, pos.y + 1, pos.z + 1};  // Haut droite
            v[3] = (Vector3){x, pos.y, pos.z + 1};      // Bas droite
        } else {
            v[0] = (Vector3){x, pos.y, pos.z + 1};      // Bas gauche
            v[1] = (Vector3){x, pos.y + 1, pos.z + 1};  // Haut gauche
            v[2] = (Vector3){x, pos.y + 1, pos.z};      // Haut droite
            v[3] = (Vector3){x, pos.y, pos.z};          // Bas droite
        }
    } else if (normal.y != 0) {
        // Face Y (haut/bas)
        float y = pos.y + (normal.y > 0 ? 1.0f : 0.0f);
        if (normal.y > 0) {
            v[0] = (Vector3){pos.x, y, pos.z};          // Bas gauche
            v[1] = (Vector3){pos.x, y, pos.z + 1};      // Haut gauche
            v[2] = (Vector3){pos.x + 1, y, pos.z + 1};  // Haut droite
            v[3] = (Vector3){pos.x + 1, y, pos.z};      // Bas droite
        } else {
            v[0] = (Vector3){pos.x, y, pos.z + 1};      // Bas gauche
            v[1] = (Vector3){pos.x, y, pos.z};          // Haut gauche
            v[2] = (Vector3){pos.x + 1, y, pos.z};      // Haut droite
            v[3] = (Vector3){pos.x + 1, y, pos.z + 1};  // Bas droite
        }
    } else {
        // Face Z (avant/arrière)
        float z = pos.z + (normal.z > 0 ? 1.0f : 0.0f);
        if (normal.z > 0) {
            v[0] = (Vector3){pos.x, pos.y, z};          // Bas gauche
            v[1] = (Vector3){pos.x, pos.y + 1, z};      // Haut gauche
            v[2] = (Vector3){pos.x + 1, pos.y + 1, z};  // Haut droite
            v[3] = (Vector3){pos.x + 1, pos.y, z};      // Bas droite
        } else {
            v[0] = (Vector3){pos.x + 1, pos.y, z};      // Bas gauche
            v[1] = (Vector3){pos.x + 1, pos.y + 1, z};  // Haut gauche
            v[2] = (Vector3){pos.x, pos.y + 1, z};      // Haut droite
            v[3] = (Vector3){pos.x, pos.y, z};          // Bas droite
        }
    }
    
    // Ajouter les vertices et leurs attributs
    for (int i = 0; i < 4; i++) {
        mesh->vertices[mesh->vertexCount + i] = v[i];
        mesh->normals[mesh->vertexCount + i] = normal;  // Ajouter la normale
        mesh->colors[mesh->vertexCount + i] = color;
    }
    
    // Ajouter les indices pour former deux triangles (ordre clockwise)
    int baseIndex = mesh->vertexCount;
    mesh->indices[mesh->indexCount++] = baseIndex;      // Triangle 1
    mesh->indices[mesh->indexCount++] = baseIndex + 1;
    mesh->indices[mesh->indexCount++] = baseIndex + 2;
    mesh->indices[mesh->indexCount++] = baseIndex;      // Triangle 2
    mesh->indices[mesh->indexCount++] = baseIndex + 2;
    mesh->indices[mesh->indexCount++] = baseIndex + 3;
    
    mesh->vertexCount += 4;
}

// Génère ou met à jour le mesh d'un chunk
static void updateChunkMesh(ChunkMesh* mesh, ChunkData* chunk) {
    // Libérer les anciens buffers GPU et CPU
    if (mesh->initialized) {
        UnloadMesh(mesh->mesh);
        mesh->initialized = false;
    }
    
    if (mesh->vertices) {
        free(mesh->vertices);
        free(mesh->normals);
        free(mesh->colors);
        free(mesh->indices);
        mesh->vertices = NULL;
        mesh->normals = NULL;
        mesh->colors = NULL;
        mesh->indices = NULL;
        mesh->capacity = 0;
    }

    // Réinitialiser le mesh
    mesh->vertexCount = 0;
    mesh->indexCount = 0;
    
    // Pour chaque block dans le chunk
    for (int x = 0; x < CHUNK_SIZE; x++) {
        for (int y = 0; y < WORLD_HEIGHT; y++) {
            for (int z = 0; z < CHUNK_SIZE; z++) {
                BlockType block = chunk->blocks[x][y][z];
                if (block == BLOCK_AIR) continue;
                
                // Déterminer la couleur du bloc
                Color color;
                switch (block) {
                    case BLOCK_BEDROCK: color = BLACK; break;
                    case BLOCK_STONE: color = GRAY; break;
                    case BLOCK_DIRT: color = BROWN; break;
                    case BLOCK_GRASS: color = GREEN; break;
                    default: continue;
                }
                
                Vector3 pos = {x, y, z};
                
                // Vérifier chaque face
                if (isFaceVisible(chunk, x, y, z, -1, 0, 0)) 
                    addFace(mesh, pos, (Vector3){-1,0,0}, color);
                if (isFaceVisible(chunk, x, y, z, 1, 0, 0))
                    addFace(mesh, pos, (Vector3){1,0,0}, color);
                if (isFaceVisible(chunk, x, y, z, 0, -1, 0))
                    addFace(mesh, pos, (Vector3){0,-1,0}, color);
                if (isFaceVisible(chunk, x, y, z, 0, 1, 0))
                    addFace(mesh, pos, (Vector3){0,1,0}, color);
                if (isFaceVisible(chunk, x, y, z, 0, 0, -1))
                    addFace(mesh, pos, (Vector3){0,0,-1}, color);
                if (isFaceVisible(chunk, x, y, z, 0, 0, 1))
                    addFace(mesh, pos, (Vector3){0,0,1}, color);
            }
        }
    }
    
    // Créer le mesh seulement si on a des vertices
    if (mesh->vertexCount > 0) {
        mesh->mesh = (Mesh){0};
        mesh->mesh.vertexCount = mesh->vertexCount;
        mesh->mesh.triangleCount = mesh->indexCount / 3;
        
        // Convertir les buffers pour le GPU
        float* vertices = (float*)malloc(mesh->vertexCount * 3 * sizeof(float));
        float* normals = (float*)malloc(mesh->vertexCount * 3 * sizeof(float));  // Allouer les normales
        unsigned char* colors = (unsigned char*)malloc(mesh->vertexCount * 4);
        unsigned short* indices = (unsigned short*)malloc(mesh->indexCount * sizeof(unsigned short));
        
        if (!vertices || !normals || !colors || !indices) {
            // Gestion d'erreur d'allocation
            free(vertices);
            free(normals);
            free(colors);
            free(indices);
            return;
        }
        
        // Copier les données
        for (int i = 0; i < mesh->vertexCount; i++) {
            vertices[i * 3] = mesh->vertices[i].x;
            vertices[i * 3 + 1] = mesh->vertices[i].y;
            vertices[i * 3 + 2] = mesh->vertices[i].z;
            
            normals[i * 3] = mesh->normals[i].x;     // Copier les normales
            normals[i * 3 + 1] = mesh->normals[i].y;
            normals[i * 3 + 2] = mesh->normals[i].z;
        }
        
        for (int i = 0; i < mesh->vertexCount; i++) {
            colors[i * 4] = mesh->colors[i].r;
            colors[i * 4 + 1] = mesh->colors[i].g;
            colors[i * 4 + 2] = mesh->colors[i].b;
            colors[i * 4 + 3] = mesh->colors[i].a;
        }
        
        for (int i = 0; i < mesh->indexCount; i++) {
            indices[i] = (unsigned short)mesh->indices[i];
        }
        
        // Assigner les buffers au mesh
        mesh->mesh.vertices = vertices;
        mesh->mesh.normals = normals;
        mesh->mesh.colors = colors;
        mesh->mesh.indices = indices;
        
        // Charger dans la VRAM
        UploadMesh(&mesh->mesh, false);
        mesh->initialized = true;
    }
    
    mesh->dirty = false;
}

// Libère les ressources du mesh
static void freeChunkMesh(ChunkMesh* mesh) {
    if (!mesh) return;
    
    if (mesh->initialized) {
        UnloadMesh(mesh->mesh);
    }
    
    free(mesh->vertices);
    free(mesh->normals);  // Libérer les normales
    free(mesh->colors);
    free(mesh->indices);
    
    mesh->vertices = NULL;
    mesh->normals = NULL;  // Mettre à NULL
    mesh->colors = NULL;
    mesh->indices = NULL;
    mesh->vertexCount = 0;
    mesh->indexCount = 0;
    mesh->capacity = 0;
    mesh->initialized = false;
    mesh->dirty = false;
}

#endif // CHUNK_MESH_H