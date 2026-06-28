"""Sphinx configuration for the dynostatic-buffer documentation.

Sphinx cannot parse C on its own, so the public API is documented with
Doxygen and bridged into Sphinx via Breathe. To keep a single source of
truth (and to work unchanged on Read the Docs, where only ``sphinx-build``
runs), this file drives Doxygen itself: it reuses the project Doxyfile and
overrides only the few settings that differ for the XML/Breathe flow.
"""

import subprocess
from pathlib import Path

# -- Paths -------------------------------------------------------------------
DOCS_DIR = Path(__file__).resolve().parent
REPO_ROOT = DOCS_DIR.parent
DOXYFILE = REPO_ROOT / "scripts" / "doxygen" / "Doxyfile"

# Where Doxygen's XML (the input Breathe turns into Sphinx pages) is written.
DOXYGEN_OUTPUT_DIR = DOCS_DIR / "_doxygen"
DOXYGEN_XML_DIR = DOXYGEN_OUTPUT_DIR / "xml"


def generate_doxygen_xml() -> None:
    """Run Doxygen to produce the XML consumed by Breathe.

    The project Doxyfile is reused as-is and only the settings that matter
    for the Sphinx flow are appended (Doxygen keeps the last value for a
    tag, so these overrides win): an absolute input path, XML output and no
    HTML output.
    """
    overrides = "\n".join(
        [
            f"INPUT = {REPO_ROOT / 'library'}",
            f"OUTPUT_DIRECTORY = {DOXYGEN_OUTPUT_DIR}",
            "GENERATE_HTML = NO",
            "GENERATE_XML = YES",
            "XML_OUTPUT = xml",
            "QUIET = YES",
        ]
    )
    config = DOXYFILE.read_text() + "\n" + overrides + "\n"
    subprocess.run(
        ["doxygen", "-"],
        input=config,
        text=True,
        check=True,
        cwd=REPO_ROOT,
    )


generate_doxygen_xml()

# -- Project information ------------------------------------------------------
project = "dynostatic-buffer"
copyright = "2022, Jakub Brzezowski"
author = "Jakub Brzezowski"
release = "0.1"

# -- General configuration ---------------------------------------------------
extensions = ["breathe"]

exclude_patterns = ["_build", "_doxygen", "Thumbs.db", ".DS_Store"]

# -- Breathe configuration ---------------------------------------------------
breathe_projects = {project: str(DOXYGEN_XML_DIR)}
breathe_default_project = project
breathe_domain_by_extension = {"h": "c"}

# -- HTML output -------------------------------------------------------------
html_theme = "sphinx_rtd_theme"
