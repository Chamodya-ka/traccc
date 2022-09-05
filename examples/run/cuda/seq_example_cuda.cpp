/** TRACCC library, part of the ACTS project (R&D line)
 *
 * (c) 2021-2022 CERN for the benefit of the ACTS project
 *
 * Mozilla Public License Version 2.0
 */

// Project include(s).
#include "traccc/clusterization/clusterization_algorithm.hpp"
#include "traccc/clusterization/spacepoint_formation.hpp"
#include "traccc/cuda/clusterization/clusterization_algorithm.hpp"
#include "traccc/cuda/seeding/seeding_algorithm.hpp"
#include "traccc/cuda/seeding/track_params_estimation.hpp"
#include "traccc/efficiency/seeding_performance_writer.hpp"
#include "traccc/io/csv.hpp"
#include "traccc/io/reader.hpp"
#include "traccc/io/utils.hpp"
#include "traccc/io/writer.hpp"
#include "traccc/options/common_options.hpp"
#include "traccc/options/full_tracking_input_options.hpp"
#include "traccc/options/handle_argument_errors.hpp"
#include "traccc/seeding/seeding_algorithm.hpp"
#include "traccc/seeding/track_params_estimation.hpp"

// VecMem include(s).
#include <vecmem/memory/cuda/device_memory_resource.hpp>
#include <vecmem/memory/cuda/host_memory_resource.hpp>
#include <vecmem/memory/cuda/managed_memory_resource.hpp>
#include <vecmem/memory/host_memory_resource.hpp>
#include <vecmem/utils/cuda/copy.hpp>

// System include(s).
#include <chrono>
#include <exception>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>

// boost inclues
#include <boost/interprocess/mapped_region.hpp>
#include <boost/interprocess/shared_memory_object.hpp>
//#include <boost/exception/diagnostic_information.hpp>

#include "traccc/cuda/utils/Sync.hpp"

namespace po = boost::program_options;

int seq_run(const traccc::full_tracking_input_config& i_cfg,
            const traccc::common_options& common_opts, bool run_cpu,
            std::ofstream& logfile, unsigned char* mem) {

    // Read the surface transforms
    auto surface_transforms = traccc::read_geometry(i_cfg.detector_file);

    // Read the digitization configuration file
    auto digi_cfg =
        traccc::read_digitization_config(i_cfg.digitization_config_file);

    // Output stats
    uint64_t n_cells = 0;
    uint64_t n_modules = 0;
    // uint64_t n_clusters = 0;
    uint64_t n_measurements = 0;
    uint64_t n_spacepoints = 0;
    uint64_t n_spacepoints_cuda = 0;
    uint64_t n_seeds = 0;
    uint64_t n_seeds_cuda = 0;

    // Elapsed time
    float wall_time(0);
    float file_reading_cpu(0);
    float clusterization_cpu(0);
    float sp_formation_cpu(0);
    float seeding_cpu(0);
    float clusterization_sp_cuda(0);
    float seeding_cuda(0);
    float tp_estimating_cpu(0);
    float tp_estimating_cuda(0);

    // Memory resource used by the EDM.
    vecmem::host_memory_resource host_mr;
    vecmem::cuda::managed_memory_resource mng_mr;
    vecmem::cuda::host_memory_resource cu_host_mr;
    vecmem::cuda::device_memory_resource cu_dev_mr;

    // Struct with memory resources to pass to CUDA algorithms
    traccc::memory_resource mr{cu_dev_mr, &cu_host_mr};

    traccc::clusterization_algorithm ca(host_mr);
    traccc::spacepoint_formation sf(host_mr);
    traccc::seeding_algorithm sa(host_mr);
    traccc::track_params_estimation tp(host_mr);

    traccc::cuda::seeding_algorithm sa_cuda(mr, &logfile, mem);
    traccc::cuda::track_params_estimation tp_cuda(mr, &logfile, mem);
    traccc::cuda::clusterization_algorithm ca_cuda(mr, &logfile, mem);

    // performance writer
    traccc::seeding_performance_writer sd_performance_writer(
        traccc::seeding_performance_writer::config{});
    if (i_cfg.check_performance) {
        sd_performance_writer.add_cache("CPU");
        sd_performance_writer.add_cache("CUDA");
    }

    // Loop over events
    for (unsigned int event = common_opts.skip;
         event < common_opts.events + common_opts.skip; ++event) {
        logfile << event << ",";
        /*time*/ auto start_wall_time = std::chrono::system_clock::now();

        /*time*/ auto start_file_reading_cpu = std::chrono::system_clock::now();

        traccc::cell_container_types::host cells_per_event;

        if (run_cpu) {
            // Read the cells from the relevant event file for CPU algorithm
            cells_per_event = traccc::read_cells_from_event(
                event, common_opts.input_directory,
                common_opts.input_data_format, surface_transforms, digi_cfg,
                host_mr);
        }

        // Read the cells from the relevant event file for CUDA algorithm
        traccc::cell_container_types::host cells_per_event_cuda =
            traccc::read_cells_from_event(event, common_opts.input_directory,
                                          common_opts.input_data_format,
                                          surface_transforms, digi_cfg,
                                          (mr.host ? *(mr.host) : mr.main));

        /*time*/ auto end_file_reading_cpu = std::chrono::system_clock::now();
        /*time*/ std::chrono::duration<double> time_file_reading_cpu =
            end_file_reading_cpu - start_file_reading_cpu;
        /*time*/ file_reading_cpu += time_file_reading_cpu.count();
        logfile << time_file_reading_cpu.count() << ",";
        /*-----------------------------
              Clusterization and Spacepoint Creation (cuda)
          -----------------------------*/
        /*time*/ auto start_cluterization_cuda =
            std::chrono::system_clock::now();

        auto spacepoints_cuda_buffer = ca_cuda(cells_per_event_cuda);

        /*time*/ auto end_cluterization_cuda = std::chrono::system_clock::now();
        /*time*/ std::chrono::duration<double> time_clusterization_cuda =
            end_cluterization_cuda - start_cluterization_cuda;
        /*time*/ clusterization_sp_cuda += time_clusterization_cuda.count();
        logfile << time_clusterization_cuda.count() << ",";
        traccc::clusterization_algorithm::output_type measurements_per_event;
        traccc::spacepoint_formation::output_type spacepoints_per_event;

        if (run_cpu) {

            /*-----------------------------
                  Clusterization (cpu)
              -----------------------------*/

            /*time*/ auto start_clusterization_cpu =
                std::chrono::system_clock::now();

            measurements_per_event = ca(cells_per_event);

            /*time*/ auto end_clusterization_cpu =
                std::chrono::system_clock::now();
            /*time*/ std::chrono::duration<double> time_clusterization_cpu =
                end_clusterization_cpu - start_clusterization_cpu;
            /*time*/ clusterization_cpu += time_clusterization_cpu.count();

            /*---------------------------------
                   Spacepoint formation (cpu)
              ---------------------------------*/

            /*time*/ auto start_sp_formation_cpu =
                std::chrono::system_clock::now();

            spacepoints_per_event = sf(measurements_per_event);

            /*time*/ auto end_sp_formation_cpu =
                std::chrono::system_clock::now();
            /*time*/ std::chrono::duration<double> time_sp_formation_cpu =
                end_sp_formation_cpu - start_sp_formation_cpu;
            /*time*/ sp_formation_cpu += time_sp_formation_cpu.count();
        }

        /*----------------------------
             Seeding algorithm
          ----------------------------*/

        // CUDA

        /*time*/ auto start_seeding_cuda = std::chrono::system_clock::now();

        auto seeds_cuda_buffer = sa_cuda(spacepoints_cuda_buffer);

        /*time*/ auto end_seeding_cuda = std::chrono::system_clock::now();
        /*time*/ std::chrono::duration<double> time_seeding_cuda =
            end_seeding_cuda - start_seeding_cuda;
        /*time*/ seeding_cuda += time_seeding_cuda.count();
        logfile << time_seeding_cuda.count() << ",";
        // CPU

        traccc::seeding_algorithm::output_type seeds;

        if (run_cpu) {
            /*time*/ auto start_seeding_cpu = std::chrono::system_clock::now();

            seeds = sa(spacepoints_per_event);

            /*time*/ auto end_seeding_cpu = std::chrono::system_clock::now();
            /*time*/ std::chrono::duration<double> time_seeding_cpu =
                end_seeding_cpu - start_seeding_cpu;
            /*time*/ seeding_cpu += time_seeding_cpu.count();
        }

        /*----------------------------
          Track params estimation
          ----------------------------*/

        // CUDA

        /*time*/ auto start_tp_estimating_cuda =
            std::chrono::system_clock::now();

        auto params_cuda = tp_cuda(spacepoints_cuda_buffer, seeds_cuda_buffer);

        /*time*/ auto end_tp_estimating_cuda = std::chrono::system_clock::now();
        /*time*/ std::chrono::duration<double> time_tp_estimating_cuda =
            end_tp_estimating_cuda - start_tp_estimating_cuda;
        /*time*/ tp_estimating_cuda += time_tp_estimating_cuda.count();
        logfile << time_tp_estimating_cuda.count() << std::endl;
        // CPU

        traccc::track_params_estimation::output_type params;
        if (run_cpu) {
            /*time*/ auto start_tp_estimating_cpu =
                std::chrono::system_clock::now();

            params = tp(spacepoints_per_event, seeds);

            /*time*/ auto end_tp_estimating_cpu =
                std::chrono::system_clock::now();
            /*time*/ std::chrono::duration<double> time_tp_estimating_cpu =
                end_tp_estimating_cpu - start_tp_estimating_cpu;
            /*time*/ tp_estimating_cpu += time_tp_estimating_cpu.count();
        }

        /*----------------------------------
          compare cpu and cuda result
          ----------------------------------*/

        vecmem::cuda::copy copy;
        traccc::spacepoint_container_types::host spacepoints_per_event_cuda;
        traccc::host_seed_collection seeds_cuda;

        if (run_cpu) {

            // Converting spacepoints container buffer to host
            copy(spacepoints_cuda_buffer.headers,
                 spacepoints_per_event_cuda.get_headers());
            copy(spacepoints_cuda_buffer.items,
                 spacepoints_per_event_cuda.get_items());
            copy(seeds_cuda_buffer, seeds_cuda);

            // Initial information about number of seeds found
            std::cout << "event " << std::to_string(event) << std::endl;
            std::cout << " number of seeds (cpu): " << seeds.size()
                      << std::endl;
            std::cout << " number of seeds (cuda): " << seeds_cuda.size()
                      << std::endl;

            // measurements & spacepoint matching rate
            int n_m_match = 0;
            int n_match = 0;
            assert(spacepoints_per_event.size() ==
                   spacepoints_per_event_cuda.size());
            for (std::size_t i = 0; i < spacepoints_per_event.size(); ++i) {
                assert(spacepoints_per_event[i].items.size() ==
                       spacepoints_per_event_cuda[i].items.size());
                for (auto& sp : spacepoints_per_event[i].items) {
                    auto found_sp = std::find(
                        spacepoints_per_event_cuda[i].items.begin(),
                        spacepoints_per_event_cuda[i].items.end(), sp);
                    auto found_m = std::find_if(
                        spacepoints_per_event_cuda[i].items.begin(),
                        spacepoints_per_event_cuda[i].items.end(),
                        [&sp](auto& sp_cuda) {
                            return sp.meas == sp_cuda.meas;
                        });
                    if (found_m != spacepoints_per_event_cuda[i].items.end()) {
                        n_m_match++;
                    }
                    if (found_sp != spacepoints_per_event_cuda[i].items.end()) {
                        n_match++;
                    }
                }
            }
            float m_matching_rate =
                float(n_m_match) / spacepoints_per_event.total_size();
            std::cout << " measurements matching rate: " << m_matching_rate
                      << std::endl;
            float matching_rate =
                float(n_match) / spacepoints_per_event.total_size();
            std::cout << " spacepoint matching rate: " << matching_rate
                      << std::endl;

            // seeding matching rate
            n_match = 0;
            std::vector<std::array<traccc::spacepoint, 3>> sp3_vector =
                traccc::get_spacepoint_vector(seeds, spacepoints_per_event);

            std::vector<std::array<traccc::spacepoint, 3>> sp3_vector_cuda =
                traccc::get_spacepoint_vector(seeds_cuda,
                                              spacepoints_per_event_cuda);

            for (const auto& sp3 : sp3_vector) {
                if (std::find(sp3_vector_cuda.cbegin(), sp3_vector_cuda.cend(),
                              sp3) != sp3_vector_cuda.cend()) {
                    n_match++;
                }
            }
            matching_rate = float(n_match) / seeds.size();
            std::cout << " seed matching rate: " << matching_rate << std::endl;

            // track parameter estimation matching rate
            n_match = 0;
            for (auto& param : params) {
                if (std::find(params_cuda.begin(), params_cuda.end(), param) !=
                    params_cuda.end()) {
                    n_match++;
                }
            }
            matching_rate = float(n_match) / params.size();
            std::cout << " track parameters matching rate: " << matching_rate
                      << std::endl;

            /// Statistics
            n_modules += cells_per_event.size();
            n_cells += cells_per_event.total_size();
            n_measurements += measurements_per_event.total_size();
            n_spacepoints += spacepoints_per_event.total_size();
            n_spacepoints_cuda += spacepoints_per_event_cuda.total_size();
            n_seeds_cuda += seeds_cuda.size();
            n_seeds += seeds.size();
        }

        if (i_cfg.check_performance) {

            // Converting spacepoints container buffer to host
            copy(spacepoints_cuda_buffer.headers,
                 spacepoints_per_event_cuda.get_headers());
            copy(spacepoints_cuda_buffer.items,
                 spacepoints_per_event_cuda.get_items());
            copy(seeds_cuda_buffer, seeds_cuda);

            traccc::event_map evt_map(
                event, i_cfg.detector_file, i_cfg.digitization_config_file,
                common_opts.input_directory, common_opts.input_directory,
                common_opts.input_directory, host_mr);
            sd_performance_writer.write("CUDA", seeds_cuda,
                                        spacepoints_per_event_cuda, evt_map);

            if (run_cpu) {
                sd_performance_writer.write("CPU", seeds, spacepoints_per_event,
                                            evt_map);
            }
        }

        /*time*/ auto end_wall_time = std::chrono::system_clock::now();
        /*time*/ std::chrono::duration<double> time_wall_time =
            end_wall_time - start_wall_time;

        /*time*/ wall_time += time_wall_time.count();
    }

    if (i_cfg.check_performance) {
        sd_performance_writer.finalize();
    }

    std::cout << "==> Statistics ... " << std::endl;
    std::cout << "- read    " << n_spacepoints << " spacepoints from "
              << n_modules << " modules" << std::endl;
    std::cout << "- created        " << n_cells << " cells           "
              << std::endl;
    std::cout << "- created        " << n_measurements << " meaurements     "
              << std::endl;
    std::cout << "- created        " << n_spacepoints << " spacepoints     "
              << std::endl;

    std::cout << "- created (cpu)  " << n_seeds << " seeds" << std::endl;
    std::cout << "- created (cuda) " << n_seeds_cuda << " seeds" << std::endl;
    std::cout << "==> Elpased time ... " << std::endl;
    std::cout << "wall time           " << std::setw(10) << std::left
              << wall_time << std::endl;
    std::cout << "file reading (cpu)        " << std::setw(10) << std::left
              << file_reading_cpu << std::endl;
    std::cout << "clusterization_time (cpu) " << std::setw(10) << std::left
              << clusterization_cpu << std::endl;
    std::cout << "spacepoint_formation_time (cpu) " << std::setw(10)
              << std::left << sp_formation_cpu << std::endl;
    std::cout << "clusterization and sp formation (cuda) " << std::setw(10)
              << std::left << clusterization_sp_cuda << std::endl;

    std::cout << "seeding_time (cpu)        " << std::setw(10) << std::left
              << seeding_cpu << std::endl;
    std::cout << "seeding_time (cuda)       " << std::setw(10) << std::left
              << seeding_cuda << std::endl;
    std::cout << "tr_par_esti_time (cpu)    " << std::setw(10) << std::left
              << tp_estimating_cpu << std::endl;
    std::cout << "tr_par_esti_time (cuda)   " << std::setw(10) << std::left
              << tp_estimating_cuda << std::endl;
    logfile.close();
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
    desc.add_options()("u_id", po::value<int>()->default_value(0),
                       "unique if of process");
    desc.add_options()("n_proc", po::value<int>()->default_value(1),
                       "number of processes in batch");
    desc.add_options()("log_time", po::value<std::string>()->default_value("0"),
                       "log tstart time [unique]");

    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);

    // Check errors
    traccc::handle_argument_errors(vm, desc);

    // Read options
    common_opts.read(vm);
    full_tracking_input_cfg.read(vm);
    auto run_cpu = vm["run_cpu"].as<bool>();
    int u_id = vm["u_id"].as<int>();
    int n_proc = vm["n_proc"].as<int>();
    std::string log_time = vm["log_time"].as<std::string>();

    // Setup log file
    // Does not create directory. Make sure directory is properly created by
    // bash script
    std::string log_file_path = log_time + "/" + std::to_string(u_id);
    std::ofstream logfile;
    logfile.open(log_file_path + ".csv",
                 std::ios_base::app);  // append instead of overwrite
    logfile
        << "event,file "
           "io,find_clusters_kernel,count_cluster_cells,connect_components,"
           "create_measurements,form_spacepoints,overall "
           "clusterization,count_grid_capacities,populate_grid,count_doublets,"
           "find_doublets,set_zero_kernel,triplet_counting_kernel,set_zero_"
           "triplet_finding,triplet_finding,weight_updating,seed_selecting,"
           "overall_seeding,track_param_est_kernel,overall_track_param_est"
        << std::endl;

    // Set up shared memory
    /* struct shm_remove
    {
        shm_remove() {
    boost::interprocess::shared_memory_object::remove("shared_memory"); }
        ~shm_remove(){
    boost::interprocess::shared_memory_object::remove("shared_memory"); } }
    remover; */
    // create shared memory object
    boost::interprocess::shared_memory_object shm(
        boost::interprocess::open_or_create, "shared_memory",
        boost::interprocess::read_write);
    // resize shared memory
    shm.truncate(n_proc);
    // map the whole shared memory in this process
    boost::interprocess::mapped_region region(shm,
                                              boost::interprocess::read_write);
    // get a pointer to this memory region
    unsigned char* mem = static_cast<unsigned char*>(region.get_address());
    Sync::init_shared_mem(n_proc, u_id, region.get_size(), mem);
    printf("%d ID %d \n", u_id, Sync::u_id);
    std::cout << "Running " << argv[0] << " "
              << full_tracking_input_cfg.detector_file << " "
              << common_opts.input_directory << " " << common_opts.events
              << std::endl;

    return seq_run(full_tracking_input_cfg, common_opts, run_cpu, logfile, mem);
}
