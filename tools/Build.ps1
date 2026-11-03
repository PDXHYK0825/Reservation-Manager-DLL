[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$QtRoot,
    [ValidateSet('DataStatistics','Reservation','AttendanceViolation','Application')]
    [string[]]$Module = @('DataStatistics','Reservation','AttendanceViolation','Application'),
    [ValidateSet('Debug','Release')][string[]]$Configuration = @('Debug','Release')
)
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$QtRoot = (Resolve-Path -LiteralPath $QtRoot).Path
Push-Location -LiteralPath $root
try { $relativeQt = Resolve-Path -LiteralPath $QtRoot -Relative }
finally { Pop-Location }
if ([System.IO.Path]::IsPathRooted($relativeQt)) {
    throw 'Keep the Qt SDK and project on the same drive to use a relative SDK path.'
}
$escapedQt = [System.Security.SecurityElement]::Escape($relativeQt)
@"
<?xml version="1.0" encoding="utf-8"?>
<Project xmlns="http://schemas.microsoft.com/developer/msbuild/2003"><PropertyGroup><QtSdkRoot>`$(MSBuildThisFileDirectory)$escapedQt</QtSdkRoot></PropertyGroup></Project>
"@ | Set-Content -LiteralPath (Join-Path $root 'QtPaths.props') -Encoding UTF8
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$vsPath = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $vsPath) { throw 'Install the VS2022 C++ desktop development tools.' }
$msbuild = Join-Path $vsPath 'MSBuild\Current\Bin\amd64\MSBuild.exe'
New-Item -ItemType Directory -Force -Path (Join-Path $root 'verification') | Out-Null

function Invoke-Recorded {
    param([string]$Executable, [string[]]$Arguments, [string]$LogPath)
    "Command: $Executable $($Arguments -join ' ')" | Tee-Object -FilePath $LogPath
    $previousPreference = $ErrorActionPreference
    try {
        $ErrorActionPreference = 'Continue'
        & $Executable @Arguments 2>&1 | Tee-Object -FilePath $LogPath -Append
        $nativeExit = $LASTEXITCODE
    } finally { $ErrorActionPreference = $previousPreference }
    "Exit code: $nativeExit" | Add-Content -LiteralPath $LogPath
    if ($nativeExit -ne 0) { throw "Command failed. See $LogPath" }
}

$savedPath = $env:PATH
$savedPlatform = $env:QT_QPA_PLATFORM
$savedPluginPath = $env:QT_QPA_PLATFORM_PLUGIN_PATH
try {
    $env:PATH = "$(Join-Path $QtRoot 'bin');$savedPath"
    foreach ($name in @('DataStatistics','Reservation','AttendanceViolation','Application')) {
        if ($Module -notcontains $name) { continue }
        $solution = if ($name -eq 'Application') {
            Join-Path $root 'StudyRoomReservation.sln'
        } else { Join-Path $root "modules\$name\$name.sln" }
        foreach ($config in $Configuration) {
            Invoke-Recorded $msbuild @($solution, '/m:1', '/nr:false', '/nologo', '/v:minimal', "/p:Configuration=$config", '/p:Platform=x64') (Join-Path $root "verification\build-$name-$config.txt")
            $test = if ($name -eq 'Application') { 'IntegrationTests' } else { "${name}Tests" }
            $exe = Join-Path $root "bin\$test\x64\$config\$test.exe"
            $xml = Join-Path $root "verification\tests-$name-$config.xml"
            if ($name -eq 'Application') {
                $env:QT_QPA_PLATFORM = 'windows'
                $env:QT_QPA_PLATFORM_PLUGIN_PATH = Join-Path $QtRoot 'plugins\platforms'
            }
            Invoke-Recorded $exe @('-o', "$xml,junitxml", '-o', '-,txt') (Join-Path $root "verification\test-$name-$config.txt")
            [xml]$result = Get-Content -LiteralPath $xml -Raw
            if ([int]$result.testsuite.failures -ne 0 -or [int]$result.testsuite.errors -ne 0 -or [int]$result.testsuite.skipped -ne 0) { throw "$test has failing or skipped cases." }
        }
    }
} finally {
    $env:PATH = $savedPath
    $env:QT_QPA_PLATFORM = $savedPlatform
    $env:QT_QPA_PLATFORM_PLUGIN_PATH = $savedPluginPath
}
Write-Host 'Requested DLL builds and tests passed.'
