
// Define chunk size constant and world height
#define CHUNK_SIZE 16
#define WORLD_HEIGHT 256

// 3 bits enum
typedef enum
{
    BLOCK_AIR = 0,
    BLOCK_GRASS = 1,
    BLOCK_DIRT = 2,
    BLOCK_STONE = 3,
    BLOCK_BEDROCK = 4,
    BLOCK_WATER = 5,
    BLOCK_SAND = 6,
    BLOCK_WOOD = 7
} BlockType;

// Use union to efficiently pack block types
typedef union __attribute__((packed)) {
    struct {
        uint8_t b0 : 3;
        uint8_t b1 : 3;
        uint8_t b2 : 3;
        uint8_t b3 : 3;
        uint8_t b4 : 3;
        uint8_t b5 : 3;
        uint8_t b6 : 3;
        uint8_t b7 : 3;
        uint8_t b8 : 3;
        uint8_t b9 : 3;
    } blocks;
    uint32_t raw;  // For bulk operations
} ChunkBits;

typedef struct __attribute__((packed)) ChunkRow
{
    ChunkBits bits[(CHUNK_SIZE + 9) / 10]; // Each ChunkBits holds 10 blocks
} ChunkRow;

typedef struct __attribute__((packed)) ChunkLayer
{
    ChunkRow rows[CHUNK_SIZE];
} ChunkLayer;

typedef struct __attribute__((packed)) ChunkVertical
{
    ChunkLayer* layers[CHUNK_SIZE];
    unsigned int y;
    unsigned int isOnlyBlockType : 1;
    BlockType blockType : 3;
    unsigned int layersAllocated : 8; // Track which layers are allocated
} ChunkVertical;

typedef struct __attribute__((packed)) ChunkData
{
    ChunkVertical verticals[WORLD_HEIGHT / CHUNK_SIZE];
    unsigned int isLoaded : 1;
    int x, z;
} ChunkData;

// Helper functions to get index into the bit array
static inline int get_block_array_index(int local_x) {
    return local_x / 10;
}

static inline int get_block_bit_position(int local_x) {
    return local_x % 10;
}

// Initialize a chunk
void chunk_init(ChunkData* chunk, int x, int z) {
    chunk->x = x;
    chunk->z = z;
    chunk->isLoaded = 0;
    
    for (int i = 0; i < WORLD_HEIGHT / CHUNK_SIZE; i++) {
        chunk->verticals[i].y = i * CHUNK_SIZE;
        chunk->verticals[i].isOnlyBlockType = 1;
        chunk->verticals[i].blockType = BLOCK_AIR;
        chunk->verticals[i].layersAllocated = 0;
        
        for (int j = 0; j < CHUNK_SIZE; j++) {
            chunk->verticals[i].layers[j] = NULL;
        }
    }
}

// Free memory allocated for a chunk
void chunk_free(ChunkData* chunk) {
    for (int i = 0; i < WORLD_HEIGHT / CHUNK_SIZE; i++) {
        for (int j = 0; j < CHUNK_SIZE; j++) {
            if (chunk->verticals[i].layers[j] != NULL) {
                free(chunk->verticals[i].layers[j]);
                chunk->verticals[i].layers[j] = NULL;
            }
        }
    }
    chunk->isLoaded = 0;
}

// Ensure that a specific layer is allocated
ChunkLayer* chunk_ensure_layer(ChunkData* chunk, int y) {
    int vertical_index = y / CHUNK_SIZE;
    int layer_index = y % CHUNK_SIZE;
    
    if (vertical_index >= WORLD_HEIGHT / CHUNK_SIZE) {
        return NULL; // Out of bounds
    }
    
    ChunkVertical* vertical = &chunk->verticals[vertical_index];
    
    if (vertical->layers[layer_index] == NULL) {
        vertical->layers[layer_index] = (ChunkLayer*)calloc(1, sizeof(ChunkLayer));
        if (vertical->layers[layer_index] == NULL) {
            return NULL; // Memory allocation failed
        }
        
        // Mark the layer as allocated using bitfield
        vertical->layersAllocated |= (1 << layer_index);
        
        // If this was an optimized section (all one type), we need to fill it
        if (vertical->isOnlyBlockType) {
            BlockType fillType = vertical->blockType;
            for (int z = 0; z < CHUNK_SIZE; z++) {
                for (int x = 0; x < CHUNK_SIZE; x++) {
                    int array_index = get_block_array_index(x);
                    int bit_pos = get_block_bit_position(x);
                    
                    switch (bit_pos) {
                        case 0: vertical->layers[layer_index]->rows[z].bits[array_index].blocks.b0 = fillType; break;
                        case 1: vertical->layers[layer_index]->rows[z].bits[array_index].blocks.b1 = fillType; break;
                        case 2: vertical->layers[layer_index]->rows[z].bits[array_index].blocks.b2 = fillType; break;
                        case 3: vertical->layers[layer_index]->rows[z].bits[array_index].blocks.b3 = fillType; break;
                        case 4: vertical->layers[layer_index]->rows[z].bits[array_index].blocks.b4 = fillType; break;
                        case 5: vertical->layers[layer_index]->rows[z].bits[array_index].blocks.b5 = fillType; break;
                        case 6: vertical->layers[layer_index]->rows[z].bits[array_index].blocks.b6 = fillType; break;
                        case 7: vertical->layers[layer_index]->rows[z].bits[array_index].blocks.b7 = fillType; break;
                        case 8: vertical->layers[layer_index]->rows[z].bits[array_index].blocks.b8 = fillType; break;
                        case 9: vertical->layers[layer_index]->rows[z].bits[array_index].blocks.b9 = fillType; break;
                    }
                }
            }
        }
    }
    
    return vertical->layers[layer_index];
}

// Get block type at a specific coordinate
BlockType chunk_get_block(ChunkData* chunk, int x, int y, int z) {
    // Convert to local coordinates
    int local_x = x & (CHUNK_SIZE - 1); // Fast modulo for power of 2
    int local_z = z & (CHUNK_SIZE - 1);
    
    int vertical_index = y / CHUNK_SIZE;
    int layer_index = y % CHUNK_SIZE;
    
    if (vertical_index >= WORLD_HEIGHT / CHUNK_SIZE) {
        return BLOCK_AIR; // Out of bounds
    }
    
    ChunkVertical* vertical = &chunk->verticals[vertical_index];
    
    // Fast path: if the entire vertical is a single block type
    if (vertical->isOnlyBlockType) {
        return vertical->blockType;
    }
    
    // If layer isn't allocated, it's full of the vertical's block type
    if (vertical->layers[layer_index] == NULL) {
        return vertical->blockType;
    }
    
    // Calculate array index and bit position
    int array_index = get_block_array_index(local_x);
    int bit_pos = get_block_bit_position(local_x);
    
    // Access the specific block
    switch (bit_pos) {
        case 0: return (BlockType)vertical->layers[layer_index]->rows[local_z].bits[array_index].blocks.b0;
        case 1: return (BlockType)vertical->layers[layer_index]->rows[local_z].bits[array_index].blocks.b1;
        case 2: return (BlockType)vertical->layers[layer_index]->rows[local_z].bits[array_index].blocks.b2;
        case 3: return (BlockType)vertical->layers[layer_index]->rows[local_z].bits[array_index].blocks.b3;
        case 4: return (BlockType)vertical->layers[layer_index]->rows[local_z].bits[array_index].blocks.b4;
        case 5: return (BlockType)vertical->layers[layer_index]->rows[local_z].bits[array_index].blocks.b5;
        case 6: return (BlockType)vertical->layers[layer_index]->rows[local_z].bits[array_index].blocks.b6;
        case 7: return (BlockType)vertical->layers[layer_index]->rows[local_z].bits[array_index].blocks.b7;
        case 8: return (BlockType)vertical->layers[layer_index]->rows[local_z].bits[array_index].blocks.b8;
        case 9: return (BlockType)vertical->layers[layer_index]->rows[local_z].bits[array_index].blocks.b9;
        default: return BLOCK_AIR; // Should never happen
    }
}

// Set block type at a specific coordinate
int chunk_set_block(ChunkData* chunk, int x, int y, int z, BlockType type) {
    // Convert to local coordinates
    int local_x = x & (CHUNK_SIZE - 1);
    int local_z = z & (CHUNK_SIZE - 1);
    
    int vertical_index = y / CHUNK_SIZE;
    int layer_index = y % CHUNK_SIZE;
    
    if (vertical_index >= WORLD_HEIGHT / CHUNK_SIZE) {
        return 0; // Out of bounds
    }
    
    ChunkVertical* vertical = &chunk->verticals[vertical_index];
    
    // Fast path optimization: if setting to the same type as the entire vertical
    if (vertical->isOnlyBlockType && vertical->blockType == type) {
        return 1; // Already set to this type
    }
    
    // Ensure the layer is allocated
    ChunkLayer* layer = chunk_ensure_layer(chunk, y);
    if (layer == NULL) {
        return 0; // Layer allocation failed
    }
    
    // The vertical is no longer of only one block type
    vertical->isOnlyBlockType = 0;
    
    // Calculate array index and bit position
    int array_index = get_block_array_index(local_x);
    int bit_pos = get_block_bit_position(local_x);
    
    // Set the specific block
    switch (bit_pos) {
        case 0: layer->rows[local_z].bits[array_index].blocks.b0 = type; break;
        case 1: layer->rows[local_z].bits[array_index].blocks.b1 = type; break;
        case 2: layer->rows[local_z].bits[array_index].blocks.b2 = type; break;
        case 3: layer->rows[local_z].bits[array_index].blocks.b3 = type; break;
        case 4: layer->rows[local_z].bits[array_index].blocks.b4 = type; break;
        case 5: layer->rows[local_z].bits[array_index].blocks.b5 = type; break;
        case 6: layer->rows[local_z].bits[array_index].blocks.b6 = type; break;
        case 7: layer->rows[local_z].bits[array_index].blocks.b7 = type; break;
        case 8: layer->rows[local_z].bits[array_index].blocks.b8 = type; break;
        case 9: layer->rows[local_z].bits[array_index].blocks.b9 = type; break;
    }
    
    return 1;
}

// Fill a vertical section with a specific block type
void chunk_fill_vertical(ChunkData* chunk, int vertical_index, BlockType type) {
    if (vertical_index >= WORLD_HEIGHT / CHUNK_SIZE) {
        return; // Out of bounds
    }
    
    ChunkVertical* vertical = &chunk->verticals[vertical_index];
    
    // Free any allocated layers since we're going to optimize
    for (int j = 0; j < CHUNK_SIZE; j++) {
        if (vertical->layers[j] != NULL) {
            free(vertical->layers[j]);
            vertical->layers[j] = NULL;
        }
    }
    
    vertical->isOnlyBlockType = 1;
    vertical->blockType = type;
    vertical->layersAllocated = 0;
}

// Check if an entire chunk is filled with a single block type
int chunk_is_homogeneous(ChunkData* chunk, BlockType* type) {
    if (!chunk->isLoaded) {
        return 0;
    }
    
    BlockType first_type = BLOCK_AIR;
    int first_found = 0;
    
    for (int i = 0; i < WORLD_HEIGHT / CHUNK_SIZE; i++) {
        ChunkVertical* vertical = &chunk->verticals[i];
        
        if (!vertical->isOnlyBlockType) {
            return 0; // This vertical has mixed block types
        }
        
        if (!first_found) {
            first_type = vertical->blockType;
            first_found = 1;
        } else if (first_type != vertical->blockType) {
            return 0; // Different block type found
        }
    }
    
    if (type != NULL) {
        *type = first_type;
    }
    
    return first_found;
}

// Optimize a chunk by converting uniform areas to optimized verticals
void chunk_optimize(ChunkData* chunk) {
    for (int i = 0; i < WORLD_HEIGHT / CHUNK_SIZE; i++) {
        ChunkVertical* vertical = &chunk->verticals[i];
        
        if (vertical->isOnlyBlockType) {
            continue; // Already optimized
        }
        
        // Check if all blocks in this vertical are the same type
        int all_same = 1;
        BlockType first_type = BLOCK_AIR;
        int first_found = 0;
        
        for (int y = 0; y < CHUNK_SIZE && all_same; y++) {
            if (vertical->layers[y] == NULL) {
                if (!first_found) {
                    first_type = vertical->blockType;
                    first_found = 1;
                } else if (first_type != vertical->blockType) {
                    all_same = 0;
                }
                continue;
            }
            
            for (int z = 0; z < CHUNK_SIZE && all_same; z++) {
                for (int x = 0; x < CHUNK_SIZE && all_same; x++) {
                    BlockType block = chunk_get_block(chunk, 
                                                     x + chunk->x * CHUNK_SIZE, 
                                                     y + i * CHUNK_SIZE, 
                                                     z + chunk->z * CHUNK_SIZE);
                    
                    if (!first_found) {
                        first_type = block;
                        first_found = 1;
                    } else if (first_type != block) {
                        all_same = 0;
                    }
                }
            }
        }
        
        if (all_same && first_found) {
            // Optimize this vertical
            chunk_fill_vertical(chunk, i, first_type);
        }
    }
}

// Load chunk from disk or generate it
int chunk_load(ChunkData* chunk) {
    // This would normally load from disk or generate terrain
    // For this example, we'll just initialize it
    chunk_init(chunk, chunk->x, chunk->z);
    
    // Fill with basic terrain pattern
    for (int y = 0; y < WORLD_HEIGHT; y++) {
        BlockType type;
        
        if (y == 0) {
            type = BLOCK_BEDROCK;
        } else if (y < 50) {
            type = BLOCK_STONE;
        } else if (y < 60) {
            type = BLOCK_DIRT;
        } else if (y == 60) {
            type = BLOCK_GRASS;
        } else {
            type = BLOCK_AIR;
        }
        
        // Fill the entire layer with this block type
        int vertical_index = y / CHUNK_SIZE;
        
        // Use our optimized filling when possible
        if (vertical_index * CHUNK_SIZE == y && (vertical_index + 1) * CHUNK_SIZE - 1 < WORLD_HEIGHT) {
            // We can fill an entire vertical section
            int can_optimize = 1;
            
            // Check if the entire vertical can have the same block type
            for (int check_y = y; check_y < y + CHUNK_SIZE; check_y++) {
                BlockType check_type;
                
                if (check_y == 0) {
                    check_type = BLOCK_BEDROCK;
                } else if (check_y < 50) {
                    check_type = BLOCK_STONE;
                } else if (check_y < 60) {
                    check_type = BLOCK_DIRT;
                } else if (check_y == 60) {
                    check_type = BLOCK_GRASS;
                } else {
                    check_type = BLOCK_AIR;
                }
                
                if (check_type != type) {
                    can_optimize = 0;
                    break;
                }
            }
            
            if (can_optimize) {
                chunk_fill_vertical(chunk, vertical_index, type);
                y += CHUNK_SIZE - 1;  // Skip the rest of this vertical section
                continue;
            }
        }
        
        // Fill the entire layer with this block type
        for (int z = 0; z < CHUNK_SIZE; z++) {
            for (int x = 0; x < CHUNK_SIZE; x++) {
                chunk_set_block(chunk, 
                               x + chunk->x * CHUNK_SIZE, 
                               y, 
                               z + chunk->z * CHUNK_SIZE, 
                               type);
            }
        }
    }
    
    // Run optimization to compress uniform areas
    chunk_optimize(chunk);
    
    chunk->isLoaded = 1;
    return 1;
}

// Save chunk to disk
int chunk_save(ChunkData* chunk) {
    // This would normally save to disk
    // For this example, we'll just return success
    return chunk->isLoaded;
}