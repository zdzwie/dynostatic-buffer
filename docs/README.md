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

## Translations

The docs are internationalized with Sphinx's gettext workflow. English is the
source language; translated strings live in per-language catalogs under
`docs/locale/<lang>/LC_MESSAGES/*.po` (Polish, `pl`, is provided). Sphinx
compiles the `.po` files to `.mo` automatically at build time, so only the
`.po` sources are tracked in git.

Preview a translated build locally with the `docs-pl` CMake preset:

```sh
cmake --preset docs-pl
cmake --build --preset docs-pl
# Output: build/docs-pl/sphinx/index.html
```

Or directly with Sphinx (the preset just wraps this):

```sh
sphinx-build -b html -D language=pl docs build/docs/sphinx-pl
# Output: build/docs/sphinx-pl/index.html
```

When the English `.rst` sources change, refresh the catalogs so translators see
the new or modified strings (needs `sphinx-intl`, in `requirements.txt`):

```sh
# 1. Extract the up-to-date translatable strings.
sphinx-build -b gettext docs docs/_build/gettext
# 2. Merge them into every language catalog (add more with -l <lang>).
sphinx-intl update -p docs/_build/gettext -d docs/locale -l pl
```

Then translate any empty/`fuzzy` `msgstr` entries in the updated `.po` files.

### Read the Docs hosting

Read the Docs builds one language per project and links them together, so a
translation is served as its own project pointed at this same repository. No
per-language build configuration is needed: both projects reuse this
`.readthedocs.yaml` and `docs/conf.py`, and RTD injects the language from each
project's own setting (equivalent to `sphinx-build -D language=pl`). The
repository only has to carry the committed `.po` catalogs — which it does.

To publish the Polish docs, in the Read the Docs dashboard:

1. Import a second project from this repository, e.g. `dynostatic-buffer-pl`
   (it may share the repo URL with the English project). In its **Settings**,
   set **Language** to *Polish*.
2. On the main (English) project, open **Settings → Translations** and add the
   Polish project as a translation.

Read the Docs then rebuilds both and shows a language switcher (`/en/`, `/pl/`)
in the flyout menu on the site.
