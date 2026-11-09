param(
    [int]$QueriesPerTrack = 10000,
    [uint32]$Seed = 317,
    [string]$GameDirectory = '',
    [string]$CorpusDirectory = ''
)

$ErrorActionPreference = 'Stop'

$vpRepo = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$vpVcvars = 'C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvarsall.bat'
if (-not (Test-Path -LiteralPath $vpVcvars)) { throw "Visual Studio Build Tools not found: $vpVcvars" }
if ([string]::IsNullOrWhiteSpace($GameDirectory)) { $GameDirectory = Join-Path $vpRepo 'out\installed-viper-racing\Data' }
$GameDirectory = (Resolve-Path -LiteralPath $GameDirectory).Path
$vpRaceExe = Join-Path $vpRepo 'out\race_v10.exe'
if (-not (Test-Path -LiteralPath $vpRaceExe)) { throw "Reference v1.0 executable not found: $vpRaceExe" }
$vpOutput = Join-Path $vpRepo 'out\world-wld-terrain-parity'
New-Item -ItemType Directory -Force -Path $vpOutput | Out-Null
$vpCorpus = if ([string]::IsNullOrWhiteSpace($CorpusDirectory)) {
    $vpCorpusId = [guid]::NewGuid().ToString('N')
    Join-Path (Join-Path $vpRepo 'out\native-terrain-parity-corpus') ("run-{0}-{1}-{2}" -f $Seed, $QueriesPerTrack, $vpCorpusId)
} else { $CorpusDirectory }
New-Item -ItemType Directory -Force -Path $vpCorpus | Out-Null
$vpCorpus = (Resolve-Path -LiteralPath $vpCorpus).Path
$vpExecutable = Join-Path $vpOutput 'world_wld_terrain.exe'
$vpBatch = Join-Path $vpOutput 'build-and-run.bat'
$vpLines = @(
    '@echo off',
    ('call "{0}" x86 >nul' -f $vpVcvars),
    'if errorlevel 1 exit /b %errorlevel%',
    ('cd /d "{0}"' -f $vpRepo),
    ('cl /nologo /O2 /arch:IA32 /fp:precise /MT /W3 /EHsc /std:c++17 /FC /Fo"{0}\\" /Fe"{1}" test\world_wld_terrain.cpp native\native_track_info.cpp /link /BASE:0x10000000 /DYNAMICBASE:NO /FIXED /MACHINE:X86' -f $vpOutput, $vpExecutable),
    'if errorlevel 1 exit /b %errorlevel%',
    ('"{0}" {1} {2} "{3}" "{4}"' -f $vpExecutable, $QueriesPerTrack, $Seed, $GameDirectory, $vpCorpus)
)
Set-Content -LiteralPath $vpBatch -Value $vpLines -Encoding Ascii
& $vpBatch
if ($LASTEXITCODE -ne 0) { throw "Native terrain parity harness failed (exit $LASTEXITCODE)" }

$vpX64Executable = Join-Path $vpOutput 'native_track_parity_corpus_test.exe'
$vpX64Batch = Join-Path $vpOutput 'run-x64-parity.bat'
$vpX64Lines = @(
    '@echo off',
    ('call "{0}" x64 >nul' -f $vpVcvars),
    'if errorlevel 1 exit /b %errorlevel%',
    ('cd /d "{0}"' -f $vpRepo),
    ('cl /nologo /EHsc /std:c++17 /W4 /WX /Fo"{0}\\" /Fe"{1}" native\native_track_info.cpp hook\content_registry.cpp hook\content_manifest.cpp hook\content_sha256.cpp test\native_track_parity_corpus_test.cpp' -f $vpOutput, $vpX64Executable),
    'if errorlevel 1 exit /b %errorlevel%',
    ('"{0}" "{1}"' -f $vpX64Executable, $vpCorpus)
)
Set-Content -LiteralPath $vpX64Batch -Value $vpX64Lines -Encoding Ascii
& $vpX64Batch
if ($LASTEXITCODE -ne 0) { throw "Native x64 terrain parity check failed (exit $LASTEXITCODE)" }
