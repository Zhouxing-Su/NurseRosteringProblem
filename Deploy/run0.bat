set name=TSR_ACBR_RBNO_0

echo a >timeout.txt
.\GenerateTimeout.exe
move timeout.txt %name%

cd %name%
.\%name%.bat