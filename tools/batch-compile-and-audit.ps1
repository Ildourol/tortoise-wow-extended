<#
.SYNOPSIS
    Batch compile and audit verification script for tortoise-wow-extended.
.DESCRIPTION
    Executes high-throughput batch compile and full link verification
    (modules.lib and mangosd.exe) across all CPU cores in parallel.
    Enforces the policy: commit one by one to git, push one by one as default,
    never do a batch commit, and verify with a single batch compile pass.
#>
[CmdletBinding()]
param(
    [Parameter()]
    [string]$Target = "mangosd",

    [Parameter()]
    [string]$Config = "Release",

    [Parameter()]
    [switch]$ModulesOnly,

    [Parameter()]
    [switch]$Quiet
)

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$RepoRoot = Split-Path -Parent $ScriptDir
$BuildDir = Join-Path $RepoRoot "build"

$Cores = if ($env:NUMBER_OF_PROCESSORS) { [int]$env:NUMBER_OF_PROCESSORS } else { 4 }
$cmake = "C:/vcpkg/downloads/tools/cmake-4.4.2-windows/cmake-4.4.2-windows-x86_64/bin/cmake.exe"
if (-not (Test-Path $cmake)) {
    $cmake = "cmake.exe"
}

Write-Host "============================================================" -ForegroundColor Cyan
Write-Host " BATCH COMPILE AND AUDIT (tortoise-wow-extended)" -ForegroundColor Cyan
Write-Host "============================================================" -ForegroundColor Cyan
Write-Host " Policy: Commits must be applied 1-by-1 to Git (never batch commit)." -ForegroundColor Yellow
Write-Host "         Pushes to origin/playerbots are 1-by-1 by default." -ForegroundColor Yellow
Write-Host "         Compilation is batched to avoid repetitive MSVC LTCG passes." -ForegroundColor Gray
Write-Host ""

# Step 1: Check Git working tree and branch
$branch = & git -C "$RepoRoot" branch --show-current 2>&1
$unpushed = & git -C "$RepoRoot" log "origin/$branch..HEAD" --oneline 2>&1
$unpushedCount = if ($unpushed) { @($unpushed).Count } else { 0 }
Write-Host ">>> Git Status on branch '$branch':" -ForegroundColor Cyan
Write-Host "    Unpushed commits ahead of origin: $unpushedCount" -ForegroundColor Gray

# Step 2: Build modules.lib
Write-Host "`n>>> Compiling modules.lib ($Cores cores)..." -ForegroundColor Cyan
$start = Get-Date
$vFlag = if ($Quiet) { @("--", "/nologo", "/v:q") } else { @() }
& $cmake --build "$BuildDir" --config $Config --target modules --parallel $Cores $vFlag
if ($LASTEXITCODE -ne 0) {
    Write-Host "[FAIL] modules.lib compilation failed with exit code $LASTEXITCODE" -ForegroundColor Red
    exit $LASTEXITCODE
}
$elapsedMod = [Math]::Round(((Get-Date) - $start).TotalSeconds, 1)
Write-Host "[PASS] modules.lib compiled cleanly in ${elapsedMod}s." -ForegroundColor Green

if ($ModulesOnly) {
    Write-Host "[DONE] Modules-only verification complete." -ForegroundColor Green
    exit 0
}

# Step 3: Build mangosd.exe
Write-Host "`n>>> Linking mangosd.exe ($Cores cores)..." -ForegroundColor Cyan
$startFull = Get-Date
$vFlagFull = if ($Quiet) { @("--", "/nologo", "/v:m") } else { @() }
& $cmake --build "$BuildDir" --config $Config --target mangosd --parallel $Cores $vFlagFull
if ($LASTEXITCODE -ne 0) {
    Write-Host "[FAIL] mangosd.exe build/link failed with exit code $LASTEXITCODE" -ForegroundColor Red
    exit $LASTEXITCODE
}
$elapsedFull = [Math]::Round(((Get-Date) - $startFull).TotalSeconds, 1)
Write-Host "[PASS] mangosd.exe linked cleanly in ${elapsedFull}s." -ForegroundColor Green

Write-Host "`n============================================================" -ForegroundColor Cyan
Write-Host " BATCH COMPILE AND AUDIT PASSED (Total: $($elapsedMod + $elapsedFull)s)" -ForegroundColor Green
Write-Host "============================================================" -ForegroundColor Cyan
