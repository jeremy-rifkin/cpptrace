param(
    [string]$InstallPrefix = "C:/foo",
    [switch]$Ccache
)

$ErrorActionPreference = "Stop"

$ccacheFlags = @()
if ($Ccache) {
    $ccacheFlags = @(
        "-DCMAKE_CXX_COMPILER_LAUNCHER=ccache",
        "-DCMAKE_C_COMPILER_LAUNCHER=ccache",
        "-DCMAKE_POLICY_DEFAULT_CMP0141=NEW",
        "-DCMAKE_MSVC_DEBUG_INFORMATION_FORMAT=Embedded"
    )
}

$checkoutDir = Get-Location
$workspaceDir = Split-Path $checkoutDir
$tag = git rev-parse --abbrev-ref HEAD

# Share FetchContent downloads across builds so deps are only fetched once
$depsDir = "$workspaceDir/deps-cache"
$depsCacheFlags = @()

foreach ($shared in "On", "Off") {
    if ($shared -eq "On") { $label = "shared" } else { $label = "static" }

    # -- findpackage --
    Write-Output "::group::findpackage ($label)"
    $sw = [System.Diagnostics.Stopwatch]::StartNew()
    mkdir build
    cd build
    cmake .. -GNinja -DCMAKE_BUILD_TYPE=Debug "-DBUILD_SHARED_LIBS=$shared" `
        "-DCMAKE_INSTALL_PREFIX=$InstallPrefix" -DCPPTRACE_WERROR_BUILD=On `
        "-DFETCHCONTENT_BASE_DIR=$depsDir" @depsCacheFlags @ccacheFlags
    # After the first configure populates deps, point subsequent builds at the
    # already-extracted source dirs so cmake skips re-downloading entirely.
    $depsCacheFlags = @(
        "-DFETCHCONTENT_SOURCE_DIR_ZSTD=$depsDir/zstd-src",
        "-DFETCHCONTENT_SOURCE_DIR_LIBDWARF=$depsDir/libdwarf-src"
    )
    ninja install
    cd $workspaceDir
    cp -Recurse cpptrace/test/findpackage-integration .
    mkdir findpackage-integration/build
    cd findpackage-integration/build
    cmake .. -GNinja -DCMAKE_BUILD_TYPE=Debug "-DCMAKE_PREFIX_PATH=$InstallPrefix" @ccacheFlags
    ninja
    .\main.exe
    $sw.Stop()
    Write-Output "::endgroup::"
    Write-Output "findpackage ($label) completed in $([math]::Round($sw.Elapsed.TotalSeconds))s"

    # -- add_subdirectory --
    Write-Output "::group::add_subdirectory ($label)"
    $sw = [System.Diagnostics.Stopwatch]::StartNew()
    cd $workspaceDir
    cp -Recurse cpptrace/test/add_subdirectory-integration .
    cp -Recurse cpptrace add_subdirectory-integration
    mkdir add_subdirectory-integration/build
    cd add_subdirectory-integration/build
    cmake .. -GNinja -DCMAKE_BUILD_TYPE=Debug "-DBUILD_SHARED_LIBS=$shared" `
        -DCPPTRACE_WERROR_BUILD=On @depsCacheFlags @ccacheFlags
    ninja
    .\main.exe
    $sw.Stop()
    Write-Output "::endgroup::"
    Write-Output "add_subdirectory ($label) completed in $([math]::Round($sw.Elapsed.TotalSeconds))s"

    # -- fetchcontent --
    Write-Output "::group::fetchcontent ($label)"
    $sw = [System.Diagnostics.Stopwatch]::StartNew()
    cd $workspaceDir
    cp -Recurse cpptrace/test/fetchcontent-integration .
    mkdir fetchcontent-integration/build
    cd fetchcontent-integration/build
    cmake .. -GNinja -DCMAKE_BUILD_TYPE=Debug -DCPPTRACE_TAG="$tag" `
        "-DBUILD_SHARED_LIBS=$shared" -DCPPTRACE_WERROR_BUILD=On `
        "-DFETCHCONTENT_BASE_DIR=$depsDir" @depsCacheFlags @ccacheFlags
    ninja
    .\main.exe
    $sw.Stop()
    Write-Output "::endgroup::"
    Write-Output "fetchcontent ($label) completed in $([math]::Round($sw.Elapsed.TotalSeconds))s"

    # -- cleanup --
    Write-Output "::group::cleanup ($label)"
    cd $checkoutDir
    Remove-Item -Recurse -Force build
    Remove-Item -Recurse -Force $InstallPrefix
    Remove-Item -Recurse -Force $workspaceDir/findpackage-integration
    Remove-Item -Recurse -Force $workspaceDir/add_subdirectory-integration
    Remove-Item -Recurse -Force $workspaceDir/fetchcontent-integration
    Write-Output "::endgroup::"
}
