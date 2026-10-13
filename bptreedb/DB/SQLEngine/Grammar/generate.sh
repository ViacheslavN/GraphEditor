#!/bin/sh
# Regenerates the SQL parser and scanner from SQLParser.y / SQLLexer.l (bison 3.2+, flex 2.6+).
# The generated files are kept in the repository: the build doesn't need bison / flex
# (or configure with -DBPTREEDB_GENERATE_SQL_GRAMMAR=ON to regenerate them on a change).
set -e
cd "$(dirname "$0")"
bison -Wall --no-lines -o SQLParser.cpp --header=SQLParser.hpp SQLParser.y
flex --noline -o SQLLexer.cpp SQLLexer.l
