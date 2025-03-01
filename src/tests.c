#include <stdio.h>
#include <assert.h>
#include "raylib.h"
#include "chunk_manager.h"

// Forward declarations for test functions
void test_chunk_manager_init();
void test_client_chunk_creation();
void test_add_get_remove_chunk();
void test_world_to_chunk_coords();
void test_chunk_vertical_operations();
void test_unload_distant_chunks();

int main() 
{
    InitWindow(800, 600, "Chunk Manager Tests");
    
    printf("Running Chunk Manager tests...\n\n");
    
    // Run all tests
    test_chunk_manager_init();
    test_client_chunk_creation();
    test_add_get_remove_chunk();
    test_world_to_chunk_coords();
    test_chunk_vertical_operations();
    test_unload_distant_chunks();
    
    printf("\nAll tests passed successfully!\n");
    
    CloseWindow();
    return 0;
}

void test_chunk_manager_init() 
{
    printf("Testing ChunkManager initialization...\n");
    
    // Test initialization with valid capacity
    int capacity = 10;
    ChunkManager manager = InitChunkManager(capacity);
    
    assert(manager.capacity == capacity);
    assert(manager.count == 0);
    assert(manager.chunks != NULL);
    
    // Test cleanup
    FreeChunkManager(manager);
    
    printf("ChunkManager initialization test passed.\n\n");
}

void test_client_chunk_creation() 
{
    printf("Testing ClientChunk creation and destruction...\n");
    
    // Test chunk creation
    int x = 5, z = 10;
    ClientChunk* chunk = CreateClientChunk(x, z);
    
    assert(chunk != NULL);
    assert(chunk->x == x);
    assert(chunk->z == z);
    assert(chunk->loaded == 0);
    assert(chunk->data != NULL);
    assert(chunk->mesh != NULL);
    
    // Test chunk cleanup
    FreeClientChunk(chunk);
    
    printf("ClientChunk creation test passed.\n\n");
}

void test_add_get_remove_chunk() 
{
    printf("Testing chunk addition, retrieval, and removal...\n");
    
    // Initialize manager
    ChunkManager manager = InitChunkManager(5);
    
    // Create and add chunks
    ClientChunk* chunk1 = CreateClientChunk(0, 0);
    ClientChunk* chunk2 = CreateClientChunk(1, 0);
    ClientChunk* chunk3 = CreateClientChunk(0, 1);
    
    // Mark as loaded
    chunk1->loaded = 1;
    chunk2->loaded = 1;
    chunk3->loaded = 1;
    
    // Add chunks to manager
    AddChunk(&manager, chunk1);
    AddChunk(&manager, chunk2);
    AddChunk(&manager, chunk3);
    
    assert(manager.count == 3);
    
    // Test chunk retrieval
    ClientChunk* retrieved = GetChunk(&manager, 0, 0);
    assert(retrieved != NULL);
    assert(retrieved->x == 0);
    assert(retrieved->z == 0);
    
    retrieved = GetChunk(&manager, 1, 0);
    assert(retrieved != NULL);
    assert(retrieved->x == 1);
    assert(retrieved->z == 0);
    
    // Test chunk removal
    RemoveChunk(&manager, 0, 0);
    assert(manager.count == 2);
    
    // Test chunk no longer exists
    retrieved = GetChunk(&manager, 0, 0);
    assert(retrieved == NULL);
    
    // Cleanup
    FreeChunkManager(manager);
    
    printf("Chunk addition, retrieval, and removal test passed.\n\n");
}

void test_world_to_chunk_coords() 
{
    printf("Testing world to chunk coordinate conversion...\n");
    
    // Test positive coordinates
    Vector3 pos1 = (Vector3){ 16.5f, 64.0f, 32.0f };
    Vector2 chunk_pos1 = worldToChunkCoords(pos1);
    assert(chunk_pos1.x == 1.0f);
    assert(chunk_pos1.y == 2.0f);
    
    // Test coordinates at chunk boundaries
    Vector3 pos2 = (Vector3){ 16.0f, 64.0f, 16.0f };
    Vector2 chunk_pos2 = worldToChunkCoords(pos2);
    assert(chunk_pos2.x == 1.0f);
    assert(chunk_pos2.y == 1.0f);
    
    // Test negative coordinates
    Vector3 pos3 = (Vector3){ -16.5f, 64.0f, -32.0f };
    Vector2 chunk_pos3 = worldToChunkCoords(pos3);
    assert(chunk_pos3.x == -2.0f);
    assert(chunk_pos3.y == -2.0f);
    
    printf("World to chunk coordinate conversion test passed.\n\n");
}

void test_chunk_vertical_operations() 
{
    printf("Testing chunk vertical operations...\n");
    
    // Test creation
    unsigned int y = 5;
    ChunkVertical* vertical = CreateChunkVertical(y);
    
    assert(vertical != NULL);
    assert(vertical->y == y);
    assert(vertical->isOnlyBlockType == 1);
    assert(vertical->blockType == BLOCK_AIR);
    
    // Test cleanup
    FreeChunkVertical(vertical);
    
    printf("Chunk vertical operations test passed.\n\n");
}

void test_unload_distant_chunks() 
{
    // Initialize manager
    ChunkManager manager = InitChunkManager(200);
    
    // Create and add chunks at various positions
    for (int x = -3; x <= 3; x++) {
        for (int z = -3; z <= 3; z++) {
            ClientChunk* chunk = CreateClientChunk(x, z);
            chunk->loaded = 1;
            AddChunk(&manager, chunk);
            printf("Added chunk at (%d, %d)\n", x, z);
        }
    }
    // Player position at origin
    Vector3 playerPos = (Vector3){ 0.0f, 64.0f, 0.0f };
    
    // Unload chunks outside render distance
    unloadDistantChunks(&manager, playerPos);
    // Check that chunks within render distance still exist
    for (int x = -RENDER_DISTANCE; x <= RENDER_DISTANCE; x++) {
        for (int z = -RENDER_DISTANCE; z <= RENDER_DISTANCE; z++) {
            ClientChunk* chunk = GetChunk(&manager, x, z);
            assert(chunk != NULL && "Chunk within render distance should not be unloaded");
        }
    }
    
    // Check that chunks outside render distance were removed
    // Only need to check one as an example
    ClientChunk* distant_chunk = GetChunk(&manager, -3, -3);
    assert(distant_chunk == NULL);
    
    // Cleanup
    FreeChunkManager(manager);
}

void test_reload_chunk()
{
    // testing to load 100 chunks and remove them all
    ChunkManager manager = InitChunkManager(100);
    ClientChunk* chunklist[100] = {0};
    int index = 0;
    for (int x = -5; x <= 5; x++) {
        for (int z = -5; z <= 5; z++) {
            ClientChunk* chunk = CreateClientChunk(x, z);
            assert(chunk && "Chunk creation failed");
            if (chunk) {
                chunk->loaded = 1;
                chunklist[index++] = chunk;
                AddChunk(&manager, chunk);
                assert(manager.count <= 100 && "Chunk manager count exceeded 100");
            }
            else
            {
                printf("Failed to create chunk at (%d, %d)\n", x, z);
            }
        }
    }

    // remove all chunks and add them back
    for (int i = 0; i < 100; i++)
    {
        RemoveChunk(&manager, chunklist[i]->x, chunklist[i]->z);
        assert(chunklist[i] == NULL && "Chunk should be successfully removed");
    }

    FreeChunkManager(manager);
    printf("Chunk loading and removal test passed.\n\n");
}