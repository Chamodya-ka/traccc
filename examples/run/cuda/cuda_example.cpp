/** TRACCC library, part of the ACTS project (R&D line)
 *
 * (c) 2021-2022 CERN for the benefit of the ACTS project
 *
 * Mozilla Public License Version 2.0
 */

// io
#include "traccc/io/csv.hpp"
#include "traccc/io/reader.hpp"
#include "traccc/io/utils.hpp"
#include "traccc/io/writer.hpp"

// algorithms
#include "traccc/clusterization/clusterization_algorithm.hpp"
#include "traccc/clusterization/spacepoint_formation.hpp"
#include "traccc/cuda/clusterization/clusterization_algorithm.hpp"
#include "traccc/cuda/seeding/seeding_algorithm.hpp"
#include "traccc/cuda/seeding/track_params_estimation.hpp"
#include "traccc/seeding/seeding_algorithm.hpp"
#include "traccc/seeding/track_params_estimation.hpp"

// performance
//#include "traccc/efficiency/seeding_performance_writer.hpp"

// options
#include "traccc/options/common_options.hpp"
#include "traccc/options/full_tracking_input_options.hpp"
#include "traccc/options/handle_argument_errors.hpp"

// vecmem
#include <vecmem/memory/cuda/managed_memory_resource.hpp>
#include <vecmem/memory/host_memory_resource.hpp>
//#include <vecmem/memory/binary_page_memory_resource.hpp>

// System include(s).
#include <chrono>
#include <exception>
#include <iomanip>
#include <iostream>

namespace po = boost::program_options;

int seq_run(const traccc::full_tracking_input_config& i_cfg,
            const traccc::common_options& common_opts, bool run_cpu) {

      

    // Read the surface transforms
    auto surface_transforms = traccc::read_geometry(i_cfg.detector_file);

    // Read the digitization configuration file
    auto digi_cfg =
        traccc::read_digitization_config(i_cfg.digitization_config_file);

    // Output stats
    uint64_t n_seeds_cuda = 0;

    // Elapsed time
    float wall_time(0);
    float file_reading_cpu(0);
    float clusterization_sp_cuda(0);
    float seeding_cuda(0);
    float tp_estimating_cuda(0);

    // Memory resource used by the EDM.
    //vecmem::host_memory_resource host_mr;
    vecmem::cuda::managed_memory_resource mng_mr;
    //vecmem::binary_page_memory_resource bpmr(mng_mr);

    traccc::cuda::seeding_algorithm sa_cuda(mng_mr);
    traccc::cuda::track_params_estimation tp_cuda(mng_mr);
    traccc::cuda::clusterization_algorithm ca_cuda(mng_mr);

   
    /*time*/ auto start_wall_time = std::chrono::system_clock::now();  
    // Loop over events
    for (unsigned int event = common_opts.skip;
         event < common_opts.events + common_opts.skip; ++event) {

        /*time*/ auto start_file_reading_cpu = std::chrono::system_clock::now();

        // Read the cells from the relevant event file
        traccc::cell_container_types::host cells_per_event =
            traccc::read_cells_from_event(event, i_cfg.cell_directory,
                                          common_opts.input_data_format,
                                          surface_transforms, digi_cfg, mng_mr);

        /*time*/ auto end_file_reading_cpu = std::chrono::system_clock::now();
        /*time*/ std::chrono::duration<double> time_file_reading_cpu =
            end_file_reading_cpu - start_file_reading_cpu;
        /*time*/ file_reading_cpu += time_file_reading_cpu.count();


        /*-----------------------------
              Clusterization and Spacepoint Creation (cuda)
          -----------------------------*/
        /*time*/ auto start_cluterization_cuda =
            std::chrono::system_clock::now();

        auto spacepoints_per_event_cuda = ca_cuda(cells_per_event);

        /*time*/ auto end_cluterization_cuda = std::chrono::system_clock::now();
        /*time*/ std::chrono::duration<double> time_clusterization_cuda =
            end_cluterization_cuda - start_cluterization_cuda;
        /*time*/ clusterization_sp_cuda += time_clusterization_cuda.count();

        /*----------------------------
             Seeding algorithm
          ----------------------------*/

        // CUDA

        /*time*/ auto start_seeding_cuda = std::chrono::system_clock::now();

        auto seeds_cuda = sa_cuda(spacepoints_per_event_cuda);

        /*time*/ auto end_seeding_cuda = std::chrono::system_clock::now();
        /*time*/ std::chrono::duration<double> time_seeding_cuda =
            end_seeding_cuda - start_seeding_cuda;
        /*time*/ seeding_cuda += time_seeding_cuda.count();

        

        /*----------------------------
          Track params estimation
          ----------------------------*/

        // CUDA

        /*time*/ auto start_tp_estimating_cuda =
            std::chrono::system_clock::now();

        auto params_cuda = tp_cuda(std::move(spacepoints_per_event_cuda),
                                   std::move(seeds_cuda));

        /*time*/ auto end_tp_estimating_cuda = std::chrono::system_clock::now();
        /*time*/ std::chrono::duration<double> time_tp_estimating_cuda =
            end_tp_estimating_cuda - start_tp_estimating_cuda;
        /*time*/ tp_estimating_cuda += time_tp_estimating_cuda.count();



        /*----------------
             Statistics
          ---------------*/

        n_seeds_cuda += seeds_cuda.size();

    }

    /*time*/ auto end_wall_time = std::chrono::system_clock::now();
    /*time*/ std::chrono::duration<double> time_wall_time =
        end_wall_time - start_wall_time;

    /*time*/ wall_time += time_wall_time.count();

    //sd_performance_writer.finalize();

    std::cout << "==> Statistics ... " << std::endl;
    std::cout << "- created (cuda) " << n_seeds_cuda << " seeds" << std::endl;
    std::cout << "==> Elpased time ... " << std::endl;
    std::cout << "wall time           " << std::setw(10) << std::left
              << wall_time << std::endl;
    std::cout << "file reading (cpu)        " << std::setw(10) << std::left
              << file_reading_cpu << std::endl;
    std::cout << "clusterization and sp formation (cuda) " << std::setw(10)
              << std::left << clusterization_sp_cuda << std::endl;
    std::cout << "seeding_time (cuda)       " << std::setw(10) << std::left
              << seeding_cuda << std::endl;
    std::cout << "tr_par_esti_time (cuda)   " << std::setw(10) << std::left
              << tp_estimating_cuda << std::endl;

    return 0;
}

// The main routine
//
int main(int argc, char* argv[]) {
    // Set up the program options
    po::options_description desc("Allowed options");

    // Add options
    desc.add_options()("help,h", "Give some help with the program's options");
    traccc::common_options common_opts(desc);
    traccc::full_tracking_input_config full_tracking_input_cfg(desc);
    desc.add_options()("run_cpu", po::value<bool>()->default_value(false),
                       "run cpu tracking as well");

    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);

    // Check errors
    traccc::handle_argument_errors(vm, desc);

    // Read options
    common_opts.read(vm);
    full_tracking_input_cfg.read(vm);
    auto run_cpu = vm["run_cpu"].as<bool>();

    std::cout << "Running " << argv[0] << " "
              << full_tracking_input_cfg.detector_file << " "
              << full_tracking_input_cfg.cell_directory << " "
              << common_opts.events << std::endl;

    return seq_run(full_tracking_input_cfg, common_opts, run_cpu);
}
