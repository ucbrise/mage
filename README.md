MAGE: Memory-Aware Garbling Engine
==================================

MAGE is a memory-aware garbling engine. It is written in C++. To build, use g++ version 9 or later or clang++ version 6 or later.

How to Build
------------
This is still in progress, but it's far enough along that you can run the code.

To build MAGE, first install OpenSSL. Also install version 0.63 of the yaml-cpp library (https://github.com/jbeder/yaml-cpp).

On a Ubuntu install, you can install the dependencies by running `apt install build-essential clang cmake libssl-dev libaio-dev`. Then you have to install the yaml-cpp library, using the `-DYAML_BUILD_SHARED_LIBS=ON` option when running `cmake`.

Once you've done this, you should be able to run `make`. I've tested this on an Ubuntu 16.04 system. You will obtain three executables: `mage`, `planner`, and `example_input`.

How to Run
----------
`planner` produces a memory program for one of the example programs built-in to the executable. `example_input` produces a file containing the input to a program. `mage` executes a memory program using secure multiparty computation.

To run the Aspirin Count problem, first create a `config.yaml` file. Here is an example of one that should work:
```
page_shift: 12
num_pages: 1024
prefetch_buffer_size: 256
prefetch_lookahead: 10000

garbler:
    workers:
        - internal_host: localhost
          internal_port: 50000
          external_host: localhost
          external_port: 54321
          storage_path: garbler_swapfile_1

evaluator:
    workers:
        - internal_host: localhost
          internal_port: 60000
          external_host: localhost
          external_port: 54324
          storage_path: evaluator_swapfile_1
```
The internal host/port won't matter on a single machine. A page shift of 12 corresponds to 64 KiB pages, so this config will ask MAGE to run the program within 64 MiB of memory.

As a quick check that everything works correctly, run `./planner aspirin ../config.yaml garbler 0 128` and `./example_input 128 1`. Here, 128 is the size of the program. You can increase it for a larger computation, but it should be a power of 2 (the code isn't written to work with anything else). Then run, in two separate terminal windows:
`./mage ../config.yaml evaluate 0 aspirin_128`
`./mage ../config.yaml garble 0 aspirin_128`
Make sure to start the evaluator first. If you hexdump the output file, you should see `0xff`.

License
-------
The code in this repository is open-source under version 3 of the GNU General Public License (GPL).
