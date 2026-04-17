# Purpose of this project
This project is aimed at reading BUNDLE archives used in Burnout Paradise.
The project is used in [libapt2](https://github.com/Bo98/libapt2) but might be helpful for others for usage.

# Build status
[![Build Status](https://travis-ci.org/Bo98/libbndl.svg?branch=master)](https://travis-ci.org/Bo98/libbndl)
[![Build status](https://ci.appveyor.com/api/projects/status/9ek63lhv0inwcxxr?svg=true)](https://ci.appveyor.com/project/Bo98/libbndl)

# How to build

```sh
$ mkdir build && cd build
$ cmake ..
$ cmake --build .
```

# How to use the library

```c++
#include <libbndl/bundle.hpp>
#include <iostream>

int main(int argc,char** argv)
{
    // Create a bundle instance
    libbndl::Bundle arch;
    // Load the archive
    arch.Load(argv[1]);
    // Load an entry from the archive
    EntryData *entry = arch.GetBinary(argv[2]);
    // etc.
    // Remember to delete the EntryData and its data members.
    // ...
}
```
