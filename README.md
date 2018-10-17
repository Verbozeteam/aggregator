# aggregator
Aggregates data from kernels

# Building
## Requirements
### Boost
To install boost 1.65.x, [download it](http://www.boost.org/users/download/), extract it and and run:
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
    mkdir build && cd build
    # ==================
    # FOR MAC:
    cmake .. -OPENSSL_ROOT_DIR=/usr/local/opt/openssl/
    # NON-MAC:
    cmake ..
    # ==================
    make
    sudo make install
```

# Generating self-signed certificate
Command: `openssl req -newkey rsa:2048 -nodes -keyout sslkey.pem -x509 -days 7300 -out sslcert.pem -subj '/CN=www.verboze.com/O=Verboze QSTP-LLC./C=QA'`

# Bugs
- I don't know
