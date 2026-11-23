REM Run this batch from the directory containing LS.COM.
ECHO === LS : default columns ===
LS
PAUSE
ECHO === LS -l : detailed listing ===
LS -l
PAUSE
ECHO === LS -w : wide listing ===
LS -w
PAUSE
ECHO === LS -w 40 : explicit width ===
LS -w 40
PAUSE
ECHO === LS -1 : one entry per line ===
LS -1
PAUSE
ECHO === LS -l *.TXT : wildcard ===
LS -l *.TXT
PAUSE
ECHO === LS -lh : human-readable sizes ===
LS -lh
PAUSE
ECHO === Finished. You can now try LS commands manually. ===
