/** TRACCC library, part of the ACTS project (R&D line)
 *
 * (c) 2022 CERN for the benefit of the ACTS project
 *
 * Mozilla Public License Version 2.0
 */


// Project include(s).
#include "component_connection.hpp"

namespace traccc::cuda {

namespace kernel{
__global__ 
void component_connection(
    cell_container_types::const_view cells_view,
    vecmem::data::vector_view<std::size_t> cluster_prefix_sum_view,
    cluster_container_types::view clusters_view,
    vecmem::data::jagged_vector_view<unsigned int> sparse_ccl_indices_view)
    {
        auto id_x = threadIdx.x + blockDim.x * blockIdx.x;
        auto id_y = threadIdx.y + blockDim.y * blockIdx.y;

        // Initialize the data on the device
        cell_container_types::const_device cells_device(cells_view);
        cluster_container_types::device clusters_device(clusters_view);

        // Ignore if id_x is out of range
        if (id_x >= cells_device.size())
            return;

        // Get the cells from the current module
        const auto& cells = cells_device.at(id_x).items;
        const auto module = cells_device.at(id_x).header;

        // Ignore if id_y is out of range
        if (id_y >= cells.size())
            return;

        // Vectors used for cluster indices found by sparse CCL
        vecmem::jagged_device_vector<unsigned int>
            device_sparse_ccl_indices(sparse_ccl_indices_view);
        const auto& cluster_indices =
            device_sparse_ccl_indices.at(id_x);

        // Number of clusters found for this module
        const auto num_clusters = cluster_indices.back();

        // Get the prefix sum for this id_x
        vecmem::device_vector<std::size_t> device_cluster_prefix_sum(
            cluster_prefix_sum_view);
        const auto prefix_sum = device_cluster_prefix_sum[id_x];

        // Push back the cells to the correct item vector
        unsigned int cindex = cluster_indices[id_y] - 1;
        if (cindex < num_clusters) {
            // Create cluster id - same for all clusters in this module
            cluster_id cl_id{};
            cl_id.module = module.module;
            cl_id.placement = module.placement;
            cl_id.module_idx = id_x;
            cl_id.pixel = module.pixel;

            // Push back the header and items
            clusters_device[prefix_sum + cindex].header = cl_id;
            clusters_device[prefix_sum + cindex].items.push_back(
                cells[id_y]);
    }
}
}


void component_connection(
    cluster_container_types::view clusters_view,
    const cell_container_types::host& cells_per_event,
    vecmem::data::jagged_vector_view<unsigned int> sparse_ccl_indices_view,
    vecmem::data::vector_view<std::size_t> cluster_prefix_sum_view,
    std::size_t cells_max, vecmem::memory_resource& resource) {

    // Execution size of the algorithm
    std::size_t n_modules = cells_per_event.size();

    // Calculate the 2-dim NDrange for the kernel
    auto wGroupSize = 16;
    auto numGroupsX = (n_modules + wGroupSize - 1) / wGroupSize;
    auto numGroupsY = (cells_max + wGroupSize - 1) / wGroupSize;

    dim3 nComponentConThreads   = {wGroupSize, wGroupSize, 1};
    dim3 nComponentConBlocks    = {numGroupsX * wGroupSize,
        numGroupsY * wGroupSize, 1}; //HERE NO NEED to * by GROUPSIZE check other kernels and fix them as well

    // Get the view of the cells container
    auto cells_data = get_data(cells_per_event, &resource);
    cell_container_types::const_view cells_view(cells_data);

    kernel::component_connection<<<nComponentConBlocks,nComponentConThreads>>>(
        cells_view,cluster_prefix_sum_view,
        clusters_view,sparse_ccl_indices_view);

}

}  // namespace traccc::cuda