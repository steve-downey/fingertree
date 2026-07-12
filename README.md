# beman.fingertree: A Persistent Sequence Container

<!--
SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-->

<!-- markdownlint-disable line-length -->
[![Library Status](https://raw.githubusercontent.com/bemanproject/beman/refs/heads/main/images/badges/beman_badge-beman_library_under_development.svg)](https://github.com/bemanproject/beman/blob/main/docs/beman_library_maturity_model.md#the-beman-library-maturity-model)
[![Continuous Integration Tests](https://github.com/bemanproject/fingertree/actions/workflows/ci_tests.yml/badge.svg)](https://github.com/bemanproject/fingertree/actions/workflows/ci_tests.yml)
[![Lint Check (pre-commit)](https://github.com/bemanproject/fingertree/actions/workflows/pre-commit-check.yml/badge.svg)](https://github.com/bemanproject/fingertree/actions/workflows/pre-commit-check.yml)
[![Coverage](https://coveralls.io/repos/github/bemanproject/fingertree/badge.svg?branch=main)](https://coveralls.io/github/bemanproject/fingertree?branch=main)
![Standard Target](https://github.com/bemanproject/beman/blob/main/images/badges/cpp29.svg)
<!-- markdownlint-restore -->

`beman.fingertree` is (... TODO: description).

**Implements**: `std::todo` proposed in [TODO (PnnnnRr)](https://wg21.link/PnnnnRr).

**Status**: [Under development and not yet ready for production use.](https://github.com/bemanproject/beman/blob/main/docs/beman_library_maturity_model.md#under-development-and-not-yet-ready-for-production-use)

## License

`beman.fingertree` is licensed under the Apache License v2.0 with LLVM Exceptions.

The design notes under [`docs/`](docs/) are licensed under
[CC BY-SA 4.0](LICENSE.CC-BY-SA-4.0); each carries its own SPDX identifier.

## Usage

TODO

Full runnable examples can be found in [`examples/`](examples/).

## Dependencies

### Build Environment

This project requires at least the following to build:

* A C++ compiler that conforms to the C++20 standard or greater
* CMake 3.30 or later
* (Test Only) GoogleTest

You can disable building tests by setting CMake option `BEMAN_FINGERTREE_BUILD_TESTS` to
`OFF` when configuring the project.

### Supported Platforms

| Compiler   | Version | C++ Standards | Standard Library  |
|------------|---------|---------------|-------------------|
| GCC        | 16-13   | C++26-C++17   | libstdc++         |
| GCC        | 12-11   | C++23-C++17   | libstdc++         |
| Clang      | 22-19   | C++26-C++17   | libstdc++, libc++ |
| Clang      | 18      | C++26-C++17   | libc++            |
| Clang      | 18      | C++23-C++17   | libstdc++         |
| Clang      | 17      | C++26-C++17   | libc++            |
| Clang      | 17      | C++20, C++17  | libstdc++         |
| AppleClang | latest  | C++26-C++17   | libc++            |
| MSVC       | latest  | C++23         | MSVC STL          |

## Design notes

The implementation choices are documented under [`docs/`](docs/):

* [Compile-time design](docs/compile-time-design.md) --- why the spine is a
  uniform type and a runtime variant rather than the textbook nested type, and
  the probe test that keeps it that way.
* [Allocator design](docs/allocator-design.md) --- how structural sharing and
  the Lakos coherency rule are reconciled, and how the PMR layer stays
  zero-global-allocation.
* [Measure-based splitting](docs/measure-based-splitting.md) --- splitting on
  accumulated measures, the three split methods, and how the split walks the
  spine.

## Development

See the [Contributing Guidelines](CONTRIBUTING.md).

## Integrate beman.fingertree into your project

### Build

You can build fingertree using a CMake workflow preset:

```bash
cmake --workflow --preset gcc-release
```

To list available workflow presets, you can invoke:

```bash
cmake --list-presets=workflow
```

For details on building beman.fingertree without using a CMake preset, refer to the
[Contributing Guidelines](CONTRIBUTING.md).

### Installation

#### Vcpkg

The preferred way to install fingertree is via vcpkg. To do so, after installing vcpkg
itself, you need to add support for the Beman project's [vcpkg
registry](https://github.com/bemanproject/vcpkg-registry) by configuring a
`vcpkg-configuration.json` file (which fingertree [provides](vcpkg-configuration.json)).

Then, simply run `vcpkg install beman-fingertree`.

#### Manual

To install beman.fingertree globally after building with the `gcc-release` preset, you can
run:

```bash
sudo cmake --install build/gcc-release
```

Alternatively, to install to a prefix, for example `/opt/beman`, you can run:

```bash
sudo cmake --install build/gcc-release --prefix /opt/beman
```

This will generate the following directory structure:

```txt
/opt/beman
├── include
│   └── beman
│       └── fingertree
│           ├── fingertree.hpp
│           └── ...
└── lib
    └── cmake
        └── beman.fingertree
            ├── beman.fingertree-config-version.cmake
            ├── beman.fingertree-config.cmake
            └── beman.fingertree-targets.cmake
```

### CMake Configuration

If you installed beman.fingertree to a prefix, you can specify that prefix to your CMake
project using `CMAKE_PREFIX_PATH`; for example, `-DCMAKE_PREFIX_PATH=/opt/beman`.

You need to bring in the `beman.fingertree` package to define the `beman::fingertree` CMake
target:

```cmake
find_package(beman.fingertree REQUIRED)
```

You will then need to add `beman::fingertree` to the link libraries of any libraries or
executables that include `beman.fingertree` headers.

```cmake
target_link_libraries(yourlib PUBLIC beman::fingertree)
```

### Using beman.fingertree

To use `beman.fingertree` in your C++ project,
include an appropriate `beman.fingertree` header from your source code.

```c++
#include <beman/fingertree/fingertree.hpp>
```
