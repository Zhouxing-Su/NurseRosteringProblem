set name=TSR_ACBR_RBNO_1

echo a >timeout.txt
.\GenerateTimeout.exe
move timeout.txt %name%

cd %name%
.\%name%.bat