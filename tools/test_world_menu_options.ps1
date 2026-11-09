param(
    [ValidateRange(1, 1000)]
    [int]$Rounds = 4
)

$ErrorActionPreference = 'Stop'
$vpRepo = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$vpVcvars = 'C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvarsall.bat'
if (-not (Test-Path -LiteralPath $vpVcvars)) {
    throw "Visual Studio Build Tools not found: $vpVcvars"
}

$vpOutputRoot = Join-Path $vpRepo 'out\world-menu-options'
New-Item -ItemType Directory -Force -Path $vpOutputRoot | Out-Null
$vpExecutable = Join-Path $vpOutputRoot 'world_menu_options.exe'
$vpBatch = Join-Path $vpOutputRoot 'run-tests.bat'
$vpBatchLines = @(
    '@echo off',
    ('call "{0}" x86 >nul' -f $vpVcvars),
    'if errorlevel 1 exit /b %errorlevel%',
    ('cd /d "{0}"' -f $vpRepo),
    ('cl /nologo /O2 /arch:IA32 /fp:precise /MT /W3 /EHsc /std:c++17 /FC /DVP_MENU_FIXES test\world_menu_options.cpp hook\content_registry.cpp hook\content_catalog.cpp hook\content_game.cpp hook\content_manifest.cpp hook\content_sha256.cpp hook\content_preferences.cpp /Fo"{0}\\" /Fe"{1}" /link /BASE:0x10000000 /DYNAMICBASE:NO /FIXED /MACHINE:X86' -f $vpOutputRoot, $vpExecutable),
    'if errorlevel 1 exit /b %errorlevel%',
    'set VP_ONLY=HackOptionsControl::Added',
    ('"{0}" {1}' -f $vpExecutable, $Rounds),
    'if errorlevel 1 exit /b %errorlevel%',
    'set VP_ONLY=MenuDoRaceSetup',
    ('"{0}" {1}' -f $vpExecutable, $Rounds),
    'if errorlevel 1 exit /b %errorlevel%',
    'set VP_ONLY=',
    ('"{0}" {1}' -f $vpExecutable, $Rounds)
)
Set-Content -LiteralPath $vpBatch -Value $vpBatchLines -Encoding Ascii
& $vpBatch
if ($LASTEXITCODE -ne 0) {
    throw "world_menu_options tests failed (exit $LASTEXITCODE)"
}
Write-Host "world_menu_options fix-mode tests passed ($Rounds rounds)"
