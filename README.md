# aggregator
Aggregates data from kernels

# Building
## Requirements
### Boost
To install boost, [download it](http://www.boost.org/users/download/), extract it and and run:
```
    ./bootstrap.sh
    ./b2 install
```
### JSON for modern C++
To install [JSON for modern C++](https://github.com/nlohmann/json):
```
    brew tap nlohmann/json
    brew install nlohmann_json
```

### C++ Rest Framework (Casablanca)
Follow the instructions [here](https://github.com/Microsoft/cpprestsdk). For mac just
```
    brew install cpprestsdk
```
For Linux (Raspberry Pi Raspbian Jessie)
```
    sudo apt-get install g++ zlib1g-dev libssl-dev cmake
    git clone https://github.com/Microsoft/cpprestsdk.git casablanca
    cd casablanca/Release
    mkdir build.release
    cd build.release
    cmake .. -DCMAKE_BUILD_TYPE=Release
    sed -i 's/-Werror//g' src/CMakeFiles/cpprest.dir/flags.make
    make
    sudo make install
```

# Bugs
- I don't know
