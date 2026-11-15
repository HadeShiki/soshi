@echo off
setlocal
cd /d "%~dp0.."

set PY=py -3.12

echo [1/4] check native libraries
%PY% scripts\check_release.py --sources
if errorlevel 1 exit /b 1

echo [2/4] build wheel and sdist
if exist dist rmdir /s /q dist
%PY% -m pip wheel . -w dist --no-deps
if errorlevel 1 exit /b 1
%PY% setup.py --quiet sdist --dist-dir dist
if errorlevel 1 exit /b 1

echo [3/4] check the artifacts carry the native libraries
for %%a in (dist\*.whl dist\*.tar.gz) do (
    %PY% scripts\check_release.py "%%a"
    if errorlevel 1 exit /b 1
)

echo [4/4] check metadata
%PY% -c "import importlib.util, sys; sys.exit(0 if importlib.util.find_spec('twine') else 1)"
if errorlevel 1 (
    echo   twine is not installed, so metadata was not checked:
    echo       %PY% -m pip install twine
    exit /b 1
)
%PY% -m twine check dist/*
if errorlevel 1 exit /b 1

echo.
echo Built:
dir /b dist
echo.
echo Test on TestPyPI first:
echo     %PY% -m twine upload --repository testpypi dist/*
echo Then publish for real:
echo     %PY% -m twine upload dist/*
