#include <cstdio>
#include <cmath>
#include <iostream>

struct ChunkCoord {
    int x, z;
};

struct PerlinChunk {
    ChunkCoord coord;
    float noise[16][16];
};

__device__ float smoothNoise(int x, int z) {
    int n = x + z * 57;
    n = (n << 13) ^ n;
    return (1.0f - ((n * (n * n * 15731 + 789221) + 1376312589) & 0x7fffffff) / 1073741824.0f);
}

__device__ float interpolate(float a, float b, float t) {
    float ft = t * 3.14159265f;
    float f = (1.0f - cosf(ft)) * 0.5f;
    return a * (1.0f - f) + b * f;
}

__device__ float sampleNoise(float x, float z) {
    int ix = (int)floor(x);
    int iz = (int)floor(z);
    float fx = x - ix;
    float fz = z - iz;

    float v1 = smoothNoise(ix, iz);
    float v2 = smoothNoise(ix + 1, iz);
    float v3 = smoothNoise(ix,     iz + 1);
    float v4 = smoothNoise(ix + 1, iz + 1);

    float i1 = interpolate(v1, v2, fx);
    float i2 = interpolate(v3, v4, fx);

    return interpolate(i1, i2, fz);
}

__global__ void generateNoiseMap(PerlinChunk* chunks) {
    int blockX = blockIdx.x;
    int blockZ = blockIdx.y;
    PerlinChunk* chunk = &chunks[blockX * gridDim.y + blockZ];
    ChunkCoord coords = chunk->coord;

    int x = threadIdx.x;
    int z = threadIdx.y;

    float wx = (coords.x * 16 + x) * 0.03f;
    float wz = (coords.z * 16 + z) * 0.03f;

    float noise = sampleNoise(wx, wz);

    chunk->noise[x][z] = noise;
}

// 0.050 ms for 20 x 20 chunks

extern "C" void launchNoiseMap(int x, int z, PerlinChunk* chunks) {
    PerlinChunk* deviceData;
    size_t bytes = x * z * sizeof(PerlinChunk);

    cudaEvent_t start, stop;
    cudaEventCreate(&start);
    cudaEventCreate(&stop);

    cudaMalloc(&deviceData, bytes);
    cudaMemcpy(deviceData, chunks, bytes, cudaMemcpyHostToDevice);

    dim3 gridSize(x, z);
    dim3 blockSize(16, 16);

    cudaEventRecord(start);
    generateNoiseMap<<<gridSize, blockSize>>>(deviceData);
    cudaEventRecord(stop);

    cudaEventSynchronize(stop);

    float kernelMs = 0;
    cudaEventElapsedTime(&kernelMs, start, stop);

    cudaMemcpy(chunks, deviceData, bytes, cudaMemcpyDeviceToHost);
    cudaFree(deviceData);

    cudaEventDestroy(start);
    cudaEventDestroy(stop);
}