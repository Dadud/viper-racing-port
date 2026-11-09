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
        $vpLayoutExecutable = Join-Path $vpArchOutput 'native_ui_layout_test.exe'
        $vpMenuExecutable = Join-Path $vpArchOutput 'native_menu_model_test.exe'
        $vpModelExecutable = Join-Path $vpArchOutput 'native_model_info_test.exe'
        $vpTrackExecutable = Join-Path $vpArchOutput 'native_track_info_test.exe'
        $vpBatch = Join-Path $vpArchOutput 'run-tests.bat'
        $vpBatchLines = @(
            '@echo off',
            ('call "{0}" {1} >nul' -f $vpVcvars, $vpArch),
            'if errorlevel 1 exit /b %errorlevel%',
            ('cd /d "{0}"' -f $vpRepo),
            ('cl /nologo /EHsc /std:c++17 /W4 /WX /Fo"{0}\\" /Fe"{1}" {2}' -f $vpArchOutput, $vpExecutable, $vpSources),
            'if errorlevel 1 exit /b %errorlevel%',
            ('"{0}"' -f $vpExecutable),
            ('cl /nologo /EHsc /std:c++17 /W4 /WX /Fo"{0}\\" /Fe"{1}" test\\native_ui_layout_test.cpp' -f $vpArchOutput, $vpLayoutExecutable),
            'if errorlevel 1 exit /b %errorlevel%',
            ('"{0}"' -f $vpLayoutExecutable),
            ('cl /nologo /EHsc /std:c++17 /W4 /WX /Fo"{0}\\" /Fe"{1}" native\\native_menu_model.cpp native\\native_resource_store.cpp native\\native_model_info.cpp native\\native_track_info.cpp hook\\content_registry.cpp hook\\content_catalog.cpp hook\\content_game.cpp hook\\content_manifest.cpp hook\\content_sha256.cpp hook\\content_preferences.cpp test\\native_menu_model_test.cpp' -f $vpArchOutput, $vpMenuExecutable),
            'if errorlevel 1 exit /b %errorlevel%',
            ('"{0}"' -f $vpMenuExecutable),
            ('cl /nologo /EHsc /std:c++17 /W4 /WX /Fo"{0}\\" /Fe"{1}" native\\native_model_info.cpp hook\\content_registry.cpp hook\\content_manifest.cpp hook\\content_sha256.cpp test\\native_model_info_test.cpp' -f $vpArchOutput, $vpModelExecutable),
            'if errorlevel 1 exit /b %errorlevel%',
            ('"{0}"' -f $vpModelExecutable),
            ('cl /nologo /EHsc /std:c++17 /W4 /WX /Fo"{0}\\" /Fe"{1}" native\\native_track_info.cpp hook\\content_registry.cpp hook\\content_manifest.cpp hook\\content_sha256.cpp test\\native_track_info_test.cpp' -f $vpArchOutput, $vpTrackExecutable),
            'if errorlevel 1 exit /b %errorlevel%',
            ('"{0}"' -f $vpTrackExecutable)
        )
        Set-Content -LiteralPath $vpBatch -Value $vpBatchLines -Encoding Ascii
        & $vpBatch
        if ($LASTEXITCODE -ne 0) {
            throw "Content catalog tests failed for $vpArch (exit $LASTEXITCODE)"
        }
        Write-Host "Content catalog, native UI layout, and menu model tests passed for $vpArch"
        foreach ($vpInstall in @('installed-viper-racing', 'content-ui-v124-test', 'content-ui-v125-test', 'content-ui-v126-test')) {
            $vpDataDirectory = Join-Path $vpRepo (Join-Path (Join-Path 'out' $vpInstall) 'Data')
            if (Test-Path -LiteralPath $vpDataDirectory -PathType Container) {
                & $vpTrackExecutable $vpDataDirectory
                if ($LASTEXITCODE -ne 0) { throw "Native BPP asset parsing failed for $vpInstall ($vpArch)" }
            }
        }
    }
}
finally {
    Pop-Location
}
