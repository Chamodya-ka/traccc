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
#include "traccc/seeding/seeding_algorithm.hpp"
#include "traccc/seeding/track_params_estimation.hpp"

// vecmem
#include <vecmem/memory/host_memory_resource.hpp>
#include <vecmem/memory/binary_page_memory_resource.hpp>
#include <vecmem/memory/contiguous_memory_resource.hpp>

// options
#include "traccc/options/common_options.hpp"
#include "traccc/options/full_tracking_input_options.hpp"
#include "traccc/options/handle_argument_errors.hpp"

// System include(s).
#include <exception>
#include <iostream>

namespace po = boost::program_options;

int seq_run(const traccc::full_tracking_input_config& i_cfg,
            const traccc::common_options& common_opts) {

    float wall_time(0);
    float file_reading(0);
    float clusterization_cpu(0);
    float sp_formation_cpu(0);
    float seeding_cpu(0);
    float tp_estimating_cpu(0);

    // Read the surface transforms
    auto surface_transforms = traccc::read_geometry(i_cfg.detector_file);

    // Read the digitization configuration file
    auto digi_cfg =
        traccc::read_digitization_config(i_cfg.digitization_config_file);

    // Output stats
    uint64_t n_cells = 0;
    uint64_t n_modules = 0;
    uint64_t n_measurements = 0;
    uint64_t n_spacepoints = 0;
    uint64_t n_seeds = 0;

    // Memory resource used by the EDM.
    vecmem::host_memory_resource host_mr;
    vecmem::binary_page_memory_resource bpmr(host_mr);
    vecmem::contiguous_memory_resource c_mr(host_mr,1073741824);
    traccc::clusterization_algorithm ca(c_mr);
    traccc::spacepoint_formation sf(c_mr);
    traccc::seeding_algorithm sa(c_mr);
    traccc::track_params_estimation tp(c_mr);

    auto start_wall_time = std::chrono::system_clock::now();

    // Loop over events
    for (unsigned int event = common_opts.skip;
         event < common_opts.events + common_opts.skip; ++event) {

        // Read the cells from the relevant event file
        auto start_file_reading = std::chrono::system_clock::now();
        traccc::cell_container_types::host cells_per_event =
            traccc::read_cells_from_event(
                event, i_cfg.cell_directory, common_opts.input_data_format,
                surface_transforms, digi_cfg, c_mr);
        auto end_file_reading = std::chrono::system_clock::now();
          /*time*/ std::chrono::duration<double> file_reading_time =
        end_file_reading - start_file_reading;

      /*time*/ file_reading += file_reading_time.count();
        /*-------------------
            Clusterization
          -------------------*/
        auto start_clusterization = std::chrono::system_clock::now();
        auto measurements_per_event = ca(cells_per_event);
        auto end_clusterization = std::chrono::system_clock::now();
          /*time*/ std::chrono::duration<double> clusterization_time =
        end_clusterization - start_clusterization;

      /*time*/ clusterization_cpu += clusterization_time.count();
        /*------------------------
            Spacepoint formation
          ------------------------*/

        auto start_spacepoint = std::chrono::system_clock::now();
        auto spacepoints_per_event = sf(measurements_per_event);
        auto end_spacepoint = std::chrono::system_clock::now();
          /*time*/ std::chrono::duration<double> sp_time =
        end_spacepoint - start_spacepoint;

      /*time*/ sp_formation_cpu += sp_time.count();
        /*-----------------------
          Seeding algorithm
          -----------------------*/

        auto start_seeding = std::chrono::system_clock::now();
        auto seeds = sa(spacepoints_per_event);
        auto end_seeding = std::chrono::system_clock::now();
          /*time*/ std::chrono::duration<double> seeding_time =
        end_seeding - start_seeding;

      /*time*/ seeding_cpu += seeding_time.count();
        /*----------------------------
          Track params estimation
          ----------------------------*/

        auto start_tp = std::chrono::system_clock::now();
        auto params = tp(spacepoints_per_event, seeds);
        auto end_tp = std::chrono::system_clock::now();
          /*time*/ std::chrono::duration<double> tp_time =
        end_tp - start_tp;

      /*time*/ tp_estimating_cpu += tp_time.count();
        /*----------------------------
          Statistics
          ----------------------------*/

        n_modules += cells_per_event.size();
        n_cells += cells_per_event.total_size();
        n_measurements += measurements_per_event.total_size();
        n_spacepoints += spacepoints_per_event.total_size();
        n_seeds += seeds.size();


    }

    /*time*/ auto end_wall_time = std::chrono::system_clock::now();
    /*time*/ std::chrono::duration<double> time_wall_time =
        end_wall_time - start_wall_time;

    /*time*/ wall_time += time_wall_time.count();
    std::cout << "==> Elpased time [CPU]... " << std::endl;
        std::cout << "wall time           " << std::setw(10) << std::left
              << wall_time << std::endl;
        std::cout << "file reading time           " << std::setw(10) << std::left
              << file_reading << std::endl;
        std::cout << "clusterization time           " << std::setw(10) << std::left
              << clusterization_cpu << std::endl;
        std::cout << "spacepoint form time           " << std::setw(10) << std::left
              << sp_formation_cpu << std::endl;
        std::cout << "seeding time           " << std::setw(10) << std::left
              << seeding_cpu << std::endl;
        std::cout << "tp est. time           " << std::setw(10) << std::left
              << tp_estimating_cpu << std::endl;
    std::cout << "==> Statistics ... " << std::endl;
    std::cout << "- read    " << n_cells << " cells from " << n_modules
              << " modules" << std::endl;
    std::cout << "- created " << n_measurements << " measurements. "
              << std::endl;
    std::cout << "- created " << n_spacepoints << " space points. "
              << std::endl;
    std::cout << "- created " << n_seeds << " seeds" << std::endl;

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

    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);

    // Check errors
    traccc::handle_argument_errors(vm, desc);

    // Read options
    common_opts.read(vm);
    full_tracking_input_cfg.read(vm);

    std::cout << "Running " << argv[0] << " "
              << full_tracking_input_cfg.detector_file << " "
              << full_tracking_input_cfg.cell_directory << " "
              << common_opts.events << std::endl;

    return seq_run(full_tracking_input_cfg, common_opts);
}
