#! /bin/bash
export TRACCC_TEST_DATA_DIR='../../recent/traccc/data/'
build/bin/traccc_seq_example_cuda --detector_file=tml_detector/trackml-detector.csv --digitization_config_file=tml_detector/default-geometric-config-generic.json  --input_directory=tml_full/ttbar_mu200/   --events=10  --input-binary --u_id=0 --n_proc=8 &
build/bin/traccc_seq_example_cuda --detector_file=tml_detector/trackml-detector.csv --digitization_config_file=tml_detector/default-geometric-config-generic.json  --input_directory=tml_full/ttbar_mu200/   --events=10  --input-binary --u_id=1 --n_proc=8 &
build/bin/traccc_seq_example_cuda --detector_file=tml_detector/trackml-detector.csv --digitization_config_file=tml_detector/default-geometric-config-generic.json  --input_directory=tml_full/ttbar_mu200/   --events=10  --input-binary --u_id=2 --n_proc=8 &
build/bin/traccc_seq_example_cuda --detector_file=tml_detector/trackml-detector.csv --digitization_config_file=tml_detector/default-geometric-config-generic.json  --input_directory=tml_full/ttbar_mu200/   --events=10  --input-binary --u_id=3 --n_proc=8 &
build/bin/traccc_seq_example_cuda --detector_file=tml_detector/trackml-detector.csv --digitization_config_file=tml_detector/default-geometric-config-generic.json  --input_directory=tml_full/ttbar_mu200/   --events=10  --input-binary --u_id=4 --n_proc=8 &
build/bin/traccc_seq_example_cuda --detector_file=tml_detector/trackml-detector.csv --digitization_config_file=tml_detector/default-geometric-config-generic.json  --input_directory=tml_full/ttbar_mu200/   --events=10  --input-binary --u_id=5 --n_proc=8 &
build/bin/traccc_seq_example_cuda --detector_file=tml_detector/trackml-detector.csv --digitization_config_file=tml_detector/default-geometric-config-generic.json  --input_directory=tml_full/ttbar_mu200/   --events=10  --input-binary --u_id=6 --n_proc=8 &
build/bin/traccc_seq_example_cuda --detector_file=tml_detector/trackml-detector.csv --digitization_config_file=tml_detector/default-geometric-config-generic.json  --input_directory=tml_full/ttbar_mu200/   --events=10  --input-binary --u_id=7 --n_proc=8 &
sleep 3
wait
