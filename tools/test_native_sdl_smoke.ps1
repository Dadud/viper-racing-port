$ErrorActionPreference = 'Stop'

$vpRepo = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$vpBuild = Join-Path $vpRepo 'native\build_windows_x64.bat'
$vpExecutable = Join-Path $vpRepo 'out\native-x64\viper-native.exe'
$vpGameDirectory = Join-Path $vpRepo 'out\native-smoke-game'

& cmd.exe /c "`"$vpBuild`""
if ($LASTEXITCODE -ne 0) { throw "Native Windows x64 build failed with exit $LASTEXITCODE" }
New-Item -ItemType Directory -Force -Path (Join-Path $vpGameDirectory 'Config\ContentLibrary') | Out-Null
New-Item -ItemType Directory -Force -Path (Join-Path $vpGameDirectory 'Data') | Out-Null

$vpPreviousDriver = $env:SDL_VIDEODRIVER
$env:SDL_VIDEODRIVER = 'dummy'
try {
    & $vpExecutable --game-dir $vpGameDirectory --smoke-test
    if ($LASTEXITCODE -ne 0) { throw "Native SDL menu smoke test failed with exit $LASTEXITCODE" }
}
finally {
    if ($null -eq $vpPreviousDriver) {
        Remove-Item Env:SDL_VIDEODRIVER -ErrorAction SilentlyContinue
    }
    else {
        $env:SDL_VIDEODRIVER = $vpPreviousDriver
    }
}
