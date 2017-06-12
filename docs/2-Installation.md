## Installation

## Prerequisites

Low CAN binding must be installed. Prerequisites are the same.

```bash
$ git clone https://gerrit.automotivelinux.org/gerrit/apps/low-level-can-service
```

## Clone and build high level binding

### Build requirements

* CMake version 3.0 or later
* G++, Clang++ or any C++11 compliant compiler.

### Clone

```bash
$ export WD=$(pwd)
$ git clone https://github.com/iotbzh/high-level-viwi-service.git
```

### Build

```bash
$ cd $WD/high-level-viwi-service
$ mkdir build
$ cd build
$ cmake ..
$ make
```

## Launching

The Json high level configuration file *high.json* must be placed in the directory where you launch afb-daemon.

```bash
$ cp $WD/high-level-viwi-service/high.json $WD
$ cd $WD
```
Natively under linux you can launch afb-daemon with the low-level and high-level bindings with a command like:

```bash
$ cd $WD
$ afb-daemon --rootdir=$WD/low-level-can-service/CAN-binder/build/package --ldpaths=$WD/low-level-can-service/CAN-binder/build/package/lib:$WD/high-level-viwi-service/build/high-can-binding --port=1234 --tracereq=common --token=1 --verbose
```
