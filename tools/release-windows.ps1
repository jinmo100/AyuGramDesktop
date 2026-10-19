[CmdletBinding()]
param(
    [string]$Repo = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path,
    [string]$OutputRoot = 'E:\TBuild\fork-release',
    [string]$TBuildRoot = 'E:\TBuild',
    [string]$Python = 'python',
    [string]$SourceSha,
    [string]$QtVersion = '5.15.19',
    [string]$MsvcVersion = '14.44',
    [string]$Tag,
    [switch]$Resume
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
function Invoke-Checked([string]$Command, [string[]]$Arguments) {
    $script:LogSequence++
    $step = '{0:D2}-{1}' -f $script:LogSequence, [IO.Path]::GetFileNameWithoutExtension($Command)
    $logPath = Join-Path $script:LogDirectory ($script:LogRun + '-' + $step + '.log')
    Write-Host "Running $step; log: $logPath"
    $resolvedCommand = Get-Command $Command -ErrorAction Stop
    $previousPreference = $ErrorActionPreference
    try {
        $ErrorActionPreference = 'Continue'
        & $resolvedCommand @Arguments 2>&1 | ForEach-Object {
            $line = [string]$_
            if ($line -match '(?i)TDESKTOP_API_(ID|HASH)') { '[API setting redacted]' }
            else { $line -replace '(?i)\b[0-9a-f]{32}\b', '[API hash redacted]' }
        } | Out-File -LiteralPath $logPath -Encoding utf8 -ErrorAction Stop
        $exitCode = $LASTEXITCODE
    } finally {
        $ErrorActionPreference = $previousPreference
    }
    if ($exitCode -ne 0) { throw "Required command failed: $step (exit $exitCode). See $logPath" }
}
function Assert-Stopped([string]$Executable) {
    $running = Get-CimInstance Win32_Process -Filter "Name = 'AyuGram.exe'" | Where-Object { $_.ExecutablePath -eq $Executable }
    if ($running) { throw 'The exact checkout AyuGram.exe is running; close it and retry' }
}
$repo = (Resolve-Path $Repo).Path
$out = [IO.Path]::GetFullPath($OutputRoot)
$tbuild = [IO.Path]::GetFullPath($TBuildRoot).TrimEnd('\')
$build = Join-Path $repo 'out'
$expected = Join-Path $build 'Debug\AyuGram.exe'
foreach ($path in @($out, $build, $tbuild)) {
    if ([IO.Path]::GetPathRoot($path) -ine 'E:\') { throw 'All build and release output must be on E:' }
}
if ($tbuild -ine 'E:\TBuild') { throw 'TBuildRoot must be E:\TBuild' }
if (-not $out.StartsWith($tbuild + '\', [StringComparison]::OrdinalIgnoreCase)) { throw 'OutputRoot must be under E:\TBuild' }
if ($out -eq $repo -or $out.StartsWith($repo + '\', [StringComparison]::OrdinalIgnoreCase)) { throw 'OutputRoot must be outside the source checkout' }
if ($Resume -and -not $Tag) { throw '-Resume requires -Tag' }
if ($MsvcVersion -notmatch '^14\.\d+(\.\d+)?$') { throw 'Invalid MSVC toolset version' }
$lockPath = & git -C $repo rev-parse --git-path fork-release-wrapper.lock
if ($LASTEXITCODE -ne 0) { throw 'Source is not a Git checkout' }
$lockPath = $lockPath.Trim()
if (-not [IO.Path]::IsPathRooted($lockPath)) { $lockPath = Join-Path $repo $lockPath }
$lock = [IO.File]::Open($lockPath, [IO.FileMode]::CreateNew, [IO.FileAccess]::Write, [IO.FileShare]::None)
try {
    $env:TEMP = Join-Path $out 'Temp'
    $env:TMP = $env:TEMP
    $env:CCACHE_DIR = Join-Path $out 'ccache'
    $env:NUGET_PACKAGES = Join-Path $out 'nuget\packages'
    $env:NUGET_HTTP_CACHE_PATH = Join-Path $out 'nuget\http'
    $env:NUGET_PLUGINS_CACHE_PATH = Join-Path $out 'nuget\plugins'
    $env:TBUILD_ROOT = $tbuild
    New-Item -ItemType Directory -Force -Path $env:TEMP,$env:CCACHE_DIR,$env:NUGET_PACKAGES,$env:NUGET_HTTP_CACHE_PATH,$env:NUGET_PLUGINS_CACHE_PATH | Out-Null
    $script:LogDirectory = Join-Path $out 'logs'
    New-Item -ItemType Directory -Force -Path $script:LogDirectory | Out-Null
    $script:LogRun = [DateTime]::UtcNow.ToString('yyyyMMddTHHmmss') + '-' + [Guid]::NewGuid().ToString('N')
    $script:LogSequence = 0
    $publisher = Join-Path $repo 'tools\fork_release.py'
    $sha = & git -C $repo rev-parse HEAD
    if ($LASTEXITCODE -ne 0) { throw 'Cannot resolve source commit' }
    $sha = $sha.Trim()
    if ($SourceSha -and $SourceSha -ne $sha) { throw 'HEAD differs from requested source SHA' }
    Invoke-Checked $Python @($publisher, '--repo', $repo, '--source-sha', $sha, '--preflight-only')
    Assert-Stopped $expected
    $manifest = Join-Path $out 'build-proof.json'
    $compilerFiles = @(Get-ChildItem -Path (Join-Path $build 'CMakeFiles\*\CMakeCXXCompiler.cmake'))
    if ($compilerFiles.Count -ne 1) { throw 'Configured compiler evidence is missing or ambiguous' }
    $compilerText = Get-Content $compilerFiles[0].FullName -Raw
    if ($compilerText -notmatch 'set\(CMAKE_CXX_COMPILER "[^"\r\n]*/MSVC/(14\.\d+\.\d+)/bin/Hostx64/x64/cl.exe"\)') { throw 'Configured x64 MSVC compiler is missing' }
    $configuredMsvc = $Matches[1]
    if (-not ($configuredMsvc -eq $MsvcVersion -or $configuredMsvc.StartsWith($MsvcVersion + '.'))) { throw 'Configured out compiler differs from -MsvcVersion' }
    if ($Resume) {
        $proofConfig = (Get-Content $manifest -Raw | ConvertFrom-Json).validation_config
        if ($proofConfig.msvc_version -ne $configuredMsvc -or $proofConfig.qt_version -ne $QtVersion -or $proofConfig.tbuild_root.Replace('/', '\').TrimEnd('\') -ine $tbuild) { throw 'Resume proof differs from requested validation configuration' }
        Invoke-Checked $Python @($publisher, '--repo', $repo, '--source-sha', $sha, '--build-manifest', $manifest, '--verify-proof-only')
    } else {
        $started = [DateTimeOffset]::UtcNow.ToUnixTimeSeconds()
        $cmake = Join-Path $build 'CMakeCache.txt'
        if (-not (Test-Path $cmake)) { throw 'Configured out tree is missing' }
        $cacheText = Get-Content $cmake -Raw
        if ($cacheText -notmatch '(?m)^CMAKE_GENERATOR_TOOLSET:INTERNAL=(v143|.*version=14\.[^\r\n]+)\r?$') { throw 'Configured out toolset is unsupported' }
        $vswhere = Get-Command vswhere -ErrorAction SilentlyContinue
        $vswherePath = if ($vswhere) { $vswhere.Source } else { Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe' }
        if (-not (Test-Path $vswherePath)) { throw 'vswhere is missing' }
        $install = & $vswherePath -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
        if ($LASTEXITCODE -ne 0 -or -not $install) { throw 'MSVC x64 installation not found' }
        $devshell = Join-Path $install 'Common7\Tools\Microsoft.VisualStudio.DevShell.dll'
        if (-not (Test-Path $devshell)) { throw 'Visual Studio DevShell module is missing' }
        Import-Module $devshell
        Enter-VsDevShell -VsInstallPath $install -Arch amd64 -HostArch amd64 -SkipAutomaticLocation -DevCmdArguments "-vcvars_ver=$configuredMsvc" *> $null
        if (-not $?) { throw 'Visual Studio shell initialization failed' }
        if (-not $env:VCToolsVersion -or $env:VCToolsVersion.TrimEnd('\') -ne $configuredMsvc) { throw 'Active MSVC differs from configured toolset' }
        $cmakeCommand = Get-Command cmake -ErrorAction SilentlyContinue
        $cmakeExe = if ($cmakeCommand) { $cmakeCommand.Source } else { Join-Path $install 'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe' }
        if (-not (Test-Path $cmakeExe)) { throw 'Visual Studio CMake is missing' }
        Assert-Stopped $expected
        Invoke-Checked $cmakeExe @('-S', $repo, '-B', $build, '-DDESKTOP_APP_DISABLE_AUTOUPDATE=ON', '-DTDESKTOP_FORK_RELEASES_URL:STRING=https://github.com/jinmo100/AyuGramDesktop/releases')
        $buildStarted = [DateTimeOffset]::UtcNow.ToUnixTimeSeconds()
        Invoke-Checked $cmakeExe @('--build', $build, '--config', 'Debug', '--target', 'Telegram', '--', "/p:VCToolsVersion=$configuredMsvc")
        $buildFinished = [DateTimeOffset]::UtcNow.ToUnixTimeSeconds()
        if (-not (Test-Path $expected)) { throw 'AyuGram.exe is missing' }
        Assert-Stopped $expected
        $layout = Join-Path $repo '.scratch\channel-top-posts\test_layout_regression.py'
        $layoutSource = Join-Path $out 'layout-regression'
        Invoke-Checked $Python @($layout, $repo, $layoutSource)
        $layoutBuild = Join-Path $layoutSource 'build'
        Invoke-Checked $cmakeExe @('-S', $layoutSource, '-B', $layoutBuild, '-A', 'x64', '-T', "version=$configuredMsvc", "-DTEST_QT_VERSION=$QtVersion", "-DTBUILD_ROOT=$tbuild")
        Invoke-Checked $cmakeExe @('--build', $layoutBuild, '--config', 'Debug', '--', "/p:VCToolsVersion=$configuredMsvc")
        $layoutExe = Join-Path $layoutBuild 'Debug\layout_regression.exe'
        if (-not (Test-Path $layoutExe)) { throw 'Layout regression executable is missing' }
        Invoke-Checked $layoutExe @()
        Assert-Stopped $expected
        $cache = @{}
        $keys = @('DESKTOP_APP_DISABLE_AUTOUPDATE', 'TDESKTOP_FORK_RELEASES_URL', 'CMAKE_HOME_DIRECTORY', 'CMAKE_GENERATOR', 'CMAKE_GENERATOR_PLATFORM', 'CMAKE_GENERATOR_TOOLSET', 'CMAKE_CONFIGURATION_TYPES', 'CMAKE_MSVC_RUNTIME_LIBRARY')
        foreach ($line in Get-Content $cmake) {
            if ($line -match '^([^:#=]+):[^=]+=(.*)$' -and $Matches[1] -in $keys) { $cache[$Matches[1]] = $Matches[2] }
        }
        $artifactHash = (Get-FileHash $expected -Algorithm SHA256).Hash.ToLowerInvariant()
        $finished = [DateTimeOffset]::UtcNow.ToUnixTimeSeconds()
        $proof = @{
            source_sha = $sha; artifact = $expected; artifact_sha256 = $artifactHash
            build_succeeded = $true; layout_succeeded = $true; process_checked = $true
            started_at = $started; finished_at = $finished
            build_started_at = $buildStarted; build_finished_at = $buildFinished
            cache = $cache
            validation_config = @{ msvc_version = $configuredMsvc; qt_version = $QtVersion; tbuild_root = $tbuild }
        } | ConvertTo-Json -Depth 4
        [IO.File]::WriteAllText($manifest, $proof, (New-Object Text.UTF8Encoding($false)))
    }
    Assert-Stopped $expected
    $publishArgs = @($publisher, '--repo', $repo, '--build-manifest', $manifest, '--source-sha', $sha)
    if ($Tag) { $publishArgs += @('--tag', $Tag) }
    if ($Resume) { $publishArgs += '--resume' }
    Invoke-Checked $Python $publishArgs
    Write-Output 'Release publication completed.'
} finally {
    $lock.Dispose()
    Remove-Item -LiteralPath $lockPath
}
