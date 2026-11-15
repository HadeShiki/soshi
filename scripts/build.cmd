@echo off
setlocal

if defined VCVARS goto :found

set "VCVARS="
for /d %%v in ("%ProgramFiles%\Microsoft Visual Studio\*") do (
  for /d %%e in ("%%~v\*") do (
    if exist "%%~e\VC\Auxiliary\Build\vcvars64.bat" set "VCVARS=%%~e\VC\Auxiliary\Build\vcvars64.bat"
  )
)

if not defined VCVARS (
  echo Visual Studio with the C++ toolset was not found.
  echo Install the Desktop C++ workload, or set VCVARS to your own vcvars64.bat.
  exit /b 1
)

:found
echo Using %VCVARS%
call "%VCVARS%"

cd /d "%~dp0\.."
cmake -G Ninja -B build -DCMAKE_BUILD_TYPE=Release
if errorlevel 1 exit /b 1

cmake --build build
