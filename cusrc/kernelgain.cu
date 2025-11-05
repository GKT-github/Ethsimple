#include <cuda_runtime.h>
#include <device_launch_parameters.h>
#include <stdio.h>

/**
 * CUDA Kernel for Gain Compensation
 * Adjusts brightness/exposure across multiple camera views
 */

// Kernel to compute histogram in shared memory
__global__ void computeHistogramKernel(const unsigned char* image, 
                                       unsigned int* histogram,
                                       int width, int height, int channels) {
    __shared__ unsigned int sharedHist[256 * 3];
    
    int tid = threadIdx.x;
    
    // Initialize shared histogram
    for (int i = tid; i < 256 * channels; i += blockDim.x) {
        sharedHist[i] = 0;
    }
    __syncthreads();
    
    // Compute histogram
    int x = blockIdx.x * blockDim.x + threadIdx.x;
    int y = blockIdx.y * blockDim.y + threadIdx.y;
    
    if (x < width && y < height) {
        int idx = (y * width + x) * channels;
        for (int c = 0; c < channels; c++) {
            unsigned char val = image[idx + c];
            atomicAdd(&sharedHist[c * 256 + val], 1);
        }
    }
    __syncthreads();
    
    // Write shared histogram to global memory
    for (int i = tid; i < 256 * channels; i += blockDim.x) {
        atomicAdd(&histogram[i], sharedHist[i]);
    }
}

// Kernel to compute mean intensity per channel
__global__ void computeMeanKernel(const unsigned char* image,
                                  float* mean,
                                  const unsigned char* mask,
                                  int width, int height, int channels) {
    __shared__ float sharedSum[3];
    __shared__ int sharedCount;
    
    int tid = threadIdx.x;
    
    // Initialize shared memory
    if (tid < channels) {
        sharedSum[tid] = 0.0f;
    }
    if (tid == 0) {
        sharedCount = 0;
    }
    __syncthreads();
    
    // Compute sum
    int x = blockIdx.x * blockDim.x + threadIdx.x;
    int y = blockIdx.y * blockDim.y + threadIdx.y;
    
    if (x < width && y < height) {
        int idx = y * width + x;
        unsigned char maskVal = mask[idx];
        
        if (maskVal > 128) {  // Only count pixels in mask
            for (int c = 0; c < channels; c++) {
                atomicAdd(&sharedSum[c], (float)image[idx * channels + c]);
            }
            atomicAdd(&sharedCount, 1);
        }
    }
    __syncthreads();
    
    // Write to global memory
    if (tid < channels) {
        atomicAdd(&mean[tid], sharedSum[tid]);
    }
    if (tid == 0) {
        atomicAdd((int*)&mean[channels], sharedCount);
    }
}

// Kernel to apply gain compensation
__global__ void applyGainKernel(const unsigned char* src,
                                unsigned char* dst,
                                const float* gains,
                                int width, int height, int channels) {
    int x = blockIdx.x * blockDim.x + threadIdx.x;
    int y = blockIdx.y * blockDim.y + threadIdx.y;
    
    if (x >= width || y >= height) return;
    
    int idx = (y * width + x) * channels;
    
    for (int c = 0; c < channels; c++) {
        float val = (float)src[idx + c] * gains[c];
        dst[idx + c] = (unsigned char)fminf(255.0f, fmaxf(0.0f, val));
    }
}

// Kernel to apply gain compensation to 16-bit signed image
__global__ void applyGain16sKernel(const short* src,
                                   short* dst,
                                   const float* gains,
                                   int width, int height, int channels) {
    int x = blockIdx.x * blockDim.x + threadIdx.x;
    int y = blockIdx.y * blockDim.y + threadIdx.y;
    
    if (x >= width || y >= height) return;
    
    int idx = (y * width + x) * channels;
    
    for (int c = 0; c < channels; c++) {
        float val = (float)src[idx + c] * gains[c];
        dst[idx + c] = (short)fminf(32767.0f, fmaxf(-32768.0f, val));
    }
}

// Kernel to compute gain ratios between overlapping regions
__global__ void computeOverlapGainKernel(const unsigned char* img1,
                                         const unsigned char* img2,
                                         const unsigned char* mask,
                                         float* gain,
                                         int width, int height, int channels) {
    __shared__ float sharedSum1[3];
    __shared__ float sharedSum2[3];
    __shared__ int sharedCount;
    
    int tid = threadIdx.x;
    
    // Initialize shared memory
    if (tid < channels) {
        sharedSum1[tid] = 0.0f;
        sharedSum2[tid] = 0.0f;
    }
    if (tid == 0) {
        sharedCount = 0;
    }
    __syncthreads();
    
    // Compute sums in overlap region
    int x = blockIdx.x * blockDim.x + threadIdx.x;
    int y = blockIdx.y * blockDim.y + threadIdx.y;
    
    if (x < width && y < height) {
        int idx = y * width + x;
        unsigned char maskVal = mask[idx];
        
        if (maskVal > 128) {  // Overlap region
            for (int c = 0; c < channels; c++) {
                atomicAdd(&sharedSum1[c], (float)img1[idx * channels + c]);
                atomicAdd(&sharedSum2[c], (float)img2[idx * channels + c]);
            }
            atomicAdd(&sharedCount, 1);
        }
    }
    __syncthreads();
    
    // Compute gain ratios and write to global memory
    if (tid < channels) {
        atomicAdd(&gain[tid * 2], sharedSum1[tid]);
        atomicAdd(&gain[tid * 2 + 1], sharedSum2[tid]);
    }
    if (tid == 0) {
        atomicAdd((int*)&gain[channels * 2], sharedCount);
    }
}

// Host function to compute histogram
extern "C"
void cudaComputeHistogram(const unsigned char* d_image,
                          unsigned int* d_histogram,
                          int width, int height, int channels,
                          cudaStream_t stream) {
    dim3 block(16, 16);
    dim3 grid((width + block.x - 1) / block.x,
              (height + block.y - 1) / block.y);
    
    // Clear histogram
    cudaMemsetAsync(d_histogram, 0, 256 * channels * sizeof(unsigned int), stream);
    
    computeHistogramKernel<<<grid, block, 0, stream>>>(
        d_image, d_histogram, width, height, channels
    );
}

// Host function to compute mean
extern "C"
void cudaComputeMean(const unsigned char* d_image,
                     float* d_mean,
                     const unsigned char* d_mask,
                     int width, int height, int channels,
                     cudaStream_t stream) {
    dim3 block(16, 16);
    dim3 grid((width + block.x - 1) / block.x,
              (height + block.y - 1) / block.y);
    
    // Clear mean array
    cudaMemsetAsync(d_mean, 0, (channels + 1) * sizeof(float), stream);
    
    computeMeanKernel<<<grid, block, 0, stream>>>(
        d_image, d_mean, d_mask, width, height, channels
    );
}

// Host function to apply gain
extern "C"
void cudaApplyGain(const unsigned char* d_src,
                   unsigned char* d_dst,
                   const float* d_gains,
                   int width, int height, int channels,
                   cudaStream_t stream) {
    dim3 block(16, 16);
    dim3 grid((width + block.x - 1) / block.x,
              (height + block.y - 1) / block.y);
    
    applyGainKernel<<<grid, block, 0, stream>>>(
        d_src, d_dst, d_gains, width, height, channels
    );
}

// Host function to apply gain to 16-bit signed image
extern "C"
void cudaApplyGain16s(const short* d_src,
                      short* d_dst,
                      const float* d_gains,
                      int width, int height, int channels,
                      cudaStream_t stream) {
    dim3 block(16, 16);
    dim3 grid((width + block.x - 1) / block.x,
              (height + block.y - 1) / block.y);
    
    applyGain16sKernel<<<grid, block, 0, stream>>>(
        d_src, d_dst, d_gains, width, height, channels
    );
}

// Host function to compute overlap gain
extern "C"
void cudaComputeOverlapGain(const unsigned char* d_img1,
                            const unsigned char* d_img2,
                            const unsigned char* d_mask,
                            float* d_gain,
                            int width, int height, int channels,
                            cudaStream_t stream) {
    dim3 block(16, 16);
    dim3 grid((width + block.x - 1) / block.x,
              (height + block.y - 1) / block.y);
    
    // Clear gain array
    cudaMemsetAsync(d_gain, 0, (channels * 2 + 1) * sizeof(float), stream);
    
    computeOverlapGainKernel<<<grid, block, 0, stream>>>(
        d_img1, d_img2, d_mask, d_gain, width, height, channels
    );
}
