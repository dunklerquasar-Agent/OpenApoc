<#
.SYNOPSIS
    Baut OpenApoc (Fork mit Zoom-Erweiterung) und erzeugt einen Windows-Installer.

.DESCRIPTION
    Dieses Skript baut das Spiel und packt einen NSIS-Installer
    (install-openapoc-<version>.exe).

    WICHTIG: Es werden KEINE Original-Spieldateien eingebunden. Der Installer fragt
    bei der Installation nach der CD bzw. ISO einer legal erworbenen X-COM: Apocalypse
    und trägt den Pfad in die Konfiguration ein. Das Spiel extrahiert die Daten beim
    ersten Start selbst aus dieser Quelle.

    Voraussetzungen:
      - Visual Studio 2022 (C++ Desktop-Entwicklung)
      - CMake (im PATH)
      - Git (im PATH)
      - NSIS 3.x  -> https://nsis.sourceforge.io/Download  (makensis im PATH)
      - vcpkg     -> https://github.com/microsoft/vcpkg  (VCPKG_INSTALLATION_ROOT gesetzt
                     oder über -VcpkgRoot angeben)

.PARAMETER BuildType
    Release (Standard), RelWithDebInfo oder Debug.

.PARAMETER VcpkgRoot
    Pfad zum vcpkg-Verzeichnis. Standard: Umgebungsvariable VCPKG_INSTALLATION_ROOT.

.PARAMETER SkipBuild
    Überspringt den Compile-Schritt und erzeugt den Installer aus einem
    bereits vorhandenen Build (nützlich für schnelle Iterationen am Installer).

.EXAMPLE
    .\build-installer.ps1
    .\build-installer.ps1 -BuildType RelWithDebInfo -VcpkgRoot C:\dev\vcpkg
#>

param(
    [ValidateSet("Release", "RelWithDebInfo", "Debug")]
    [string]$BuildType = "Release",

    [string]$VcpkgRoot = $env:VCPKG_INSTALLATION_ROOT,

    [switch]$SkipBuild
)

$ErrorActionPreference = "Stop"

function Write-Step([string]$Message)
{
    Write-Host ""
    Write-Host "=== $Message ===" -ForegroundColor Cyan
}

function Assert-Tool([string]$Name, [string]$Hint)
{
    $cmd = Get-Command $Name -ErrorAction SilentlyContinue
    if (-not $cmd)
    {
        throw "'$Name' nicht gefunden. $Hint"
    }
    return $cmd.Source
}

# ---------------------------------------------------------------------------
# 0) Umgebung pruefen
# ---------------------------------------------------------------------------
Write-Step "Pruefe Werkzeuge"
$repoRoot = (Get-Location).Path
if (-not (Test-Path (Join-Path $repoRoot "CMakeLists.txt")))
{
    throw "Bitte das Skript aus dem Repository-Wurzelverzeichnis aufrufen."
}

Assert-Tool "git"      "Git installieren: https://git-scm.com"
Assert-Tool "cmake"    "CMake installieren: https://cmake.org/download"
$makensis = Assert-Tool "makensis" "NSIS installieren: https://nsis.sourceforge.io/Download"

if ([string]::IsNullOrWhiteSpace($VcpkgRoot) -or -not (Test-Path $VcpkgRoot))
{
    throw "vcpkg nicht gefunden. Bitte -VcpkgRoot angeben oder VCPKG_INSTALLATION_ROOT setzen."
}
$vcpkgToolchain = Join-Path $VcpkgRoot "scripts/buildsystems/vcpkg.cmake"
if (-not (Test-Path $vcpkgToolchain))
{
    throw "vcpkg-Toolchain nicht gefunden unter: $vcpkgToolchain"
}
Write-Host "  git      : $((Get-Command git).Source)"
Write-Host "  cmake    : $((Get-Command cmake).Source)"
Write-Host "  makensis : $makensis"
Write-Host "  vcpkg    : $VcpkgRoot"

# ---------------------------------------------------------------------------
# 1) Versionsstring: DATUM-COMMITS-HASH  (Format erwartet von installer.nsi)
# ---------------------------------------------------------------------------
Write-Step "Ermittle Version"
$commitCount = (git rev-list --count HEAD).Trim()
$commitHash  = (git rev-parse --short HEAD).Trim()
$dateStamp   = Get-Date -Format "yyyyMMdd"
$version     = "$dateStamp-$commitCount-$commitHash"
Write-Host "  Version: $version"

$stagingDir = Join-Path $repoRoot "OpenApoc-$version"

# ---------------------------------------------------------------------------
# 2) Abhaengigkeiten
# ---------------------------------------------------------------------------
Write-Step "Submodule und vcpkg-Abhaengigkeiten"
git submodule update --init --recursive
if ($LASTEXITCODE -ne 0) { throw "git submodule fehlgeschlagen." }

# Manifest-Modus: nur die Basis-Abhaengigkeiten, OHNE das Qt-Feature fuer den Launcher.
# (Der Installer startet das Spiel direkt, der Launcher ist nicht noetig.)
Write-Host "  vcpkg install (kann beim ersten Mal laenger dauern) ..."
vcpkg install --x-manifest-root $repoRoot --triplet x64-windows
if ($LASTEXITCODE -ne 0) { throw "vcpkg install fehlgeschlagen." }

# ---------------------------------------------------------------------------
# 3) Konfigurieren und Bauen
#    EXTRACT_DATA=OFF  -> keine Spieldaten beim Bauen, diese kommen erst zur Laufzeit
#                         aus der vom Anwender angegebenen CD/ISO
#    BUILD_LAUNCHER=OFF -> kein Qt6 noetig, deutlich kuerzere Buildzeit
# ---------------------------------------------------------------------------
if (-not $SkipBuild)
{
    Write-Step "Konfiguriere CMake ($BuildType)"
    cmake -S $repoRoot -B (Join-Path $repoRoot "build") `
          -G "Visual Studio 17 2022" -A x64 `
          -DCMAKE_TOOLCHAIN_FILE="$vcpkgToolchain" `
          -DVCPKG_TARGET_TRIPLET="x64-windows" `
          -DEXTRACT_DATA=OFF `
          -DBUILD_LAUNCHER=OFF
    if ($LASTEXITCODE -ne 0) { throw "CMake-Konfiguration fehlgeschlagen." }

    Write-Step "Baue (das dauert einige Minuten)"
    cmake --build (Join-Path $repoRoot "build") --config $BuildType -- /m
    if ($LASTEXITCODE -ne 0) { throw "Build fehlgeschlagen." }
}

# ---------------------------------------------------------------------------
# 4) Staging-Ordner zusammenstellen (Erwartet von installer.nsi)
# ---------------------------------------------------------------------------
Write-Step "Stelle Installationsordner zusammen"
$binDir = Join-Path $repoRoot "build/bin"
if (-not (Test-Path $binDir))
{
    throw "Build-Ausgabe nicht gefunden: $binDir`nWurde der Build uebersprungen oder schlug er fehl?"
}

if (Test-Path $stagingDir) { Remove-Item $stagingDir -Recurse -Force }
New-Item -ItemType Directory -Path $stagingDir | Out-Null

# Binaries und Laufzeit-DLLs
Copy-Item (Join-Path $binDir "*") -Destination $stagingDir -Recurse -Force
Get-ChildItem $stagingDir -Include *.pdb,*.lib,*.exp,*.ilk -Recurse | Remove-Item -Force -ErrorAction SilentlyContinue

# Spiel- und Ressourcendaten des Repos (KEINE Original-Spieldateien)
Copy-Item (Join-Path $repoRoot "data") -Destination (Join-Path $stagingDir "data") -Recurse -Force
Copy-Item (Join-Path $repoRoot "README.md")          -Destination (Join-Path $stagingDir "README.txt") -Force
if (Test-Path (Join-Path $repoRoot "README_HOTKEYS.md"))
{
    Copy-Item (Join-Path $repoRoot "README_HOTKEYS.md") -Destination $stagingDir -Force
}

$version | Out-File -FilePath (Join-Path $stagingDir "build-id") -Encoding ascii
$commitHash | Out-File -FilePath (Join-Path $stagingDir "git-commit") -Encoding ascii

# Sicherstellen, dass wirklich keine Original-Spieldateien enthalten sind
$forbidden = Get-ChildItem $stagingDir -Recurse -Include *.iso,*.cue,*.bin,*.pck,*.tab -ErrorAction SilentlyContinue
if ($forbidden)
{
    throw "Im Staging-Ordner wurden Original-Spieldateien gefunden - Abbruch:`n$($forbidden.FullName -join "`n")"
}

Write-Host "  Staging: $stagingDir"

# ---------------------------------------------------------------------------
# 5) Installer erzeugen
# ---------------------------------------------------------------------------
Write-Step "Erzeuge Installer mit NSIS"
Push-Location $repoRoot
try
{
    & $makensis "/DGAME_VERSION=$version" "/DEXTRACT_DATA_OFF" (Join-Path $repoRoot "install\windows\installer.nsi")
    if ($LASTEXITCODE -ne 0) { throw "makensis fehlgeschlagen." }
}
finally
{
    Pop-Location
}

$installer = Join-Path $repoRoot "install\windows\install-openapoc-$version.exe"
if (-not (Test-Path $installer))
{
    throw "Installer wurde nicht erzeugt: $installer"
}

Write-Host ""
Write-Host "===================================================" -ForegroundColor Green
Write-Host " FERTIG" -ForegroundColor Green
Write-Host "===================================================" -ForegroundColor Green
Write-Host " Installer : $installer"
Write-Host " Groesse   : $([math]::Round((Get-Item $installer).Length / 1MB, 2)) MB"
Write-Host ""
Write-Host " Der Installer fragt bei der Installation nach deiner X-COM: Apocalypse"
Write-Host " CD bzw. ISO. Spieldateien sind NICHT enthalten."
