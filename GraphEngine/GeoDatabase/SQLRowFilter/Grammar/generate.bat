@echo off
rem Regenerates the parser and the scanner of the row filter expressions: win_bison / win_flex of win_flex_bison
rem (choco install winflexbison3, or vcpkg install winflexbison) on the PATH.
cd /d "%~dp0"
win_bison -Wall --no-lines -o SQLExprParser.cpp --header=SQLExprParser.hpp SQLExprParser.y || exit /b 1
win_flex --noline --wincompat -o SQLExprLexer.cpp SQLExprLexer.l || exit /b 1
