#!/usr/bin/env bash

for flag in $1 $2 $3 $4
do
    case $flag in
        --install-mage-deps)
            # Install apt packages
            sudo apt update
            sudo apt install -y git build-essential clang cmake libssl-dev libaio-dev

            # Install yaml-cpp version 0.63
            wget https://github.com/jbeder/yaml-cpp/archive/yaml-cpp-0.6.3.tar.gz
            tar zxf yaml-cpp-0.6.3.tar.gz
            pushd yaml-cpp-yaml-cpp-0.6.3
            mkdir build
            pushd build
            cmake -DYAML_BUILD_SHARED_LIBS=ON ..
            make -j 2
            sudo make install
            popd
            popd

            # Install tfhe
            git clone https://github.com/tfhe/tfhe
            pushd tfhe
            make -j 2
            sudo make install
            popd

            # Install SEAL
            wget https://github.com/microsoft/SEAL/archive/v3.6.1.tar.gz
            tar zxf v3.6.1.tar.gz
            pushd SEAL-3.6.1
            cmake -S . -B build -DSEAL_USE_ZLIB=OFF -DBUILD_SHARED_LIBS=ON
            cmake --build build -j 2
            sudo cmake --install build
            popd

            # Update shared libraries
            sudo ldconfig
            ;;
        --install-utils)
            # Other useful tools for experimentation
            sudo apt install -y tmux iperf3 python3 htop net-tools cgroup-tools

            # EMP-Toolkit dependencies
            sudo apt install -y cmake git build-essential libssl-dev libgmp-dev libboost-all-dev
            ;;
        --setup-wan-tcp)
            # Modify /etc/sysctl.conf to widen the TCP windows for WAN experiments
            echo "# Increase TCP buffer sizes for WAN experiments" | sudo tee -a /etc/sysctl.conf
            echo "net.core.rmem_max = 67108864" | sudo tee -a /etc/sysctl.conf
            echo "net.core.wmem_max = 67108864" | sudo tee -a /etc/sysctl.conf
            echo "net.ipv4.tcp_rmem = 4096 87380 33554432" | sudo tee -a /etc/sysctl.conf
            echo "net.ipv4.tcp_wmem = 4096 65536 33554432" | sudo tee -a /etc/sysctl.conf

            # Modify /etc/sysctl.conf to use frequent TCP keepalives so that WAN firewalls don't drop idle connections
            echo "# Use frequent TCP keepalives for WAN experiments" | sudo tee -a /etc/sysctl.conf
            echo "net.ipv4.tcp_keepalive_time = 240" | sudo tee -a /etc/sysctl.conf
            echo "net.ipv4.tcp_keepalive_intvl = 65" | sudo tee -a /etc/sysctl.conf
            echo "net.ipv4.tcp_keepalive_probes = 5" | sudo tee -a /etc/sysctl.conf
            ;;
        *)
            echo "Unknown command-line flag" $flag
    esac
done
