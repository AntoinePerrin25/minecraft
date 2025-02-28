#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <time.h>
#include <x86intrin.h> // For __rdtsc()
#include "../src/chunk_manager.h"
#include "../include/raylib.h"

#define BENCHMARK_ITERATIONS 1000000
#define WARM_UP_ITERATIONS 1000

typedef struct {
    unsigned long long min;
    unsigned long long max;
    double average;
    unsigned long long total;
    int iterations;
} BenchmarkResult;

BenchmarkResult run_benchmark(void (*func)(), int iterations) {
    BenchmarkResult result = {ULLONG_MAX, 0, 0, 0, iterations};
    
    // Warm-up run
    for(int i = 0; i < WARM_UP_ITERATIONS; i++) {
        func();
    }
    
    // Actual benchmark
    for(int i = 0; i < iterations; i++) {
        unsigned long long start = __rdtsc();
        func();
        unsigned long long end = __rdtsc();
        unsigned long long duration = end - start;
        
        result.total += duration;
        result.min = (duration < result.min) ? duration : result.min;
        result.max = (duration > result.max) ? duration : result.max;
    }
    
    result.average = (double)result.total / iterations;
    return result;
}

void print_benchmark(const char* name, BenchmarkResult result) {
    printf("\n=== %s ===\n", name);
    printf("Cycles - Min: %llu\n", result.min);
    printf("Cycles - Max: %llu\n", result.max);
    printf("Cycles - Avg: %.2f\n", result.average);
    printf("Total iterations: %d\n", result.iterations);
}

// Test functions
void bench_create_block() {
    volatile BlockData block = createBlock(BLOCK_GRASS, 10);
    block = setLightLevel(block, 15);
    block = setBlockID(block, BLOCK_STONE);
    block = getBlockID(block);
    block = getLightLevel(block);
}

void bench_layer_operations() {
    static ChunkLayer layer;
    for(int x = 0; x < 16; x++) {
        for(int z = 0; z < 16; z++) {
            layer.blocks[x][z] = createBlock(BLOCK_GRASS, 10);
        }
    }
}

void bench_block_access() {
    static ChunkLayer layer = {0};
    volatile BlockType type;
    volatile uint8_t light;
    
    for(int i = 0; i < 16; i++) {
        type = getBlockID(layer.blocks[i][i]);
        light = getLightLevel(layer.blocks[i][i]);
    }
}


void test_structure_sizes()
{
    printf("Memory structure sizes:\n");
    printf("BlockData: %zu bytes\n", sizeof(BlockData));
    printf("ChunkLayer: %zu bytes\n", sizeof(ChunkLayer));
    printf("ChunkVertical: %zu bytes\n", sizeof(ChunkVertical));
    printf("ChunkData: %zu bytes\n", sizeof(ChunkData));
    printf("ClientChunk: %zu bytes\n", sizeof(ClientChunk));

    // Verify bit-packing is working
    assert(sizeof(BlockData) == 2);
    assert(sizeof(ChunkLayer) == 2 * CHUNK_SIZE * CHUNK_SIZE);
}
/*
void test_chunk_memory_usage()
{
    ChunkManager manager = {0};
    manager.capacity = RENDER_DISTANCE * RENDER_DISTANCE * 4;
    manager.chunks = malloc(sizeof(ClientChunk *) * manager.capacity);

    size_t total_memory = 0;

    // Create a chunk and measure its memory
    ClientChunk *chunk = malloc(sizeof(ClientChunk));
    chunk->data = malloc(sizeof(ChunkData));
    chunk->data->isLoaded = 1;
    chunk->data->x = 0;
    chunk->data->z = 0;

    // Allocate verticals
    for (int i = 0; i < WORLD_HEIGHT; i++)
    {
        chunk->data->verticals[i] = malloc(sizeof(ChunkVertical));
        // Fill with test data
        chunk->data->verticals[i]->isOnlyBlockType = 1;
        chunk->data->verticals[i]->blockType = BLOCK_STONE;
        total_memory += sizeof(ChunkVertical);
    }

    printf("Single chunk memory usage: %zu bytes\n", total_memory + sizeof(ClientChunk) + sizeof(ChunkData));

    // Test memory for render distance
    printf("Estimated memory for render distance %d: %zu MB\n",
           RENDER_DISTANCE,
           (total_memory * manager.capacity) / (1024 * 1024));

    // Cleanup
    for (int i = 0; i < WORLD_HEIGHT; i++)
    {
        free(chunk->data->verticals[i]);
    }
    free(chunk->data);
    free(chunk);
    free(manager.chunks);
}

void test_chunk_access_speed()
{
    ChunkData *chunk = malloc(sizeof(ChunkData));
    clock_t start, end;

    // Test sequential access
    start = clock();
    for (uint8_t vertical = 0; vertical < CHUNK_SIZE; vertical++)
    { // for each vertical chunk allocate memory
        chunk->verticals[vertical] = malloc(sizeof(ChunkVertical));
        for (int layer = 0; layer < CHUNK_SIZE; layer++)
        { // for each layer of the vertical chunk
            for (int x = 0; x < CHUNK_SIZE; x++)
            {
                for (int z = 0; z < CHUNK_SIZE; z++) 
                chunk->verticals[vertical]->layers[layer].blocks[x][z].blockType = BLOCK_STONE;
            }
        }
    }
    end = clock();

    printf("Sequential chunk fill time: %f seconds\n", ((double)(end - start)) / CLOCKS_PER_SEC);

    // Cleanup
    for (int y = 0; y < WORLD_HEIGHT; y++)
    {
        free(chunk->verticals[y]);
    }
    free(chunk);
}
*/

// Modified main function
int main() {
    // Structure sizes test
    test_structure_sizes();
    
    // Precise benchmarks
    printf("\nRunning precise benchmarks...\n");
    
    BenchmarkResult create_result = run_benchmark(bench_create_block, BENCHMARK_ITERATIONS);
    print_benchmark("Block Creation and Modification", create_result);
    
    BenchmarkResult layer_result = run_benchmark(bench_layer_operations, 10000);
    print_benchmark("Layer Operations", layer_result);
    
    BenchmarkResult access_result = run_benchmark(bench_block_access, BENCHMARK_ITERATIONS);
    print_benchmark("Block Access", access_result);
    
    // Calculate memory bandwidth
    size_t layer_size = sizeof(ChunkLayer);
    double bandwidth = (layer_size * 10000.0) / (layer_result.average / 3.0e9); // Assuming 3GHz CPU
    printf("\nEstimated memory bandwidth: %.2f MB/s\n", bandwidth);
    
    return 0;
}
