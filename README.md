# IPFIXcol - new design

## Building

```
$ mkdir build && cd build
$ cmake ..
$ make
# make install
```

## Building tests

```
$ mkdir build && cd build
$ cmake .. -DCMAKE_BUILD_TYPE=Debug -DENABLE_UNIT_TESTS=YES
$ make
$ make test
```

To enable memory check of the Unit tests, append `cmake` command with 
`-DENABLE_VALGRIND_TESTS=YES`.
