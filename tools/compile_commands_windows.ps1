$ErrorActionPreference = 'Stop'

$vpRepo = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$vpHook = Join-Path $vpRepo 'hook'
$vpSdl = Join-Path $vpHook '..\..\sdl2\SDL2-2.32.10'
$vpOut = Join-Path $vpRepo 'out\clang-audit'
$vpObjectDir = Join-Path $vpOut 'obj'
New-Item -ItemType Directory -Force -Path $vpObjectDir | Out-Null

# Keep this list and the flags aligned with hook/build.bat's cl invocation.
$vpPatterns = @(
    'viperport.cpp', 'port.cpp', 'replay.cpp', 'session.cpp', 'standalone.cpp',
    'content_registry.cpp', 'content_catalog.cpp', 'content_game.cpp', 'content_manifest.cpp',
    'content_sha256.cpp', 'content_preferences.cpp', 'net_*.cpp', 'crt_*.cpp', 'edit_*.cpp',
    'phys_*.cpp', 'wld_*.cpp', 'krn_*.cpp', 'gx_*.cpp', 'snd_*.cpp', 'ui_*.cpp',
    'menu_*.cpp', 'root_*.cpp', 'career_*.cpp', 'paint_*.cpp', 'w32_*.cpp',
    'vp_os.cpp', 'platform.cpp', 'gl_table.cpp', 'gl_core.cpp', 'gl_dxgi.cpp',
    'ddraw_gl.cpp', 'dsound_sdl.cpp'
)

$vpSources = [System.Collections.Generic.List[string]]::new()
foreach ($vpPattern in $vpPatterns) {
    $vpFiles = Get-ChildItem -LiteralPath $vpHook -Filter $vpPattern -File | Sort-Object Name
    foreach ($vpFile in $vpFiles) {
        $vpSources.Add($vpFile.FullName)
    }
}

$vpCommands = @()
foreach ($vpSource in $vpSources) {
    $vpObject = Join-Path $vpObjectDir (([IO.Path]::GetFileNameWithoutExtension($vpSource)) + '.obj')
    $vpArguments = @(
        'cl.exe', '/nologo', '/O2', '/arch:IA32', '/fp:precise', '/MT', '/LD', '/W3',
        '/EHsc', '/std:c++17', ("/I{0}" -f (Join-Path $vpSdl 'include')),
        '/c', $vpSource, ("/Fo{0}" -f $vpObject)
    )
    $vpCommands += [ordered]@{
        directory = $vpHook
        file = $vpSource
        arguments = $vpArguments
    }
}

$vpDatabase = Join-Path $vpOut 'compile_commands.json'
$vpTemporary = "$vpDatabase.tmp"
$vpJson = ConvertTo-Json -InputObject @($vpCommands) -Depth 8
[IO.File]::WriteAllText($vpTemporary, $vpJson + [Environment]::NewLine, [Text.UTF8Encoding]::new($false))
Move-Item -LiteralPath $vpTemporary -Destination $vpDatabase -Force
Write-Host ("Wrote {0} MSVC x86 compile commands to {1}" -f $vpCommands.Count, $vpDatabase)
