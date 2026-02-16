param(
    [string]$InstallPrefix = "C:/foo",
    [switch]$Ccache
)

$ErrorActionPreference = "Stop"

$ccacheFlags = @()
if ($Ccache) {
    $ccacheFlags = @("-DCMAKE_CXX_COMPILER_LAUNCHER=ccache", "-DCMAKE_C_COMPILER_LAUNCHER=ccache")
}

$checkoutDir = Get-Location
$workspaceDir = Split-Path $checkoutDir
$tag = git rev-parse --abbrev-ref HEAD

foreach ($shared in "On", "Off") {
    if ($shared -eq "On") { $label = "shared" } else { $label = "static" }

    # -- findpackage --
    Write-Output "::group::findpackage ($label)"
    mkdir build
    cd build
    cmake .. -GNinja -DCMAKE_BUILD_TYPE=Debug -DBUILD_SHARED_LIBS=$shared `
        -DCMAKE_INSTALL_PREFIX=$InstallPrefix -DCPPTRACE_WERROR_BUILD=On
    ninja install
    cd $workspaceDir
    cp -Recurse cpptrace/test/findpackage-integration .
    mkdir findpackage-integration/build
    cd findpackage-integration/build
    cmake .. -GNinja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_PREFIX_PATH=$InstallPrefix
    ninja
    .\main.exe
    Write-Output "::endgroup::"

    # -- add_subdirectory --
    Write-Output "::group::add_subdirectory ($label)"
    cd $workspaceDir
    cp -Recurse cpptrace/test/add_subdirectory-integration .
    cp -Recurse cpptrace add_subdirectory-integration
    mkdir add_subdirectory-integration/build
    cd add_subdirectory-integration/build
    cmake .. -GNinja -DCMAKE_BUILD_TYPE=Debug -DBUILD_SHARED_LIBS=$shared `
        -DCPPTRACE_WERROR_BUILD=On @ccacheFlags
    ninja
    .\main.exe
    Write-Output "::endgroup::"

    # -- fetchcontent --
    Write-Output "::group::fetchcontent ($label)"
    cd $workspaceDir
    cp -Recurse cpptrace/test/fetchcontent-integration .
    mkdir fetchcontent-integration/build
    cd fetchcontent-integration/build
    cmake .. -GNinja -DCMAKE_BUILD_TYPE=Debug -DCPPTRACE_TAG="$tag" `
        -DBUILD_SHARED_LIBS=$shared -DCPPTRACE_WERROR_BUILD=On @ccacheFlags
    ninja
    .\main.exe
    Write-Output "::endgroup::"

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
