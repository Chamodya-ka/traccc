/** TRACCC library, part of the ACTS project (R&D line)
 *
 * (c) 2021-2022 CERN for the benefit of the ACTS project
 *
 * Mozilla Public License Version 2.0
 */

// Local include(s).
#include "traccc/cuda/seeding/spacepoint_binning.hpp"
#include "traccc/cuda/utils/definitions.hpp"

// Project include(s).
#include "traccc/device/get_prefix_sum.hpp"
#include "traccc/seeding/device/count_grid_capacities.hpp"
#include "traccc/seeding/device/populate_grid.hpp"

// VecMem include(s).
#include <vecmem/utils/copy.hpp>
#include <iostream>
namespace traccc::cuda {
namespace kernels {

/// CUDA kernel for running @c traccc::device::count_grid_capacities
__global__ void count_grid_capacities(
    seedfinder_config config, sp_grid::axis_p0_type phi_axis,
    sp_grid::axis_p1_type z_axis,
    spacepoint_container_types::const_view spacepoints,
    vecmem::data::vector_view<const device::prefix_sum_element_t> sp_prefix_sum,
    vecmem::data::vector_view<unsigned int> grid_capacities) {

    device::count_grid_capacities(threadIdx.x + blockIdx.x * blockDim.x, config,
                                  phi_axis, z_axis, spacepoints, sp_prefix_sum,
                                  grid_capacities);
}

/// CUDA kernel for running @c traccc::device::populate_grid
__global__ void populate_grid(
    seedfinder_config config,
    spacepoint_container_types::const_view spacepoints,
    vecmem::data::vector_view<const device::prefix_sum_element_t> sp_prefix_sum,
    sp_grid_view grid) {

    device::populate_grid(threadIdx.x + blockIdx.x * blockDim.x, config,
                          spacepoints, sp_prefix_sum, grid);
}

}  // namespace kernels

spacepoint_binning::spacepoint_binning(
    const seedfinder_config& config, const spacepoint_grid_config& grid_config,
    vecmem::memory_resource& mr)
    : m_config(config), m_axes(get_axes(grid_config, mr)), m_mr(mr), logfile(NULL) {}

spacepoint_binning::spacepoint_binning(
    const seedfinder_config& config, const spacepoint_grid_config& grid_config,
    vecmem::memory_resource& mr,std::ofstream* logfile)
    : m_config(config), m_axes(get_axes(grid_config, mr)), m_mr(mr),logfile(logfile) {}

sp_grid_buffer spacepoint_binning::operator()(
    const spacepoint_container_types::view& sp_data) const {
    std::cout<<"here2"<<std::endl; 
    // Helper object for the data management.
    vecmem::copy copy;

    // Get the prefix sum for the spacepoints.
    const device::prefix_sum_t sp_prefix_sum =
        device::get_prefix_sum(sp_data.items, m_mr.get(), copy);
    auto sp_prefix_sum_view = vecmem::get_data(sp_prefix_sum);

    // Set up the container that will be filled with the required capacities for
    // the spacepoint grid.
    const std::size_t grid_bins = m_axes.first.n_bins * m_axes.second.n_bins;
    vecmem::vector<unsigned int> grid_capacities(grid_bins, 0, &m_mr.get());

    // Calculate the number of threads and thread blocks to run the kernels for.
    const unsigned int num_threads = WARP_SIZE * 8;
    const unsigned int num_blocks = sp_prefix_sum.size() / num_threads + 1;
    auto start_count_grid_capacities =
            std::chrono::system_clock::now();
    // Fill the grid capacity container.
    kernels::count_grid_capacities<<<num_blocks, num_threads>>>(
        m_config, m_axes.first, m_axes.second, sp_data, sp_prefix_sum_view,
        vecmem::get_data(grid_capacities));
    CUDA_ERROR_CHECK(cudaGetLastError());
    CUDA_ERROR_CHECK(cudaDeviceSynchronize());
    auto end_count_grid_capacities =
            std::chrono::system_clock::now();
    std::chrono::duration<double> time_count_grid_capacities =
            end_count_grid_capacities - start_count_grid_capacities;
    *logfile<<time_count_grid_capacities.count()<<",";    
    // Create the grid buffer.
    sp_grid_buffer grid_buffer(m_axes.first, m_axes.second,
                               std::vector<std::size_t>(grid_bins, 0),
                               std::vector<std::size_t>(grid_capacities.begin(),
                                                        grid_capacities.end()),
                               m_mr.get());
    copy.setup(grid_buffer._buffer);

    auto start_populate_grid =
            std::chrono::system_clock::now();
    // Populate the grid.
    kernels::populate_grid<<<num_blocks, num_threads>>>(
        m_config, sp_data, sp_prefix_sum_view, grid_buffer);
    CUDA_ERROR_CHECK(cudaGetLastError());
    CUDA_ERROR_CHECK(cudaDeviceSynchronize());
    auto end_populate_grid =
            std::chrono::system_clock::now();
    std::chrono::duration<double> time_populate_grid =
            end_populate_grid - start_populate_grid;
    *logfile<<time_populate_grid.count()<<",";
    // Return the freshly filled buffer.
    return grid_buffer;
}

}  // namespace traccc::cuda
