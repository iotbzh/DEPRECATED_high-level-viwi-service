## Installation

## Prerequisites

Low level CAN service (>=4.0) must be installed. Prerequisites are the same.

```bash
$ git clone --recursive https://gerrit.automotivelinux.org/gerrit/apps/low-level-can-service
```

## Clone and build high level binding

### Build requirements

* CMake version 3.0 or later
* G++, Clang++ or any C++11 compliant compiler.

### Clone

```bash
$ export WD=$(pwd)
$ git clone --recusive https://github.com/iotbzh/high-level-viwi-service.git
```

### Build

```bash
$ cd $WD/high-level-viwi-service
$ mkdir build
$ cd build
$ cmake ..
$ make
```

