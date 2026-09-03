# ---------------------------------------------------------------------------
# build.ps1 -- build every solver with optimisation on, using a 64-bit compiler.
#
# The VS Code task in .vscode/tasks.json builds with C:\MinGW (32-bit GCC 6.3)
# and passes only -g.  That combination is why the searches were slow and why
# n = 8 was out of reach: no optimiser, and a 2 GB address space that cannot
# hold the 520 MiB distance array.  This script uses the MSYS2 UCRT64 toolchain
# (x86_64 GCC 15) with -O3 instead.
#
#   .\build.ps1              build everything
#   .\build.ps1 sortpairs    build one target
# ---------------------------------------------------------------------------

param([string[]]$Targets)

# Native compilers write warnings to stderr; PowerShell turns that into a
# terminating NativeCommandError under 'Stop'.  Judge success by exit code only.
$ErrorActionPreference = 'Continue'

$candidates = @(
    'C:\msys64\ucrt64\bin\g++.exe',
    'C:\msys64\mingw64\bin\g++.exe',
    'C:\msys64\clang64\bin\clang++.exe'
)

$cxx = $candidates | Where-Object { Test-Path $_ } | Select-Object -First 1
if (-not $cxx) {
    Write-Host "No 64-bit C++ compiler found. Install MSYS2 mingw-w64-ucrt-x86_64-gcc."
    exit 1
}

# The compiler needs its own bin directory on PATH to find its runtime DLLs.
$env:PATH = (Split-Path $cxx) + ';' + $env:PATH

$all = @('sortpairs', 'bidirectional_bfs_solver', 'center_out_v2', 'mine_traces')
if (-not $Targets) { $Targets = $all }

$flags = @('-O3', '-march=native', '-DNDEBUG', '-std=c++17', '-Wall', '-Wextra')

Write-Host "compiler: $cxx"
& $cxx --version | Select-Object -First 1

foreach ($t in $Targets) {
    $src = "$t.cpp"
    if (-not (Test-Path $src)) { Write-Warning "skipping $src (not found)"; continue }

    Write-Host "building $t ..." -NoNewline
    & $cxx @flags -o "$t.exe" $src
    if ($LASTEXITCODE -ne 0) { Write-Host " FAILED"; exit 1 }
    Write-Host " ok"
}

Write-Host ""
Write-Host "Try:"
Write-Host "  .\sortpairs.exe lb 12"
Write-Host "  .\sortpairs.exe selftest 6"
Write-Host "  .\sortpairs.exe exact 7"
Write-Host "  .\sortpairs.exe exact 8 --save path_n8.txt    # ~6 min, ~600 MB"
Write-Host "  .\mine_traces.exe --path 8 path_n8.txt"
