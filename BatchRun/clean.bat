del Validator-results.txt
del log.csv
del err.csv
del checkResult.csv
del analysisResult.csv
del statistic.csv
for /d %%a in (output*) do rd /s /q %%a