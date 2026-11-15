@echo off
setlocal
cd /d "%~dp0.."

if not defined PY set PY=py -3.12

echo [1/5] check build tools
%PY% -c "import importlib.util, sys; missing = [name for name in ('setuptools', 'twine') if importlib.util.find_spec(name) is None]; print('  missing:', ', '.join(missing)) if missing else None; sys.exit(1 if missing else 0)"
if errorlevel 1 (
    echo   install them with:
    echo       %PY% -m pip install setuptools twine
    exit /b 1
)

echo [2/5] check native libraries
%PY% scripts\check_release.py --sources
if errorlevel 1 exit /b 1

echo [3/5] build wheel and sdist
if exist dist rmdir /s /q dist
%PY% -m pip wheel . -w dist --no-deps
if errorlevel 1 exit /b 1
%PY% setup.py --quiet sdist --dist-dir dist
if errorlevel 1 exit /b 1

echo [4/5] check the artifacts carry the native libraries
for %%a in (dist\*.whl dist\*.tar.gz) do (
    %PY% scripts\check_release.py "%%a"
    if errorlevel 1 exit /b 1
)

echo [5/5] check metadata
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
