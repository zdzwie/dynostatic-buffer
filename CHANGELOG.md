# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added

- Tracking of requested memory size and alignment size in the memory management functions.
- Adding Ceedling framework for unit testing to enhance test coverage and reliability.

### Fixed

- Fixed memory allocation problem when requested by user memory size is different than alignment size.

## [1.0.0] - 2026.08.16

### Added
- Added support for advanced memory management features.
- Implement properly working malloc functions with improved performance and reliability.
- Implement properly working free functions with improved performance and reliability.
- Implement properly working realloc functions with improved performance and reliability.
- Implement properly working calloc functions with improved performance and reliability.
- Implemented a comprehensive test suite for all memory management functions.
- Adding sphinx documentation for the project in english and polish languages.
- Adding safe memory copy and set functions to enhance memory manipulation capabilities.
- Adding support for memory alignment to improve performance and compatibility with different architectures.
- Adding support for Bazel build system for efficient and scalable builds.
- Adding support for traditional Makefile build system for compatibility with legacy systems.
- Adding support for compilation support for clang and MSVC compilers to ensure cross-platform compatibility.
- Adding formatting by clang-format to maintain consistent code style and improve readability.
- Adding checking by clang-tidy to identify and fix potential issues in the codebase.
- Adding checking by cppcheck to perform static analysis and detect potential bugs and vulnerabilities.
- Adding checking by MISRA:2012 to ensure compliance with industry standards for safety-critical systems.

## [0.1.0] - 2022.07.24

### Added
- Initial release of the project with basic functionality and features.
- Implemented core functionality of malloc functions
- Adding Google Test framework for unit testing
- Adding CMake build system for cross-platform compatibility
- Implemented tests for TDD paradigm

[unreleased]: https://github.com/zdzwie/dynostatic-buffer/compare/v1.0...HEAD
[1.0.0]: https://github.com/zdzwie/dynostatic-buffer/compare/v0.1...v1.0
[0.1.0]: https://github.com/zdzwie/dynostatic-buffer/releases/tag/v0.1
