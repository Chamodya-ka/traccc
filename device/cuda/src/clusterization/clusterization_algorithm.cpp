/** TRACCC library, part of the ACTS project (R&D line)
 *
 * (c) 2022 CERN for the benefit of the ACTS project
 *
 * Mozilla Public License Version 2.0
 */

// Clusterization include(s)
#include "traccc/cuda/clusterization/clusterization_algorithm.hpp"

#include "traccc/device/get_prefix_sum.hpp"

// CUDA library include(s).
#include "cluster_counting.hpp"
#include "clusters_sum.hpp"
#include "component_connection.hpp"
#include "measurements_creation.hpp"
#include "spacepoint_formation.hpp"

// Vecmem include(s).
#include <vecmem/utils/copy.hpp>
#include <algorithm>

#include <iostream>
namespace traccc::cuda {

clusterization_algorithm::clusterization_algorithm(vecmem::memory_resource &mr)
    : m_mr(mr) {}

clusterization_algorithm::output_type clusterization_algorithm::operator()(
    const cell_container_types::host &cells_per_event) const {

    // Number of modules
    unsigned int num_modules = cells_per_event.size();

    // Vecmem copy object for moving the data between host and device
    vecmem::copy copy;

    // Get the view of the cells container
    auto cells_data = get_data(cells_per_event, &m_mr.get());
    cell_container_types::const_view cells_view(cells_data);

    // Get the sizes of the cells in each module
    auto cell_sizes = copy.get_sizes(cells_view.items);
    printf("num_modules : %ld\n",num_modules);
    printf("cell_sizes : %ld\n",cell_sizes);
    
    // Get the cell sizes with +1 in each entry for sparse ccl indices buffer
    // The +1 is needed to store the number of found clusters at the end of
    // the vector in each module
    std::vector<std::size_t> cell_sizes_plus(num_modules);
    std::transform(cell_sizes.begin(), cell_sizes.end(),
                   cell_sizes_plus.begin(),
                   [](std::size_t x) { return x + 1; });

    // Helper container for sparse CCL calculations
    // Constructor from a vector of ("inner vector") sizes
    vecmem::data::jagged_vector_buffer<unsigned int> sparse_ccl_indices(
        cell_sizes_plus, m_mr.get());
    copy.setup(sparse_ccl_indices);

    // Vector buffer for prefix sums for proper indexing, used only on device.
    // Vector with "clusters per module" is needed for measurement creation
    // buffer
    vecmem::data::vector_buffer<std::size_t> cluster_prefix_sum(num_modules,
                                                                m_mr.get());
    vecmem::data::vector_buffer<std::size_t> clusters_per_module(num_modules,
                                                                 m_mr.get());

    printf("clusters_per_module %ld\n", clusters_per_module.size());
    copy.setup(cluster_prefix_sum);
    copy.setup(clusters_per_module);

    // Invoke the reduction kernel that gives the total number of clusters which
    // will be found (and also computes the prefix sums and clusters per module)
    auto total_clusters = vecmem::make_unique_alloc<unsigned int>(m_mr.get());
    *total_clusters = 0;
    // Get the prefix sum of the cells
    const device::prefix_sum_t cells_prefix_sum = 
        device::get_prefix_sum(cell_sizes, m_mr.get()); 

    printf("get_sizes(cell_sizes.items) %ld\n",cell_sizes.size());
    printf("cells_prefix_sum %ld\n",cells_prefix_sum.size());

    // 
    vecmem::jagged_device_vector<unsigned int>
        host_sparse_ccl_indices(sparse_ccl_indices);
    printf("sparse_ccl_indices [4][-1] before: %d\n",host_sparse_ccl_indices.at(4).back());
    /* vecmem::jagged_device_vector<unsigned int>
        host_sparse_ccl_indices1(sparse_ccl_indices);  

    auto t_size =0;
    for (int i=0;i<host_sparse_ccl_indices1.size();i++){
        t_size += host_sparse_ccl_indices1[i].size();
    }
    printf("sparse_ccl_indices total_size before : %ld\n",t_size); */
    auto ptr = vecmem::get_data(clusters_per_module).ptr();
    auto ptr2 = vecmem::get_data(cluster_prefix_sum).ptr();
    std::cout << "before clusters_per_module[99]"<<*(ptr+99) << std::endl;
    std::cout << "before cluster_prefix_sum[99]"<<*(ptr2+99) << std::endl;
    traccc::cuda::clusters_sum(cells_view, sparse_ccl_indices,
                               *total_clusters, cluster_prefix_sum,
                               clusters_per_module);
    
    printf("sparse_ccl_indices [4][-1] after: %d\n",host_sparse_ccl_indices.at(4).back());
    //std::vector<std::size_t>& vecRef = *vecmem::get_data(clusters_per_module).ptr();
    std::cout << "before clusters_per_module[99]"<< *(ptr+99) << std::endl;
    std::cout << "before cluster_prefix_sum[99]"<<*(ptr2+99) << std::endl;
    //printf("clusters_per_module[0] after: %d\n",clusters_per_module);
    /* vecmem::jagged_device_vector<unsigned int>
        host_sparse_ccl_indices2(sparse_ccl_indices); 
    t_size =0;
    for (int i=0;i<host_sparse_ccl_indices2.size();i++){
        t_size += host_sparse_ccl_indices2[i].size();
    }
    printf("sparse_ccl_indices total_size after : %ld\n",t_size); */
    // Vector of the exact cluster sizes, will be filled in cluster counting

    vecmem::data::vector_buffer<unsigned int> cluster_sizes_buffer(
        *total_clusters, m_mr.get());
    copy.setup(cluster_sizes_buffer);
    auto ptr3 = vecmem::get_data(cluster_sizes_buffer).ptr();
    
    std::cout << "before cluster_sizes_buffer[99] "<<*(ptr3+99) << std::endl;
    traccc::cuda::cluster_counting(sparse_ccl_indices, cluster_sizes_buffer,
                        cluster_prefix_sum,
                        vecmem::get_data(cells_prefix_sum));

    std::cout << "after cluster_sizes_buffer[99] "<<*(ptr3+99) << std::endl;

    std::vector<unsigned int> cluster_sizes;
    copy(cluster_sizes_buffer, cluster_sizes);

    // Cluster container buffer for the clusters and headers (cluster ids)
    printf("total clusters : %ld\n",*total_clusters);
    cluster_container_types::buffer clusters_buffer{
        {*total_clusters, m_mr.get()},
        {std::vector<std::size_t>(*total_clusters, 0),
         std::vector<std::size_t>(cluster_sizes.begin(), cluster_sizes.end()),
         m_mr.get()}};

    copy.setup(clusters_buffer.headers);
    copy.setup(clusters_buffer.items);

    traccc::cluster_container_types::host cluster_p_event;
    copy(clusters_buffer.headers,
            cluster_p_event.get_headers());
    copy(clusters_buffer.items,
            cluster_p_event.get_items());

    printf("cluster_p_event.total_size() before %ld\n", cluster_p_event.total_size());

    // Component connection kernel
    traccc::cuda::component_connection(clusters_buffer, cells_view, sparse_ccl_indices, cluster_prefix_sum,
        vecmem::get_data(cells_prefix_sum));

    copy(clusters_buffer.headers,
            cluster_p_event.get_headers());
    copy(clusters_buffer.items,
            cluster_p_event.get_items());
    
    printf("cluster_p_event.total_size() after %ld\n", cluster_p_event.total_size());
    printf("clusters_buffer view. header size() %ld\n", clusters_buffer.headers.size());


    // Copy the sizes of clusters per each module to the std vector for
    // measurement buffer initialization
    std::vector<std::size_t> clusters_per_module_host;
    copy(clusters_per_module, clusters_per_module_host);
    printf("Clusters Per Module Host : %ld \n", clusters_per_module_host.size());
    // Resizable buffer for the measurements
    measurement_container_types::buffer measurements_buffer{
        {num_modules, m_mr.get()},
        {std::vector<std::size_t>(num_modules, 0), clusters_per_module_host,
         m_mr.get()}};
    copy.setup(measurements_buffer.headers);
    copy.setup(measurements_buffer.items);
    // Measurement creation kernel

    traccc::cuda::measurement_creation(measurements_buffer, clusters_buffer,
                                        cells_view);
    

    // Check measurements n

    traccc::measurement_container_types::host measurements_p_event;
    copy(measurements_buffer.headers,
            measurements_p_event.get_headers());
    copy(measurements_buffer.items,
            measurements_p_event.get_items());
    //auto measurements_view = get_data(measurements_buffer);
    
    printf("measurements_p_event.total_size() %ld\n", measurements_p_event.total_size());
   


    // Spacepoint container buffer to fill in spacepoint formation
    spacepoint_container_types::buffer spacepoints_buffer{
        {num_modules, m_mr.get()},
        {std::vector<std::size_t>(num_modules, 0), clusters_per_module_host,
         m_mr.get()}};
    copy.setup(spacepoints_buffer.headers);
    copy.setup(spacepoints_buffer.items);

    printf("Measurements Buffer items m_size %d \n",measurements_buffer.items.m_size);
    // Get the prefix sum of the measurements.
    const device::prefix_sum_t measurements_prefix_sum = device::get_prefix_sum(
        copy.get_sizes(measurements_buffer.items), m_mr.get());
    printf("get_sizes(measurements_b.items) %d\n",copy.get_sizes(measurements_buffer.items).size());
    printf("prefix sum measurements %d\n",measurements_prefix_sum.size());
    // Spacepoint formation kernel
    traccc::cuda::spacepoint_formation(
        spacepoints_buffer, measurements_buffer,
        vecmem::get_data(measurements_prefix_sum));

    return spacepoints_buffer;
}

}  // namespace traccc::cuda