#pragma once
namespace traccc::cuda {

void init_cuda() {
    cudaFree(0);
}

}  // namespace traccc::cuda
