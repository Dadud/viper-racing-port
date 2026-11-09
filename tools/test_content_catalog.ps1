$ErrorActionPreference = 'Stop'

$vpRepo = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$vpVcvars = 'C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvarsall.bat'
if (-not (Test-Path -LiteralPath $vpVcvars)) {
    throw "Visual Studio Build Tools not found: $vpVcvars"
}

$vpOutputRoot = Join-Path $vpRepo 'out-portable\windows'
$vpSources = @(
    'hook\content_registry.cpp',
    'hook\content_catalog.cpp',
    'hook\content_game.cpp',
    'hook\content_manifest.cpp',
    'hook\content_sha256.cpp',
    'hook\content_preferences.cpp',
    'test\content_registry_test.cpp'
) -join ' '

Push-Location $vpRepo
try {
    foreach ($vpArch in @('x86', 'x64')) {
        $vpArchOutput = Join-Path $vpOutputRoot $vpArch
        New-Item -ItemType Directory -Force -Path $vpArchOutput | Out-Null
        $vpExecutable = Join-Path $vpArchOutput 'content_registry_test.exe'
        $vpBatch = Join-Path $vpArchOutput 'run-tests.bat'
        $vpBatchLines = @(
            '@echo off',
            ('call "{0}" {1} >nul' -f $vpVcvars, $vpArch),
            'if errorlevel 1 exit /b %errorlevel%',
            ('cd /d "{0}"' -f $vpRepo),
            ('cl /nologo /EHsc /std:c++17 /W4 /WX /Fo"{0}\\" /Fe"{1}" {2}' -f $vpArchOutput, $vpExecutable, $vpSources),
            'if errorlevel 1 exit /b %errorlevel%',
            ('"{0}"' -f $vpExecutable)
        )
        Set-Content -LiteralPath $vpBatch -Value $vpBatchLines -Encoding Ascii
        & $vpBatch
        if ($LASTEXITCODE -ne 0) {
            throw "Content catalog tests failed for $vpArch (exit $LASTEXITCODE)"
        }
        Write-Host "Content catalog tests passed for $vpArch"
    }
}
finally {
    Pop-Location
}
