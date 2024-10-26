# AJA Output Filter plugin for OBS Studio

## Introduction

This is a simple plugin for OBS Studio that provides a filter to send video to AJA output

## Properties

## Build and install
### Linux
Make sure `libobsConfig.cmake` is found by cmake.
After checkout, run these commands.
```
mkdir build && cd build
cmake \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DCMAKE_INSTALL_LIBDIR=/usr/lib \
	..
make -j2
sudo make install
```
You might need to adjust `CMAKE_INSTALL_LIBDIR` for your system.

### macOS
Make sure `libobsConfig.cmake` is found by cmake.
After checkout, run these commands.
```
mkdir build && cd build
cmake \
	-DLIBOBS_LIB=<path to libobs.0.dylib> \
	..
make -j2
```
Finally, copy `obs-aja-output-filter.so` and `data` to the obs-plugins folder.

## See also
- [obs-decklink-output-filter](https://github.com/cg2121/obs-decklink-output-filter)
