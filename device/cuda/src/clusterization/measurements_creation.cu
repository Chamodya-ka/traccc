/** TRACCC library, part of the ACTS project (R&D line)
 *
 * (c) 2022 CERN for the benefit of the ACTS project
 *
 * Mozilla Public License Version 2.0
 */

// Project include(s).
#include "measurements_creation.hpp"

namespace traccc::cuda {

__global__ 
void measurement_creation(cluster_container_types::const_view clusters_view,
                          measurement_container_view measurements_view)
{
    auto idx = threadIdx.x + blockIdx.x * blockDim.x;

    // Initialize device vectors
    const cluster_container_types::const_device clusters_device(
        clusters_view);
    device_measurement_container measurement_device(
        measurements_view);

    // Ignore if idx is out of range
    if (idx >= clusters_device.size())
        return;

    // items: cluster of cells at current idx
    // header: cluster_id object with the information about the cell
    // module
    const auto& cluster = clusters_device.get_items().at(idx);
    const cluster_id& cl_id = clusters_device.get_headers().at(idx);

    const vector2 pitch = detail::get_pitch(cl_id);
    const auto module_idx = cl_id.module_idx;

    scalar totalWeight = 0.;

    // To calculate the mean and variance with high numerical
    // stability we use a weighted variant of Welford's algorithm.
    // This is a single-pass online algorithm that works well for
    // large numbers of samples, as well as samples with very high
    // values.
    //
    // To learn more about this algorithm please refer to:
    // [1] https://doi.org/10.1080/00401706.1962.10490022
    // [2] The Art of Computer Programming, Donald E. Knuth, second
    //     edition, chapter 4.2.2.
    point2 mean = {0., 0.}, var = {0., 0.};

    // Should not happen
    assert(cluster.empty() == false);

    detail::calc_cluster_properties(cluster, cl_id, mean, var,
                                    totalWeight);

    if (totalWeight > 0.) {
        measurement m;
        // normalize the cell position
        m.local = mean;
        // normalize the variance
        m.variance[0] = var[0] / totalWeight;
        m.variance[1] = var[1] / totalWeight;
        // plus pitch^2 / 12
        m.variance = m.variance + point2{pitch[0] * pitch[0] / 12,
                                            pitch[1] * pitch[1] / 12};
        // @todo add variance estimation
        measurement_device.get_items().at(module_idx).push_back(m);
    }
}

void measurement_creation(measurement_container_view measurements_view,
                          cluster_container_types::const_view clusters_view)
{

    // The kernel execution range
    auto n_clusters = clusters_view.headers.size();

    // Calculate the execution NDrange for the kernel
    auto workGroupSize = 64;
    auto num = (n_clusters + workGroupSize - 1) / workGroupSize;
    dim3 nMeasurementCreationThreads = {64,1,1};
    dim3 nMeasurementCreationBlocks = {num,1,1};
    // Run the kernel
    measurement_creation<<<nMeasurementCreationBlocks,nMeasurementCreationThreads>>>(
        clusters_view,measurements_view);


}

}  // namespace traccc::cuda