param(
    [string]$Target = "all"
)

$ErrorActionPreference = "Stop"

$GCC = "C:\msys64\ucrt64\bin\gcc.exe"
if (-not (Test-Path $GCC)) {
    $GCC = (Get-Command gcc -ErrorAction SilentlyContinue).Source
}

if (-not $GCC) {
    Write-Error "GCC compiler not found! Please ensure GCC is installed and on PATH."
    exit 1
}

$IncludeDir = "$PSScriptRoot\include"
$SrcDir = "$PSScriptRoot\src"
$BinDir = "$PSScriptRoot\bin"

if (-not (Test-Path $BinDir)) {
    New-Item -ItemType Directory -Path $BinDir -Force | Out-Null
}

$CoreSources = @(
    "$SrcDir\mem_pal.c",
    "$SrcDir\mem_alloc.c",
    "$SrcDir\mem_roots.c",
    "$SrcDir\mem_collector.c"
)

$VmSources = $CoreSources + @("$SrcDir\mem_vm.c")

$CFlags = @(
    "-std=c23",
    "-Wall",
    "-Wextra",
    "-O2",
    "-I$IncludeDir"
)

Write-Host "Compiling Memory Collector Engine with GCC ($GCC)..." -ForegroundColor Cyan

$Demos = @(
    @{ Name = "demo_basic"; Sources = $CoreSources + @("$PSScriptRoot\examples\demo_basic.c") },
    @{ Name = "demo_cycles"; Sources = $CoreSources + @("$PSScriptRoot\examples\demo_cycles.c") },
    @{ Name = "demo_promotion"; Sources = $CoreSources + @("$PSScriptRoot\examples\demo_promotion.c") },
    @{ Name = "demo_compaction"; Sources = $CoreSources + @("$PSScriptRoot\examples\demo_compaction.c") },
    @{ Name = "demo_vm"; Sources = $VmSources + @("$PSScriptRoot\examples\demo_vm.c") }
)

foreach ($demo in $Demos) {
    $outPath = "$BinDir\$($demo.Name).exe"
    Write-Host "Building $outPath..." -ForegroundColor Green
    & $GCC @CFlags @($demo.Sources) -o $outPath
}

Write-Host "`n[SUCCESS] Build complete! 5 executables generated in: $BinDir" -ForegroundColor Cyan
