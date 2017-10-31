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

# Bugs
- Discovery protocol will only discover a device if it reads the entire discovery packet in 1 recvfrom.
