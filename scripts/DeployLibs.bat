@echo OFF
:: Usage: PublishLibs.bat [path list file] [files to copy...]

SET pathsList=%~1

IF NOT EXIST "%pathsList%" (
	ECHO 	No deployment path specified. Create %pathsList% with paths on separate lines for auto deployment.
	exit /B 0
)

:TOP
:: Iterate over arguments passed to the script
SHIFT
IF (%1) == () GOTO END

SET file=%1
:: Replace forward slashes with back slashes (because Windows)
SET file=%file:/=\%

:: Iterate over lines in %pathsList% and copy the file from argument
FOR /f "tokens=* delims= usebackq" %%a IN ("%pathsList%") DO (
	IF NOT "%%a" == "" (
		copy /Y "%file%" "%%a"
	)
)

IF "%%a" == "" (
	ECHO 	No deployment path specified.
	exit /B 0
)

GOTO TOP

:END

exit /B 0
