# dynostatic-buffer

[![CI](https://github.com/zdzwie/dynostatic-buffer/actions/workflows/ci.yml/badge.svg)](https://github.com/zdzwie/dynostatic-buffer/actions/workflows/ci.yml)
[![codecov](https://codecov.io/gh/zdzwie/dynostatic-buffer/branch/master/graph/badge.svg)](https://codecov.io/gh/zdzwie/dynostatic-buffer)
[![docs](https://img.shields.io/badge/docs-online-blue)](https://zdzwie.github.io/dynostatic-buffer/)

Implementation of dynamic memory allocation in predefined static buffer.

## Documentation

The full API documentation is built with [Sphinx](https://www.sphinx-doc.org)
(Read the Docs theme) and published to GitHub Pages on every push to `master`:

**https://zdzwie.github.io/dynostatic-buffer/**

To build it locally:

```sh
pip install -r docs/requirements.txt   # requires doxygen to be installed too
cmake --preset docs
cmake --build --preset docs
# Output: build/docs/sphinx/index.html
```

See [`docs/README.md`](docs/README.md) for details.
