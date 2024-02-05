# P4 SRv6 Live-Live Behaviour

## Directory structure

- `experiments`: contains experiments scripts used for the paper
- `lab`: contains the Kathará network scenario, without the P4 source code
- `p4src`: contains the P4 source code implementing the SRv6 Live-Live behavior
- `sim`: contains the ns3-bmv2 simulator source code, that allows to the same P4 source code into ns3 (**STILL WIP**)

## Hands On

You need to install Kathará first. Follow the installation guide on the [official repository](https://github.com/KatharaFramework/Kathara/wiki/Installation-Guides).

### Run the Network Scenario

To run the network scenario and experiment by yourself:
```
make run
```

This will copy the `p4src` folder into the `lab` folder and start the scenario.

To clean the scenario:
```
make clean
```

### Run the Evaluation experiments

Go into the `experiments` folder and install the Python3 requirements:
```
python3 -m pip install -r requirements.txt
```
You need at least Python3.11 to make it work.

To reproduce the same experiments of the paper, from the root folder:
```
make experiments_emulator
```
This will create a folder `experiments/results` containing the results.

To plot the results:
```
make plot_experiments_emulator
```
This will create a folder `experiments/figures` containing the figures of the paper evaluation.

If you want to run the experiments manually:
```
experiment_n_path.py <result_path> <max_path> <n_runs>
```
This will run a scenario with an increasing amount of multipaths from source to destination with four different configurations:
- Single: represents solutions that steer the traffic over the best path that has been pre-compute
- Random: random multi-path selection
- No Deduplication: same as SRv6 Live-Live but without the final deduplication
- SRv6 Live-Live

Parameters:
- `<result_path>`: path where to store the experiment results 
- `<max_path>`: experiments will be run from 2 paths to `<max_path>` paths 
- `<n_runs>`: number of runs for each experiment