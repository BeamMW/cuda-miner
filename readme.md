# Beam Equihash 150/5 CUDA Miner

```
Copyright 2018 The Beam Team
```

# Usage
## One Liner Examples
```
Linux: ./beam-cuda-miner --server <hostName>:<portNumer> --key <apiKey> --devices <deviceList>
Windows: beam-cuda-miner --server <hostName>:<portNumer> --key <apiKey> --devices <deviceList>
```

## Parameters
### --server 
Passes the address and port of the node the miner will mine on to the miner.
The server address can be an IP or any other valid server address.- For example when the node
is running on the same computer and listens on port 17000 then use --server localhost:17000

### --key
Pass a valid API key from "stratum.api.keys" to the miner. Required to authenticate the miner at the node

### --list-devices (Optional)
List all devices that are present in the system and their order start the miner.
Then all devices will be listed, but none selected for mining. 

### --devices (Optional)
Selects the devices to mine on. If not specified the miner will run on all devices found on the system. 
For example to mine on GPUs 0,1 and 3 but not number 2 use --devices 0,1,3
Best practice is to specify pciBusID:pciDeviceID to select a device: --devices 1:0,2:0
The miner closes when no devices were selected for mining or all selected miner fail in the compatibility check.

# How to build
## Windows
1. Install Visual Studio >= 2017 with CMake support.
2. Download and install OpenSSL prebuilt binaries https://slproweb.com/products/Win32OpenSSL.html (`Win64 OpenSSL v1.1.0h` for example).
3. Download and install CUDA Toolkit https://developer.nvidia.com/cuda-downloads
4. Open project folder in Visual Studio, select your target (`x64-Release` for example, if you downloaded 64bit OpenSSL) and select `CMake -> Build All`.
5. Go to `CMake -> Cache -> Open Cache Folder -> beam-cuda-miner` (you'll find `beam-cuda-miner.exe`).

## Linux (Ubuntu 14.04)
1. Install `gcc7` `ssl` packages.
```
  sudo add-apt-repository ppa:ubuntu-toolchain-r/test
  sudo apt update
  sudo apt install g++-7 libssl-dev -y
```
2. Set it up so the symbolic links `gcc`, `g++` point to the newer version:
```
  sudo update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-7 60 \
                           --slave /usr/bin/g++ g++ /usr/bin/g++-7 
  sudo update-alternatives --config gcc
  gcc --version
  g++ --version
```
3. Download and install CUDA Toolkit https://developer.nvidia.com/cuda-downloads
4. Install CMake 
```
  sudo apt install cmake
```
5. Go to beam-opencl-miner project folder, make directory `build`, change dir to `build` and call `cmake -DCMAKE_BUILD_TYPE=Release .. && make`.
```
  mkdir build
  cd build
  cmake -DCMAKE_BUILD_TYPE=Release .. 
  make
```
6. You'll find beam-cuda-miner binary in `build/equihash` folder.

