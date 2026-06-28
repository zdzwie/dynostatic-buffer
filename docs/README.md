# Documentation

The documentation is built with [Sphinx](https://www.sphinx-doc.org) using the
[Read the Docs theme](https://sphinx-rtd-theme.readthedocs.io). Because Sphinx
does not parse C directly, the public API is documented with Doxygen and bridged
into Sphinx via [Breathe](https://breathe.readthedocs.io). `docs/conf.py` runs
Doxygen automatically, so only `sphinx-build` needs to be invoked.

## Requirements

- `doxygen` (system package)
- Python dependencies: `pip install -r docs/requirements.txt`

## Building

With CMake (uses the `docs` preset):

```sh
cmake --preset docs
cmake --build --preset docs
# Output: build/docs/sphinx/index.html
```

Or directly with Sphinx:

```sh
pip install -r docs/requirements.txt
sphinx-build -b html docs build/docs/sphinx
# Output: build/docs/sphinx/index.html
```

Online builds are configured in `.readthedocs.yaml`.
