#define NOB_IMPLEMENTATION
#include "../nob.h"

#include <stdio.h>
#include <assert.h>
#include <raylib/raylib.h>
#include "chunk_manager.h"
#include "world.h"  // Pour la fonction generateChunk

void test_chunk_manager_init()
{
    printf("Testing ChunkManager initialization...\n");

    // Test initialization with valid capacity
    int capacity = 10;
    ChunkManager* manager = InitChunkManager(capacity);

    assert(manager->capacity == capacity);
    assert(manager->count == 0);

    // Test cleanup
    FreeChunkManager(manager);

    printf("ChunkManager initialization test passed.\n\n");
}

void test_client_chunk_creation()
{
    printf("Testing ClientChunk creation and destruction...\n");

    // Test chunk creation
    int x = 5, z = 10;
    ClientChunk *chunk = CreateClientChunk(x, z);

    assert(chunk != NULL);
    assert(chunk->x == x);
    assert(chunk->z == z);
    assert(chunk->loaded == 0);

    // Test chunk cleanup
    FreeClientChunk(chunk);

    printf("ClientChunk creation test passed.\n\n");
}

void test_add_get_remove_chunk()
{
    printf("Testing chunk addition, retrieval, and removal...\n");

    // Initialize manager
    ChunkManager *manager = InitChunkManager(5);

    // Create and add chunks
    ClientChunk *chunk1 = CreateClientChunk(0, 0);
    ClientChunk *chunk2 = CreateClientChunk(1, 0);
    ClientChunk *chunk3 = CreateClientChunk(0, 1);

    // Mark as loaded
    chunk1->loaded = 1;
    chunk2->loaded = 1;
    chunk3->loaded = 1;

    // Add chunks to manager
    AddChunk(manager, chunk1);
    AddChunk(manager, chunk2);
    AddChunk(manager, chunk3);

    assert(manager->count == 3);

    // Test chunk retrieval
    ClientChunk *retrieved = GetChunk(manager, 0, 0);
    assert(retrieved != NULL);
    assert(retrieved->x == 0);
    assert(retrieved->z == 0);

    retrieved = GetChunk(manager, 1, 0);
    assert(retrieved != NULL);
    assert(retrieved->x == 1);
    assert(retrieved->z == 0);

    // Test chunk removal
    RemoveChunk(manager, 0);
    assert(manager->count == 2);

    // Test chunk no longer exists
    retrieved = GetChunk(manager, 0, 0);
    assert(retrieved == NULL);

    // Cleanup
    FreeChunkManager(manager);

    printf("Chunk addition, retrieval, and removal test passed.\n\n");
}

void test_world_to_chunk_coords()
{
    printf("Testing world to chunk coordinate conversion...\n");

    // Test positive coordinates
    Vector3 pos1 = (Vector3){16.5f, 64.0f, 32.0f};
    Vector3Int chunk_pos1 = worldToChunkCoords(&pos1);
    assert(chunk_pos1.x == 1);
    assert(chunk_pos1.z == 2);

    // Test coordinates at chunk boundaries
    Vector3 pos2 = (Vector3){16.0f, 64.0f, 16.0f};
    Vector3Int chunk_pos2 = worldToChunkCoords(&pos2);
    assert(chunk_pos2.x == 1);
    assert(chunk_pos2.z == 1);

    // Test negative coordinates
    Vector3 pos3 = (Vector3){-16.5f, 64.0f, -32.0f};
    Vector3Int chunk_pos3 = worldToChunkCoords(&pos3);
    assert(chunk_pos3.x == -2);
    assert(chunk_pos3.z == -2);

    printf("World to chunk coordinate conversion test passed.\n\n");
}


void test_unload_distant_chunks()
{
    // Initialize manager
    ChunkManager *manager = InitChunkManager(200);

    // Create and add chunks at various positions
    for (int x = -3; x <= 3; x++)
    {
        for (int z = -3; z <= 3; z++)
        {
            ClientChunk *chunk = CreateClientChunk(x, z);
            chunk->loaded = 1;
            AddChunk(manager, chunk);
            printf("Added chunk at (%2d, %2d)\n", x, z);
        }
    }
    // Player position near the center of a chunk
    Vector3 playerPos = (Vector3){8.0f, 64.0f, 8.0f};
    printf("Number of chunks loaded: %2d\n", manager->count);
    // Unload chunks outside render distance
    printf("Unloading distant chunks...\n");
    unloadDistantChunks(manager, &playerPos);
    // Check that chunks within render distance still exist
    for (int x = -RENDER_DISTANCE; x <= RENDER_DISTANCE; x++)
    {
        for (int z = -RENDER_DISTANCE; z <= RENDER_DISTANCE; z++)
        {
            printf("Checking chunk at (%2d, %2d)\n", x, z);
            ClientChunk *chunk = GetChunk(manager, x, z);
            if (chunk)
            {
                printf("Chunk at (%2d, %2d) still loaded\n", x, z);
            }
            assert(chunk != NULL && "Chunk within render distance should not be unloaded");
        }
    }
    printf("Distant chunks unloaded successfully\n");

    printChunkLoaded(manager);

    // Check that chunks outside render distance were removed
    // Only need to check one as an example
    ClientChunk *distant_chunk = GetChunk(manager, -3, -3);
    assert(distant_chunk == NULL && "Distant chunk should have been unloaded");

    int expected_count_after_unload = 25;
    printf("Expected : %d\nCount : %d\n", expected_count_after_unload, manager->count);
    assert(manager->count == expected_count_after_unload && "Loaded chunk count is incorrect after unloading.");

    printf("Final loaded chunk count: %d\n", manager->count);

    // Cleanup
    FreeChunkManager(manager);
    printf("Cleanup completed successfully\n");
    printf("Unload distant chunks test passed.\n\n");
}

// Nouveau test pour la compression et décompression des chunks
void test_chunk_compression_decompression()
{
    FILE* chunkinit = fopen("./fullchunkinit.txt", "w");
    FILE* chunkDC =   fopen("./fullchunkDC.txt"  , "w");

    printf("Testing chunk compression and decompression...\n");
    
    // Seed aléatoire pour la génération
    int seed = 12345;
    int chunkX = 1;
    int chunkZ = 2;
    
    // Générer un chunk complet
    printf("Generating a new chunk with seed %d at position (%d, %d)...\n", seed, chunkX, chunkZ);
    FullChunk originalChunk = generateChunk(chunkX, chunkZ, seed);
    
    printFullChunk(originalChunk, chunkinit);


    // Compresser le chunk
    printf("Compressing the chunk...\n");
    ChunkData compressedChunk = CompressChunk(&originalChunk);
    
    // Analyse d'optimisation
    int totalSections = CHUNK_SIZE;
    int uniformSections = 0;
    int nullSections = 0;
    
    for (int section = 0; section < CHUNK_SIZE; section++) {
        if (compressedChunk.verticals[section] == NULL) {
            nullSections++;
        } else if (compressedChunk.verticals[section]->compressed) {
            uniformSections++;
        }
    }
    
    float compressionRatio = (float)(uniformSections + nullSections) / totalSections * 100.0f;
    printf("Compression statistics: %d/%d sections uniformes (%d NULL), %.2f%% d'efficacité\n", 
           uniformSections, totalSections, nullSections, compressionRatio);
    
    // Décompresser le chunk
    printf("Decompressing the chunk...\n");
    FullChunk decompressedChunk = DecompressChunk(&compressedChunk);
    
    printFullChunk(decompressedChunk, chunkDC);
    
    fclose(chunkinit);
    fclose(chunkDC);

    // Comparer les chunks original et décompressé
    printf("Comparing original and decompressed chunks...\n");
    int areEqual = AreFullChunksEqual(&originalChunk, &decompressedChunk);
    
    assert(areEqual && "Chunks should be identical after compression and decompression");
    printf("Chunk compression and decompression test passed!\n");
    
    // Libérer la mémoire allouée par la compression
    for (int section = 0; section < CHUNK_SIZE; section++) {
        if (compressedChunk.verticals[section] != NULL) {
            free(compressedChunk.verticals[section]);
            compressedChunk.verticals[section] = NULL;
        }
    }
    
    printf("Chunk compression and decompression test completed.\n\n");
}

typedef BlockData checksizedata[2 - sizeof(BlockData)];

int main()
{
    
    printf("Running Chunk Manager tests...\n\n");

    // Run all tests
    test_chunk_manager_init();
    test_client_chunk_creation();
    test_add_get_remove_chunk();
    test_world_to_chunk_coords();
    test_unload_distant_chunks();
    test_chunk_compression_decompression();  // Ajout du nouveau test

    printf("\nAll tests passed successfully!\n");

    return 0;
}