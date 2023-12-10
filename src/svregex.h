/*
 * Name:        svregex.h
 * Description: SV Regular Expression Interface.
 * Author:      cosh.cage#hotmail.com
 * File ID:     1024231324B1210231300L00060
 * License:     GPLv2.
 */
#ifndef _SVREGEX_H_
#define _SVREGEX_H_

#include <wchar.h>
#include <limits.h>
#include "svstring.h"

#define SIGN (1ULL << (sizeof(size_t) * CHAR_BIT - 1))

 /* This following macro is to adjust syntax tree's style. */
#define TREE_NODE_SPACE_COUNT 10

typedef P_MATRIX P_DFA; /* Redefine P_MATRIX. */

void   PrintDFA         (P_DFA      pmtx);
P_DFA  CompileRegex2DFA (wchar_t * pwc);
void   DestroyDFA       (P_DFA      dfa);
size_t NextState        (P_DFA      dfa, size_t s, wchar_t a);
size_t NextStateM       (P_DFA      dfa, size_t s, wchar_t a);
P_DFA  MinimizeDFA      (P_DFA      dfa);

/* If a DFA was generated by function CompileRegex2DFA, users should use NextState,
 * otherwise, users should use NextStateM.
 * NextState runs a little bit faster than NextStateM.
 */

 /* Escape sequence that this library accepted.
  * \e = Epsilon.
  * \n
  * \t
  * \v
  * \r
  * \v
  * \f
  * \b
  * \(
  * \)
  * \\
  * \.
  * Operators:
  * |
  * .
  * *
  * (
  * )
  * Note that:
  * a+ == aa*
  * a? == a|\e
  * (a+)? == (a?)+ == a*
  */

#endif