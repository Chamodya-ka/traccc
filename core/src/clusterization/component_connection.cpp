/** TRACCC library, part of the ACTS project (R&D line)
 *
 * (c) 2022 CERN for the benefit of the ACTS project
 *
 * Mozilla Public License Version 2.0
 */

// Library include(s).
#include "traccc/clusterization/component_connection.hpp"

#include "traccc/clusterization/detail/sparse_ccl.hpp"

// VecMem include(s).
#include <vecmem/containers/device_vector.hpp>
#include <vecmem/containers/vector.hpp>

namespace traccc {

component_connection::output_type component_connection::operator()(
    const cell_container_types::host& cells) const {

    std::vector<std::size_t> num_clusters(cells.size(), 0);
    std::vector<std::vector<unsigned int>> CCL_indices(cells.size());
    
    // test
/*     const auto& cellsw = cells.get_items()[1];
    for (int i = 0 ; i < cellsw.size() ; i ++){
        printf("%ld ",cellsw[i]);
    }
 */
    //

    for (std::size_t i = 0; i < cells.size(); i++) {
        const auto& cells_per_module = cells.get_items()[i];

        CCL_indices[i] = std::vector<unsigned int>(cells_per_module.size());

        // Run SparseCCL to fill CCL indices
        num_clusters[i] = detail::sparse_ccl(cells_per_module, CCL_indices[i]);
    }
    printf("first 20 of num clusters[] CPU \n");
    /* for (int i=0 ; i < 20 ; i++){
        printf("%ld ",num_clusters[i]);
    }
    printf("\n");
    for (int i =0 ; i < 10 ; i++){
        for (int j = 0 ; j < CCL_indices[i].size() ; j ++){
            printf("%ld ",CCL_indices[i][j]);
        }
        printf("\n");
    } */


    // Get total number of clusters
    const std::size_t N =
        std::accumulate(num_clusters.begin(), num_clusters.end(), 0);
    printf("cpu total clusters %ld \n",N);
    // Create the result container.
    output_type result(N, &(m_mr.get()));

    std::size_t stack = 0;
    for (std::size_t i = 0; i < cells.size(); i++) {

        auto& cells_per_module = cells.get_items()[i];

        // Fill the module link
        std::fill(result.get_headers().begin() + stack,
                  result.get_headers().begin() + stack + num_clusters[i], i);

        // Full the cluster cells
        for (std::size_t j = 0; j < CCL_indices[i].size(); j++) {

            auto cindex = static_cast<unsigned int>(CCL_indices[i][j] - 1);

            result.get_items()[stack + cindex].push_back(cells_per_module[j]);
        }

        stack += num_clusters[i];
    }
    printf("cpu results component con results [clusters_buffer] \n");
    for (int i =0 ; i < 20 ; i++){
        for (int j = 0 ; j < result.get_items()[i].size();j++){
            printf("%d ",result.get_items()[i][j]);
        }
        printf("\n");
    }
    printf("cpu total size component_con results [clusters_buffer] %ld \n",result.total_size());
    return result;
}

}  // namespace traccc
