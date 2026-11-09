@echo off
setlocal
set "VP_REPO=%~dp0.."
set "VP_SDL=%~dp0..\..\sdl2\SDL2-2.32.10"
set "VP_VCVARS=C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvarsall.bat"
set "VP_OUT=%VP_REPO%\out\native-x64"
if not exist "%VP_VCVARS%" goto :missing_vcvars
if not exist "%VP_SDL%\lib\x64\SDL2.lib" goto :missing_sdl
call "%VP_VCVARS%" amd64 >nul
if errorlevel 1 exit /b %errorlevel%
if not exist "%VP_OUT%" mkdir "%VP_OUT%"
pushd "%VP_OUT%"
cl /nologo /O2 /W3 /EHsc /std:c++17 /D_CRT_SECURE_NO_WARNINGS /I"%VP_SDL%\include" ^
  "%VP_REPO%\native\viper_native.cpp" ^
  "%VP_REPO%\native\native_menu_model.cpp" ^
  "%VP_REPO%\native\native_resource_store.cpp" ^
  "%VP_REPO%\native\native_model_info.cpp" ^
  "%VP_REPO%\native\native_track_info.cpp" ^
  "%VP_REPO%\hook\content_registry.cpp" "%VP_REPO%\hook\content_catalog.cpp" ^
  "%VP_REPO%\hook\content_manifest.cpp" "%VP_REPO%\hook\content_sha256.cpp" ^
  "%VP_REPO%\hook\content_preferences.cpp" ^
  /Fe"%VP_OUT%\viper-native.exe" ^
  /link /LIBPATH:"%VP_SDL%\lib\x64" SDL2.lib user32.lib gdi32.lib shell32.lib
if errorlevel 1 (popd & exit /b %errorlevel%)
popd
copy /y "%VP_SDL%\lib\x64\SDL2.dll" "%VP_OUT%\SDL2.dll" >nul
exit /b %errorlevel%

:missing_vcvars
echo Visual Studio x64 build tools not found: %VP_VCVARS%
exit /b 1

:missing_sdl
echo SDL2 x64 library not found: %VP_SDL%
exit /b 1
