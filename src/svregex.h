#ifndef _SVREGEX_H_
#define _SVREGEX_H_

#include "svstring.h"
typedef P_MATRIX P_DFA;

P_DFA  CompileRegex2DFA (wchar_t ** ppwc);
void   DestroyDFA       (P_DFA      dfa);
size_t NextState        (P_DFA      dfa, size_t s, wchar_t a);

#define SIGN (1ULL << (sizeof(size_t) * CHAR_BIT - 1))
#define TREE_NODE_SPACE_COUNT 10

#endif
