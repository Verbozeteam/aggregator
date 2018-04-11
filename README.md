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

### libwebsocket
Repository can be found [here](https://github.com/warmcat/libwebsockets).
```
    git clone https://github.com/warmcat/libwebsockets.git
    cd libwebsockets
    mkdir build
    # ==================
    # FOR MAC:
    cmake .. OPENSSL_ROOT_DIR=/usr/local/opt/openssl/
    # NON-MAC:
    cmake ..
    # ==================
    make
    sudo make install
```

# Bugs
- I don't know
