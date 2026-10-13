@echo off
rem Regenerates the SQL parser and scanner from SQLParser.y / SQLLexer.l with win_flex_bison
rem (https://github.com/lexxmark/winflexbison, "choco install winflexbison3": win_bison 3.8, win_flex 2.6).
rem The generated files are kept in the repository: the build doesn't need bison / flex
rem (or configure with -DBPTREEDB_GENERATE_SQL_GRAMMAR=ON to regenerate them on a change).
setlocal
cd /d "%~dp0"
win_bison -Wall --no-lines -o SQLParser.cpp --header=SQLParser.hpp SQLParser.y || exit /b 1
win_flex --noline -o SQLLexer.cpp SQLLexer.l || exit /b 1
echo generated: SQLParser.cpp SQLParser.hpp SQLLexer.cpp
