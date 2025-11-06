#include <cuda_runtime.h>
#include <device_launch_parameters.h>
#include <opencv2/core/cuda/common.hpp>

// Basic feed kernel - copy image and mask with offset
__global__ void feedKernel(const uchar* img, const uchar* mask, uchar* dst, uchar* dst_mask,
                           int dx, int dy, int width, int height, 
                           int img_step, int dst_step, int mask_step, int mask_dst_step) {
    int x = blockIdx.x * blockDim.x + threadIdx.x;
    int y = blockIdx.y * blockDim.y + threadIdx.y;
    
    if (x >= width || y >= height) return;
    
    int dst_x = x + dx;
    int dst_y = y + dy;
    
    // Copy 3-channel image
    for (int c = 0; c < 3; c++) {
        dst[(dst_y * dst_step + dst_x * 3) + c] = img[(y * img_step + x * 3) + c];
    }
    
    // Copy mask
    dst_mask[dst_y * mask_dst_step + dst_x] = mask[y * mask_step + x];
}

// Weight blend kernel
__global__ void weightBlendKernel(const cv::cuda::PtrStep<short> src, 
                                  const cv::cuda::PtrStepf src_weight,
                                  cv::cuda::PtrStep<short> dst, 
                                  cv::cuda::PtrStepf dst_weight,
                                  int width, int height, int dx, int dy) {
    int x = blockIdx.x * blockDim.x + threadIdx.x;
    int y = blockIdx.y * blockDim.y + threadIdx.y;
    
    if (x >= width || y >= height) return;
    
    int dst_x = x + dx;
    int dst_y = y + dy;
    
    float weight = src_weight(y, x);
    
    for (int c = 0; c < 3; c++) {
        float weighted_val = src(y, x * 3 + c) * weight;
        atomicAdd((int*)&dst(dst_y, dst_x * 3 + c), (int)weighted_val);
    }
    
    atomicAdd((float*)&dst_weight(dst_y, dst_x), weight);
}

// Add source with weight kernel
__global__ void addSrcWeightKernel(const cv::cuda::PtrStep<short> src,
                                   const cv::cuda::PtrStepf src_weight,
                                   cv::cuda::PtrStep<short> dst,
                                   cv::cuda::PtrStepf dst_weight,
                                   int x_offset, int y_offset,
                                   int width, int height) {
    int x = blockIdx.x * blockDim.x + threadIdx.x;
    int y = blockIdx.y * blockDim.y + threadIdx.y;
    
    if (x >= width || y >= height) return;
    
    int dst_x = x + x_offset;
    int dst_y = y + y_offset;
    
    float weight = src_weight(y, x);
    
    for (int c = 0; c < 3; c++) {
        short val = src(y, x * 3 + c);
        atomicAdd((int*)&dst(dst_y, dst_x * 3 + c), (int)(val * weight));
    }
    
    atomicAdd((float*)&dst_weight(dst_y, dst_x), weight);
}

// Normalize using weight map kernel
__global__ void normalizeKernel(const cv::cuda::PtrStepf weight, 
                                cv::cuda::PtrStep<short> src,
                                int width, int height) {
    int x = blockIdx.x * blockDim.x + threadIdx.x;
    int y = blockIdx.y * blockDim.y + threadIdx.y;
    
    if (x >= width || y >= height) return;
    
    float w = weight(y, x);
    
    if (w > 1e-5f) {
        for (int c = 0; c < 3; c++) {
            src(y, x * 3 + c) = (short)(src(y, x * 3 + c) / w);
        }
    }
}

// Host functions
extern "C" {

void feedCUDA(uchar* img, uchar* mask, uchar* dst, uchar* dst_mask,
              int dx, int dy, int width, int height,
              int img_step, int dst_step, int mask_step, int mask_dst_step) {
    
    dim3 block(16, 16);
    dim3 grid((width + block.x - 1) / block.x, (height + block.y - 1) / block.y);
    
    feedKernel<<<grid, block>>>(img, mask, dst, dst_mask, dx, dy, width, height,
                                img_step, dst_step, mask_step, mask_dst_step);
    cudaDeviceSynchronize();
}

void feedCUDA_Async(uchar* img, uchar* mask, uchar* dst, uchar* dst_mask,
                    int dx, int dy, int width, int height,
                    int img_step, int dst_step, int mask_step, int mask_dst_step,
                    cudaStream_t streamimage, cudaStream_t streammask) {
    
    dim3 block(16, 16);
    dim3 grid((width + block.x - 1) / block.x, (height + block.y - 1) / block.y);
    
    feedKernel<<<grid, block, 0, streamimage>>>(img, mask, dst, dst_mask, dx, dy, 
                                                 width, height, img_step, dst_step, 
                                                 mask_step, mask_dst_step);
}

void weightBlendCUDA(const cv::cuda::PtrStep<short> src, const cv::cuda::PtrStepf src_weight,
                     cv::cuda::PtrStep<short> dst, cv::cuda::PtrStepf dst_weight,
                     int width, int height, int dx, int dy) {
    
    dim3 block(16, 16);
    dim3 grid((width + block.x - 1) / block.x, (height + block.y - 1) / block.y);
    
    weightBlendKernel<<<grid, block>>>(src, src_weight, dst, dst_weight,
                                       width, height, dx, dy);
    cudaDeviceSynchronize();
}

void weightBlendCUDA_Async(const cv::cuda::PtrStep<short> src, const cv::cuda::PtrStepf src_weight,
                           cv::cuda::PtrStep<short> dst, cv::cuda::PtrStepf dst_weight,
                           int width, int height, int dx, int dy,
                           cudaStream_t stream_dst) {
    
    dim3 block(16, 16);
    dim3 grid((width + block.x - 1) / block.x, (height + block.y - 1) / block.y);
    
    weightBlendKernel<<<grid, block, 0, stream_dst>>>(src, src_weight, dst, dst_weight,
                                                       width, height, dx, dy);
}

void addSrcWeightGpu32F(const cv::cuda::PtrStep<short> src, const cv::cuda::PtrStepf src_weight,
                        cv::cuda::PtrStep<short> dst, cv::cuda::PtrStepf dst_weight,
                        int x_offset, int y_offset, int width, int height) {
    
    dim3 block(16, 16);
    dim3 grid((width + block.x - 1) / block.x, (height + block.y - 1) / block.y);
    
    addSrcWeightKernel<<<grid, block>>>(src, src_weight, dst, dst_weight,
                                        x_offset, y_offset, width, height);
    cudaDeviceSynchronize();
}

void addSrcWeightGpu32F_Async(const cv::cuda::PtrStep<short> src, const cv::cuda::PtrStepf src_weight,
                              cv::cuda::PtrStep<short> dst, cv::cuda::PtrStepf dst_weight,
                              int x_offset, int y_offset, int width, int height,
                              cudaStream_t stream_dst) {
    
    dim3 block(16, 16);
    dim3 grid((width + block.x - 1) / block.x, (height + block.y - 1) / block.y);
    
    addSrcWeightKernel<<<grid, block, 0, stream_dst>>>(src, src_weight, dst, dst_weight,
                                                        x_offset, y_offset, width, height);
}

void normalizeUsingWeightMapGpu32F(const cv::cuda::PtrStepf weight, cv::cuda::PtrStep<short> src,
                                   const int width, const int height) {
    
    dim3 block(16, 16);
    dim3 grid((width + block.x - 1) / block.x, (height + block.y - 1) / block.y);
    
    normalizeKernel<<<grid, block>>>(weight, src, width, height);
    cudaDeviceSynchronize();
}

void normalizeUsingWeightMapGpu32F_Async(const cv::cuda::PtrStepf weight, cv::cuda::PtrStep<short> src,
                                         const int width, const int height, cudaStream_t stream_src) {
    
    dim3 block(16, 16);
    dim3 grid((width + block.x - 1) / block.x, (height + block.y - 1) / block.y);
    
    normalizeKernel<<<grid, block, 0, stream_src>>>(weight, src, width, height);
}

} // extern "C"