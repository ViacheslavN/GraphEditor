#!/bin/sh
# Regenerates the parser and the scanner of the row filter expressions (bison 3.6+, flex 2.6+).
cd "$(dirname "$0")" || exit 1
bison -Wall --no-lines -o SQLExprParser.cpp --header=SQLExprParser.hpp SQLExprParser.y || exit 1
flex --noline -o SQLExprLexer.cpp SQLExprLexer.l || exit 1
