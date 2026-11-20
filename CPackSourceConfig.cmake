#[=======================================================================[.rst:
CPackSourceConfig
-----------------

Source archive packaging configuration for ``sed-windows``.

Excludes out-of-tree build directories, Git metadata, Vagrant virtual machine
state, cached toolchains, and coverage artifacts from source distributions.
#]=======================================================================]

set(CPACK_SOURCE_PACKAGE_FILE_NAME "sed-windows-${SED_VERSION}-src")
set(CPACK_SOURCE_GENERATOR "TGZ;ZIP")
set(CPACK_SOURCE_IGNORE_FILES
    "/\\.git/"
    "/build"
    "/build_"
    "/_deps/"
    "/sed/"
    "/\\.vagrant/"
    "/\\.watcom/"
    "/coverage"
    "\\.gcda$"
    "\\.gcno$"
    "\\.gcov$"
    "\\.info$"
    "\\.lst$"
    "\\.map$"
    "\\.err$"
    "\\.obj$"
    "\\.OBJ$"
    "\\.exe$"
    "\\.EXE$"
    "\\.DS_Store$"
)
