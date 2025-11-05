#include <cuda_runtime.h>
#include <device_launch_parameters.h>
#include <stdio.h>

/**
 * CUDA Kernel for Multi-Band Blending
 * Performs Laplacian pyramid blending on GPU
 */

// Kernel to downscale image (Gaussian pyramid level)
__global__ void downscaleKernel(const short* src, short* dst, 
                                int srcWidth, int srcHeight,
                                int dstWidth, int dstHeight,
                                int channels) {
    int x = blockIdx.x * blockDim.x + threadIdx.x;
    int y = blockIdx.y * blockDim.y + threadIdx.y;
    
    if (x >= dstWidth || y >= dstHeight) return;
    
    int srcX = x * 2;
    int srcY = y * 2;
    
    for (int c = 0; c < channels; c++) {
        if (srcX + 1 < srcWidth && srcY + 1 < srcHeight) {
            short val00 = src[(srcY * srcWidth + srcX) * channels + c];
            short val01 = src[(srcY * srcWidth + srcX + 1) * channels + c];
            short val10 = src[((srcY + 1) * srcWidth + srcX) * channels + c];
            short val11 = src[((srcY + 1) * srcWidth + srcX + 1) * channels + c];
            
            // Simple average
            dst[(y * dstWidth + x) * channels + c] = (val00 + val01 + val10 + val11) / 4;
        } else {
            dst[(y * dstWidth + x) * channels + c] = 0;
        }
    }
}

// Kernel to upscale image
__global__ void upscaleKernel(const short* src, short* dst,
                              int srcWidth, int srcHeight,
                              int dstWidth, int dstHeight,
                              int channels) {
    int x = blockIdx.x * blockDim.x + threadIdx.x;
    int y = blockIdx.y * blockDim.y + threadIdx.y;
    
    if (x >= dstWidth || y >= dstHeight) return;
    
    int srcX = x / 2;
    int srcY = y / 2;
    
    if (srcX < srcWidth && srcY < srcHeight) {
        for (int c = 0; c < channels; c++) {
            dst[(y * dstWidth + x) * channels + c] = 
                src[(srcY * srcWidth + srcX) * channels + c];
        }
    } else {
        for (int c = 0; c < channels; c++) {
            dst[(y * dstWidth + x) * channels + c] = 0;
        }
    }
}

// Kernel to compute Laplacian (difference between levels)
__global__ void laplacianKernel(const short* gaussian, const short* upsampled,
                                short* laplacian, int width, int height, int channels) {
    int x = blockIdx.x * blockDim.x + threadIdx.x;
    int y = blockIdx.y * blockDim.y + threadIdx.y;
    
    if (x >= width || y >= height) return;
    
    int idx = y * width + x;
    for (int c = 0; c < channels; c++) {
        laplacian[idx * channels + c] = 
            gaussian[idx * channels + c] - upsampled[idx * channels + c];
    }
}

// Kernel to blend two Laplacian pyramids using mask
__global__ void blendLaplacianKernel(const short* lap1, const short* lap2,
                                     const unsigned char* mask1, const unsigned char* mask2,
                                     short* result, int width, int height, int channels) {
    int x = blockIdx.x * blockDim.x + threadIdx.x;
    int y = blockIdx.y * blockDim.y + threadIdx.y;
    
    if (x >= width || y >= height) return;
    
    int idx = y * width + x;
    float w1 = mask1[idx] / 255.0f;
    float w2 = mask2[idx] / 255.0f;
    float wsum = w1 + w2;
    
    if (wsum > 0.001f) {
        w1 /= wsum;
        w2 /= wsum;
        
        for (int c = 0; c < channels; c++) {
            result[idx * channels + c] = (short)(
                lap1[idx * channels + c] * w1 + 
                lap2[idx * channels + c] * w2
            );
        }
    } else {
        for (int c = 0; c < channels; c++) {
            result[idx * channels + c] = 0;
        }
    }
}

// Kernel to add two images
__global__ void addImagesKernel(const short* src1, const short* src2,
                                short* dst, int width, int height, int channels) {
    int x = blockIdx.x * blockDim.x + threadIdx.x;
    int y = blockIdx.y * blockDim.y + threadIdx.y;
    
    if (x >= width || y >= height) return;
    
    int idx = y * width + x;
    for (int c = 0; c < channels; c++) {
        dst[idx * channels + c] = src1[idx * channels + c] + src2[idx * channels + c];
    }
}

// Kernel to convert short to byte with clamping
__global__ void convertToByteKernel(const short* src, unsigned char* dst,
                                    int width, int height, int channels) {
    int x = blockIdx.x * blockDim.x + threadIdx.x;
    int y = blockIdx.y * blockDim.y + threadIdx.y;
    
    if (x >= width || y >= height) return;
    
    int idx = y * width + x;
    for (int c = 0; c < channels; c++) {
        int val = src[idx * channels + c];
        dst[idx * channels + c] = (unsigned char)max(0, min(255, val));
    }
}

// Host function to launch downscale kernel
extern "C"
void cudaDownscale(const short* d_src, short* d_dst,
                   int srcWidth, int srcHeight,
                   int dstWidth, int dstHeight,
                   int channels, cudaStream_t stream) {
    dim3 block(16, 16);
    dim3 grid((dstWidth + block.x - 1) / block.x,
              (dstHeight + block.y - 1) / block.y);
    
    downscaleKernel<<<grid, block, 0, stream>>>(
        d_src, d_dst, srcWidth, srcHeight, dstWidth, dstHeight, channels
    );
}

// Host function to launch upscale kernel
extern "C"
void cudaUpscale(const short* d_src, short* d_dst,
                 int srcWidth, int srcHeight,
                 int dstWidth, int dstHeight,
                 int channels, cudaStream_t stream) {
    dim3 block(16, 16);
    dim3 grid((dstWidth + block.x - 1) / block.x,
              (dstHeight + block.y - 1) / block.y);
    
    upscaleKernel<<<grid, block, 0, stream>>>(
        d_src, d_dst, srcWidth, srcHeight, dstWidth, dstHeight, channels
    );
}

// Host function to launch Laplacian kernel
extern "C"
void cudaLaplacian(const short* d_gaussian, const short* d_upsampled,
                   short* d_laplacian, int width, int height, int channels,
                   cudaStream_t stream) {
    dim3 block(16, 16);
    dim3 grid((width + block.x - 1) / block.x,
              (height + block.y - 1) / block.y);
    
    laplacianKernel<<<grid, block, 0, stream>>>(
        d_gaussian, d_upsampled, d_laplacian, width, height, channels
    );
}

// Host function to launch blend kernel
extern "C"
void cudaBlendLaplacian(const short* d_lap1, const short* d_lap2,
                        const unsigned char* d_mask1, const unsigned char* d_mask2,
                        short* d_result, int width, int height, int channels,
                        cudaStream_t stream) {
    dim3 block(16, 16);
    dim3 grid((width + block.x - 1) / block.x,
              (height + block.y - 1) / block.y);
    
    blendLaplacianKernel<<<grid, block, 0, stream>>>(
        d_lap1, d_lap2, d_mask1, d_mask2, d_result, width, height, channels
    );
}

// Host function to launch add images kernel
extern "C"
void cudaAddImages(const short* d_src1, const short* d_src2,
                   short* d_dst, int width, int height, int channels,
                   cudaStream_t stream) {
    dim3 block(16, 16);
    dim3 grid((width + block.x - 1) / block.x,
              (height + block.y - 1) / block.y);
    
    addImagesKernel<<<grid, block, 0, stream>>>(
        d_src1, d_src2, d_dst, width, height, channels
    );
}

// Host function to launch convert to byte kernel
extern "C"
void cudaConvertToByte(const short* d_src, unsigned char* d_dst,
                       int width, int height, int channels,
                       cudaStream_t stream) {
    dim3 block(16, 16);
    dim3 grid((width + block.x - 1) / block.x,
              (height + block.y - 1) / block.y);
    
    convertToByteKernel<<<grid, block, 0, stream>>>(
        d_src, d_dst, width, height, channels
    );
}
