/** TRACCC library, part of the ACTS project (R&D line)
 *
 * (c) 2022 CERN for the benefit of the ACTS project
 *
 * Mozilla Public License Version 2.0
 */

// Project include(s).
#include "cluster_counting.hpp"

namespace traccc::cuda {

__global__ void cluster_counting(std::size_t *n_modules_ptr,
    vecmem::data::jagged_vector_view<unsigned int> sparse_ccl_indices_view,
    vecmem::data::vector_view<unsigned int> cluster_sizes_view,
    vecmem::data::vector_view<std::size_t> cluster_prefix_sum_view)
{   
    //auto id_x = item.get_global_id(0);
    std::size_t  id_x = threadIdx.x + blockIdx.x * blockDim.x; 
    //auto id_y = item.get_global_id(1);
    std::size_t  id_y = threadIdx.y + blockIdx.y * blockDim.y;
    
    // Ignore if id_x is out of range
    if (id_x >= *n_modules_ptr)
        return;

    // Vectors used for cluster indices found by sparse CCL
    vecmem::jagged_device_vector<unsigned int>
        device_sparse_ccl_indices(sparse_ccl_indices_view);
    const auto& cluster_indices =
        device_sparse_ccl_indices.at(id_x);

    // Ignore if id_y is out of range (more than num of cells
    // for this module)
    if (id_y >= cluster_indices.size())
        return;

    // Number of clusters that sparce_ccl found for this module
    const unsigned int n_clusters = cluster_indices.back();

    // Get the prefix sum at this id_x to know where to write
    // clusters from this module to the cluster_container
    vecmem::device_vector<std::size_t>
        device_cluster_prefix_sum(cluster_prefix_sum_view);
    const std::size_t prefix_sum =
        device_cluster_prefix_sum[id_x];

    // Vector to fill in with the sizes of each cluster
    vecmem::device_vector<unsigned int> device_cluster_sizes(
        cluster_sizes_view);

    // Count the cluster sizes for each position
    unsigned int cindex = cluster_indices[id_y] - 1;
    if (cindex < n_clusters) {
        vecmem::device_atomic_ref<unsigned int>(
            device_cluster_sizes[prefix_sum + cindex])
            .fetch_add(1);
    }



}   

void cluster_counting(
    std::size_t num_modules,
    vecmem::data::jagged_vector_view<unsigned int> sparse_ccl_indices_view,
    vecmem::data::vector_view<unsigned int> cluster_sizes_view,
    vecmem::data::vector_view<std::size_t> cluster_prefix_sum_view,
    std::size_t cells_max, vecmem::memory_resource& resource)
    {

    // X dimension of the execution grid (the Y dim is the cells_max)
    auto n_modules = vecmem::make_unique_alloc<std::size_t>(resource);
    *n_modules = num_modules;


    //CUDA kernel dementions
    auto wGroupSize = 16;
    const unsigned int numGroupsX = (*n_modules + wGroupSize - 1) / wGroupSize;
    const unsigned int numGroupsY = (cells_max + wGroupSize - 1) / wGroupSize;
    dim3 nClusterCountThreads = {wGroupSize,wGroupSize,1}; // 
    dim3 nCLusterCountBlocks =  {numGroupsX * wGroupSize,numGroupsY * wGroupSize,1};
    
    auto n_modules_ptr = n_modules.get();
     
    cluster_counting<<<nCLusterCountBlocks,nClusterCountThreads>>>(
        n_modules_ptr,sparse_ccl_indices_view,cluster_sizes_view,
        cluster_prefix_sum_view);
    
}

}  // namespace traccc::cuda