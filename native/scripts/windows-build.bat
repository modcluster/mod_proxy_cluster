REM kudos to Michal Karm Babacek <karm@fedoraproject.org>
REM this script is from (https://github.com/modcluster/ci.modcluster.io)

REM You need to define HTTPD_DEV_HOME environment variable to point to
REM your httpd's folder if you run this scirpt on you own.
REM Also, don't forget to call vcvars64.bat if you run it locally.
REM Otherwise you may not have your tools/environment set up properly.

SetLocal EnableDelayedExpansion

REM Note that some attributes cannot handle backslashes...
SET WORKSPACE_POSSIX=%CD:\=/%

REM CMake workspace
mkdir %CD%\cmakebuild
pushd %CD%\cmakebuild

REM It is not a good idea to try to generate the mod_proxy.lib file, so:
REM dumpbin /exports /nologo /out:!HTTPD_DEV_HOME!\lib\mod_proxy.def.tmp !HTTPD_DEV_HOME!\modules\mod_proxy.so
REM IF NOT %ERRORLEVEL% == 0 ( exit 1 )
REM echo EXPORTS> !HTTPD_DEV_HOME!\lib\mod_proxy.def
REM powershell -Command "(Get-Content !HTTPD_DEV_HOME!\lib\mod_proxy.def.tmp) ^| Foreach-Object {$_ -replace '.*\s(_?ap_proxy.*^|_?proxy_.*)$','$1'} ^| select-string -pattern '^^_?ap_proxy^|^^_?proxy_' ^| Add-    Content !HTTPD_DEV_HOME!\lib\mod_proxy.def"
REM IF NOT %ERRORLEVEL% == 0 ( exit 1 )
REM lib /def:!HTTPD_DEV_HOME!\lib\mod_proxy.def /OUT:!HTTPD_DEV_HOME!\lib\mod_proxy.lib /MACHINE:X64 /NAME:mod_proxy.so
REM IF NOT %ERRORLEVEL% == 0 ( exit 1 )

SET HTTPD_DEV_HOME_POSSIX=%HTTPD_DEV_HOME:\=/%

cmake -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=RELWITHDEBINFO ^
-DCMAKE_C_FLAGS_RELWITHDEBINFO="/DWIN32 /D_WINDOWS /W3 /MD /Zi /O2 /Ob1 /DNDEBUG" ^
-DCMAKE_C_FLAGS="/DWIN32 /D_WINDOWS /W3 /MD /Zi /O2 /Ob1 /DNDEBUG" ^
-DAPR_LIBRARY=%HTTPD_DEV_HOME_POSSIX%/lib/libapr-1.lib ^
-DAPR_INCLUDE_DIR=%HTTPD_DEV_HOME_POSSIX%/include/ ^
-DAPACHE_INCLUDE_DIR=%HTTPD_DEV_HOME_POSSIX%/include/ ^
-DAPRUTIL_LIBRARY=%HTTPD_DEV_HOME_POSSIX%/lib/libaprutil-1.lib ^
-DAPRUTIL_INCLUDE_DIR=%HTTPD_DEV_HOME_POSSIX%/include/ ^
-DAPACHE_LIBRARY=%HTTPD_DEV_HOME_POSSIX%/lib/libhttpd.lib ^
-DPROXY_LIBRARY=%HTTPD_DEV_HOME_POSSIX%/lib/mod_proxy.lib ^
%WORKSPACE_POSSIX%/native/

IF NOT %ERRORLEVEL% == 0 ( exit 1 )
REM Compile
nmake
IF NOT %ERRORLEVEL% == 0 ( exit 1 )

