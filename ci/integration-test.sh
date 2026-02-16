#!/bin/bash
set -eo pipefail

INSTALL_PREFIX="${1:?Usage: $0 <install-prefix> [--ccache]}"
CCACHE_FLAGS=""
if [ "${2:-}" = "--ccache" ]; then
    CCACHE_FLAGS="-DCMAKE_CXX_COMPILER_LAUNCHER=ccache -DCMAKE_C_COMPILER_LAUNCHER=ccache"
fi

CHECKOUT_DIR="$(pwd)"
WORKSPACE_DIR="$(dirname "$CHECKOUT_DIR")"
TAG="$(git rev-parse --abbrev-ref HEAD)"

# Share FetchContent downloads across builds so deps are only fetched once
DEPS_DIR="$WORKSPACE_DIR/deps-cache"

for SHARED in On Off; do
    if [ "$SHARED" = "On" ]; then LABEL="shared"; else LABEL="static"; fi

    # -- findpackage --
    echo "::group::findpackage ($LABEL)"
    SECONDS=0
    mkdir build && cd build
    cmake .. -GNinja -DCMAKE_BUILD_TYPE=Debug -DBUILD_SHARED_LIBS=$SHARED \
        -DCMAKE_INSTALL_PREFIX="$INSTALL_PREFIX" -DCPPTRACE_WERROR_BUILD=On \
        -DFETCHCONTENT_BASE_DIR="$DEPS_DIR" $CCACHE_FLAGS
    ninja install
    cd "$WORKSPACE_DIR"
    cp -rv cpptrace/test/findpackage-integration .
    mkdir findpackage-integration/build && cd findpackage-integration/build
    cmake .. -GNinja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_PREFIX_PATH="$INSTALL_PREFIX" $CCACHE_FLAGS
    ninja
    ./main
    echo "::endgroup::"
    echo "findpackage ($LABEL) completed in ${SECONDS}s"

    # -- add_subdirectory --
    echo "::group::add_subdirectory ($LABEL)"
    SECONDS=0
    cd "$WORKSPACE_DIR"
    cp -rv cpptrace/test/add_subdirectory-integration .
    cp -rv cpptrace add_subdirectory-integration
    mkdir add_subdirectory-integration/build && cd add_subdirectory-integration/build
    cmake .. -GNinja -DCMAKE_BUILD_TYPE=Debug -DBUILD_SHARED_LIBS=$SHARED \
        -DCPPTRACE_WERROR_BUILD=On -DFETCHCONTENT_BASE_DIR="$DEPS_DIR" $CCACHE_FLAGS
    ninja
    ./main
    echo "::endgroup::"
    echo "add_subdirectory ($LABEL) completed in ${SECONDS}s"

    # -- fetchcontent --
    echo "::group::fetchcontent ($LABEL)"
    SECONDS=0
    cd "$WORKSPACE_DIR"
    cp -rv cpptrace/test/fetchcontent-integration .
    mkdir fetchcontent-integration/build && cd fetchcontent-integration/build
    cmake .. -GNinja -DCMAKE_BUILD_TYPE=Debug -DCPPTRACE_TAG="$TAG" \
        -DBUILD_SHARED_LIBS=$SHARED -DCPPTRACE_WERROR_BUILD=On \
        -DFETCHCONTENT_BASE_DIR="$DEPS_DIR" $CCACHE_FLAGS
    ninja
    ./main
    echo "::endgroup::"
    echo "fetchcontent ($LABEL) completed in ${SECONDS}s"

    # -- cleanup --
    echo "::group::cleanup ($LABEL)"
    cd "$CHECKOUT_DIR"
    rm -rf build "$INSTALL_PREFIX" \
        "$WORKSPACE_DIR/findpackage-integration" \
        "$WORKSPACE_DIR/add_subdirectory-integration" \
        "$WORKSPACE_DIR/fetchcontent-integration"
    echo "::endgroup::"
done
