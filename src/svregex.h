/*
 * Name:        svregex.h
 * Description: SV Regular Expression Interface.
 * Author:      cosh.cage#hotmail.com
 * File ID:     1024231324B1024232001L00027
 * License:     GPLv2.
 */
#ifndef _SVREGEX_H_
#define _SVREGEX_H_

#include <wchar.h>
#include <limits.h>
#include "StoneValley/src/svstring.h"

#define SIGN (1ULL << (sizeof(size_t) * CHAR_BIT - 1))

 /* This following macro is to adjust syntax tree's style. */
#define TREE_NODE_SPACE_COUNT 10

typedef P_MATRIX P_DFA; /* Redefine P_MATRIX. */

P_DFA  CompileRegex2DFA (wchar_t ** ppwc);
void   DestroyDFA       (P_DFA      dfa);
size_t NextState        (P_DFA      dfa, size_t s, wchar_t a);

#endif
