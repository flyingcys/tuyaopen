```shell
$ sudo apt-get install -y cmake ninja-build
$ cmake -S . -B build && cmake --build build --verbose
$ cmake -S . -B build -G "Ninja" && ninja -C build --verbose
```