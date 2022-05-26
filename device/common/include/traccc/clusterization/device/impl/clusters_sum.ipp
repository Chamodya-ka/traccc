#pragma once



namespace traccc::device {

//My side note
//A function that gets
//some other kernel callthis this kernel

TRACCC_HOST_DEVICE 
void clusters_sum(
    std::size_t idx, cell_container_types::const_view cells_view,
    vecmem::data::jagged_vector_view<unsigned int> sparse_ccl_indices_view,
    unsigned int& total_clusters,
    vecmem::data::vector_view<std::size_t> cluster_prefix_sum_view,
    vecmem::data::vector_view<std::size_t> clusters_per_module_view)
    {

        // Get the global index
        //auto idx = item.get_global_linear_id();

        // Initialize the data on the device
        cell_container_types::const_device cells_device(cells_view);

        // Ignore if idx is out of range
        if (idx < cells_device.size()) {

            // Get the cells from the current module
            const auto& cells = cells_device.at(idx).items;

            // Number of clusters that sparce_ccl will find for this
            // module
            unsigned int n_clusters = 0;

            // Vectors used for cluster indices found by sparse CCL
            vecmem::jagged_device_vector<unsigned int>
                device_sparse_ccl_indices(sparse_ccl_indices_view);
            auto cluster_indices =
                device_sparse_ccl_indices.at(idx);

            // Run the sparse_ccl algorithm
            detail::sparse_ccl(cells, cluster_indices, n_clusters);

            // Save the number of clusters found in this module at
            // the last, extra place in the indices vectors
            cluster_indices.back() = n_clusters;

            auto prefix_sum =
                vecmem::device_atomic_ref<unsigned int>(
                    *total_clusters)
                    .fetch_add(n_clusters);

            // Save the current prefix sum at a correct index in a
            // vector
            vecmem::device_vector<std::size_t>
                device_cluster_prefix_sum(cluster_prefix_sum_view);
            device_cluster_prefix_sum[idx] = prefix_sum;

            // At last, fill also the "number of clusters per
            // module" for measurement creation buffer
            vecmem::device_vector<std::size_t>
                device_clusters_per_module(
                    clusters_per_module_view);
            device_clusters_per_module[idx] = n_clusters;
        }
            
  
    }



}