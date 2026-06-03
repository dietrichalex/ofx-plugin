#include <cuda_runtime.h>
#include <cstdint>
#include <cmath>

// --- Device Helpers ---

__device__ inline uint32_t hashCell(int cx, int cy, uint32_t seedOffset) {
    uint32_t h = uint32_t(cx) * 73856093 ^ uint32_t(cy) * 19349663 ^ seedOffset;
    h ^= h >> 16; h *= 0x85ebca6b; h ^= h >> 13; h *= 0xc2b2ae35; h ^= h >> 16;
    return h;
}

__device__ inline float hashToFloat(uint32_t& state) {
    state = state * 747796405u + 2891336453u; 
    return (state >> 8) * 0x1.0p-24f;
}

__device__ inline int samplePoisson(uint32_t& state, float lambda) {
    float L = expf(-lambda);
    int k = 0;
    float p = 1.0f;
    do {
        k++;
        p *= hashToFloat(state);
    } while (p > L);
    return k - 1;
}

__device__ inline float clampF(float val, float minV, float maxV) {
    return fminf(fmaxf(val, minV), maxV);
}

// --- The Kernel ---

__global__ void filmGrainKernel(
    const float* src, 
    float* dst, 
    int width, 
    int height, 
    float muR, 
    int N, 
    float blendIntensity, 
    uint32_t frameTime) 
{
    int x = blockIdx.x * blockDim.x + threadIdx.x;
    int y = blockIdx.y * blockDim.y + threadIdx.y;

    if (x >= width || y >= height) return;

    int idx = (y * width + x) * 4;
    float srcR = src[idx];
    float srcG = src[idx + 1];
    float srcB = src[idx + 2];
    float srcA = src[idx + 3];

    float cellSize = 1.0f / ceilf(1.0f / muR);
    float hitCount = 0.0f;
    
    // Simplistic MC distribution generation for the kernel
    uint32_t mcState = frameTime * 1000;
    
    for (int k = 0; k < N; ++k) {
        float xi_x = (hashToFloat(mcState) - 0.5f) * 1.6f; 
        float xi_y = (hashToFloat(mcState) - 0.5f) * 1.6f; 

        float px = (float)x + xi_x;
        float py = (float)y + xi_y;

        int minCx = floorf((px - muR) / cellSize);
        int maxCx = floorf((px + muR) / cellSize);
        int minCy = floorf((py - muR) / cellSize);
        int maxCy = floorf((py + muR) / cellSize);

        bool ptCovered = false;

        for (int cy = minCy; cy <= maxCy && !ptCovered; ++cy) {
            for (int cx = minCx; cx <= maxCx && !ptCovered; ++cx) {
                
                int fetchX = max(0, min(width - 1, (int)(cx * cellSize)));
                int fetchY = max(0, min(height - 1, (int)(cy * cellSize)));
                int fetchIdx = (fetchY * width + fetchX) * 4;
                
                float u = 0.2126f * src[fetchIdx] + 0.7152f * src[fetchIdx + 1] + 0.0722f * src[fetchIdx + 2];
                u = clampF(u, 0.0f, 0.99f); 
                
                float lambda = -((cellSize * cellSize) / (3.14159f * (muR * muR))) * logf(1.0f - u);
                
                if (lambda <= 0.00001f) continue;

                uint32_t cellSeed = hashCell(cx, cy, frameTime);
                int nGrains = samplePoisson(cellSeed, lambda);
                
                for (int g = 0; g < nGrains; ++g) {
                    float gx = (cx * cellSize) + (hashToFloat(cellSeed) * cellSize);
                    float gy = (cy * cellSize) + (hashToFloat(cellSeed) * cellSize);
                    
                    float dx = gx - px;
                    float dy = gy - py;
                    
                    if ((dx*dx + dy*dy) < (muR * muR)) {
                        hitCount += 1.0f;
                        ptCovered = true;
                        break;
                    }
                }
            }
        }
    }

    float simLuma = hitCount / (float)N;
    float origLuma = 0.2126f * srcR + 0.7152f * srcG + 0.0722f * srcB;
    float densityRatio = simLuma / (origLuma + 0.0001f);
    float finalModulation = 1.0f + ((densityRatio - 1.0f) * blendIntensity);

    dst[idx] = clampF(srcR * finalModulation, 0.0f, 1.0f);
    dst[idx + 1] = clampF(srcG * finalModulation, 0.0f, 1.0f);
    dst[idx + 2] = clampF(srcB * finalModulation, 0.0f, 1.0f);
    dst[idx + 3] = srcA; 
}

// --- Host Wrapper ---

extern "C" void RunCudaGrainKernel(
    const float* d_src, float* d_dst, int width, int height, 
    float muR, int N, float blendIntensity, uint32_t frameTime) 
{
    // Define 16x16 thread blocks
    dim3 blockSize(16, 16);
    dim3 gridSize((width + blockSize.x - 1) / blockSize.x, 
                  (height + blockSize.y - 1) / blockSize.y);

    filmGrainKernel<<<gridSize, blockSize>>>(d_src, d_dst, width, height, muR, N, blendIntensity, frameTime);
    
    // Synchronize to catch errors during development
    cudaDeviceSynchronize();
}