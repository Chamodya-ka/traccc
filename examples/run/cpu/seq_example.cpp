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

// performance
#include "traccc/efficiency/seeding_performance_writer.hpp"

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

    // Read the surface transforms
    auto surface_transforms = traccc::read_geometry(i_cfg.detector_file);

    // Read the digitization configuration file
    auto digi_cfg =
        traccc::read_digitization_config(i_cfg.digitization_config_file);
    
   float file_reading_cpu(0);
    float clusterization_cpu(0);
    float sp_formation_cpu(0);
    float seeding_cpu(0);
    float tp_estimating_cpu(0);

    // Output stats
    uint64_t n_cells = 0;
    uint64_t n_modules = 0;
    uint64_t n_measurements = 0;
    uint64_t n_spacepoints = 0;
    uint64_t n_seeds = 0;

    // Memory resource used by the EDM.
    vecmem::host_memory_resource host_mr;

    traccc::clusterization_algorithm ca(host_mr);
    traccc::spacepoint_formation sf(host_mr);
    traccc::seeding_algorithm sa(host_mr);
    traccc::track_params_estimation tp(host_mr);

    // performance writer
    traccc::seeding_performance_writer sd_performance_writer(
        traccc::seeding_performance_writer::config{});

    if (i_cfg.check_performance) {
        sd_performance_writer.add_cache("CPU");
    }

    // Loop over events
    for (unsigned int event = common_opts.skip;
         event < common_opts.events + common_opts.skip; ++event) {

/*time*/ auto start_file_reading_cpu = std::chrono::system_clock::now();

        // Read the cells from the relevant event file
        traccc::cell_container_types::host cells_per_event =
            traccc::read_cells_from_event(event, common_opts.input_directory,
                                          common_opts.input_data_format,
                                          surface_transforms, digi_cfg,
                                          host_mr);

/*time*/ auto end_file_reading_cpu = std::chrono::system_clock::now();
        /*time*/ std::chrono::duration<double> time_file_reading_cpu =
            end_file_reading_cpu - start_file_reading_cpu;
        /*time*/ file_reading_cpu += time_file_reading_cpu.count();
        /*-------------------
            Clusterization
          -------------------*/


auto start_clusterization_cpu =
                std::chrono::system_clock::now();
        auto measurements_per_event = ca(cells_per_event);

/*time*/ auto end_clusterization_cpu =
                std::chrono::system_clock::now();
            /*time*/ std::chrono::duration<double> time_clusterization_cpu =
                end_clusterization_cpu - start_clusterization_cpu;
            /*time*/ clusterization_cpu += time_clusterization_cpu.count();

        /*------------------------
            Spacepoint formation
          ------------------------*/

/*time*/ auto start_sp_formation_cpu =
                std::chrono::system_clock::now();

        auto spacepoints_per_event = sf(measurements_per_event);

        /*time*/ auto end_sp_formation_cpu =
                std::chrono::system_clock::now();
            /*time*/ std::chrono::duration<double> time_sp_formation_cpu =
                end_sp_formation_cpu - start_sp_formation_cpu;
            /*time*/ sp_formation_cpu += time_sp_formation_cpu.count();

        /*-----------------------
          Seeding algorithm
          -----------------------*/

/*time*/ auto start_seeding_cpu = std::chrono::system_clock::now();
        auto seeds = sa(spacepoints_per_event);
        /*time*/ auto end_seeding_cpu = std::chrono::system_clock::now();
            /*time*/ std::chrono::duration<double> time_seeding_cpu =
                end_seeding_cpu - start_seeding_cpu;
            /*time*/ seeding_cpu += time_seeding_cpu.count();

        /*----------------------------
          Track params estimation
          ----------------------------*/
/*time*/ auto start_tp_estimating_cpu =
                std::chrono::system_clock::now();
        auto params = tp(spacepoints_per_event, seeds);
        /*time*/ auto end_tp_estimating_cpu =
                std::chrono::system_clock::now();
            /*time*/ std::chrono::duration<double> time_tp_estimating_cpu =
                end_tp_estimating_cpu - start_tp_estimating_cpu;
            /*time*/ tp_estimating_cpu += time_tp_estimating_cpu.count();

        /*----------------------------
          Statistics
          ----------------------------*/

        n_modules += cells_per_event.size();
        n_cells += cells_per_event.total_size();
        n_measurements += measurements_per_event.total_size();
        n_spacepoints += spacepoints_per_event.total_size();
        n_seeds += seeds.size();

        /*------------
             Writer
          ------------*/

        if (i_cfg.check_performance) {
            traccc::event_map evt_map(
                event, i_cfg.detector_file, i_cfg.digitization_config_file,
                common_opts.input_directory, common_opts.input_directory,
                common_opts.input_directory, host_mr);

            sd_performance_writer.write("CPU", seeds, spacepoints_per_event,
                                        evt_map);
        }
    }

    if (i_cfg.check_performance) {
        sd_performance_writer.finalize();
    }

    std::cout << "==> Statistics ... " << std::endl;
    std::cout << "- read    " << n_cells << " cells from " << n_modules
              << " modules" << std::endl;
    std::cout << "- created " << n_measurements << " measurements. "
              << std::endl;
    std::cout << "- created " << n_spacepoints << " space points. "
              << std::endl;
    std::cout << "- created " << n_seeds << " seeds" << std::endl;

std::cout << "file reading (cpu)        " << std::setw(10) << std::left
              << file_reading_cpu << std::endl;
    std::cout << "clusterization_time (cpu) " << std::setw(10) << std::left
              << clusterization_cpu << std::endl;
    std::cout << "spacepoint_formation_time (cpu) " << std::setw(10)
              << std::left << sp_formation_cpu << std::endl;
    std::cout << "seeding_time (cpu)        " << std::setw(10) << std::left
              << seeding_cpu << std::endl;
    std::cout << "tr_par_esti_time (cpu)    " << std::setw(10) << std::left
              << tp_estimating_cpu << std::endl;

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
              << common_opts.input_directory << " " << common_opts.events
              << std::endl;

    return seq_run(full_tracking_input_cfg, common_opts);
}
