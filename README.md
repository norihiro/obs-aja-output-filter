# OBS AJA Output Filter

## Introduction

This is a simple plugin for OBS Studio that provides a filter to send video to AJA output

## Properties

## Build and install
### Linux
Use cmake to build on Linux. After checkout, run these commands.
```
sed -i 's;${CMAKE_INSTALL_FULL_LIBDIR};/usr/lib;' CMakeLists.txt
mkdir build && cd build
cmake -DCMAKE_INSTALL_PREFIX=/usr ..
make
sudo make install
```

### macOS
Use cmake to build on Linux. After checkout, run these commands.
```
mkdir build && cd build
cmake ..
make
```

## See also
- [obs-decklink-output-filter](https://github.com/cg2121/obs-decklink-output-filter)
