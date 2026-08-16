option(BUILD_DOC "Build documentation" ON)

if (BUILD_DOC)
    find_package(Doxygen)
    if (DOXYGEN_FOUND)
        set(DOXYGEN_IN ${CMAKE_CURRENT_SOURCE_DIR}/../scripts/doxygen/Doxyfile)
        set(DOXYGEN_OUT ${CMAKE_CURRENT_BINARY_DIR}/docs)

        configure_file(${DOXYGEN_IN} ${DOXYGEN_OUT} @ONLY)
        message("Documentation generation started")

        add_custom_target( doc_doxygen ALL
            COMMAND ${DOXYGEN_EXECUTABLE} ${DOXYGEN_OUT}
            WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
            COMMENT "Generating API documentation with Doxygen"
            VERBATIM )
    else (DOXYGEN_FOUND)
      message("Doxygen need to be installed to generate the doxygen documentation")
    endif (DOXYGEN_FOUND)
endif (BUILD_DOC)

option(BUILD_SPHINX "Build Sphinx (Read the Docs) documentation" OFF)

# Which translation to build. English ("en") is the source language; set to a
# language with catalogs under docs/locale/<lang>/ (e.g. "pl") to build that
# translation. The docs-pl preset flips this to "pl".
set(SPHINX_LANGUAGE "en" CACHE STRING "Language to build the Sphinx docs in")

if (BUILD_SPHINX)
    find_program(SPHINX_BUILD_EXECUTABLE sphinx-build
                 DOC "Path to the sphinx-build executable")
    if (SPHINX_BUILD_EXECUTABLE)
        set(SPHINX_SOURCE ${CMAKE_SOURCE_DIR}/docs)
        set(SPHINX_OUTPUT ${CMAKE_BINARY_DIR}/sphinx)

        # docs/conf.py runs Doxygen itself to produce the XML Breathe reads, so
        # this target is self-contained and only invokes sphinx-build. Passing
        # -D language selects the translation; Sphinx compiles the matching .po
        # catalogs to .mo on the fly (a no-op for the "en" source language).
        add_custom_target( doc_sphinx ALL
            COMMAND ${SPHINX_BUILD_EXECUTABLE} -b html -D language=${SPHINX_LANGUAGE} ${SPHINX_SOURCE} ${SPHINX_OUTPUT}
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
            COMMENT "Generating documentation with Sphinx (Read the Docs theme), language=${SPHINX_LANGUAGE}"
            VERBATIM )
    else (SPHINX_BUILD_EXECUTABLE)
        message(WARNING "sphinx-build not found; install docs/requirements.txt to build the Sphinx documentation")
    endif (SPHINX_BUILD_EXECUTABLE)
endif (BUILD_SPHINX)
