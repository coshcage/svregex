/*
 * Name:        test.c
 * Description: SV Regular Expression launcher.
 * Author:      cosh.cage#hotmail.com
 * File ID:     1024231324C0806261941L00040
 * License:     GPLv2.
 */
#include <stdio.h>
#include <wchar.h>
#include "svregex.h"

int main()
{
	size_t i, j, k;
	wchar_t wcs[BUFSIZ] = { 0 };
	wchar_t pattern[BUFSIZ] = L"(a|b)*abb";

	P_DFA dfa = CompileRegex2DFA(pattern), dfa2 = MinimizeDFA(dfa);

	PrintDFA(dfa2);
	
	printf("> ");
	wscanf(L"%ls", wcs);

	j = 1;
	k = wcslen(wcs);
	for (i = 0; i < k; ++i)
	{
		j = NextStateM(dfa2, j, wcs[i]);
		if (j & SIGN)
		{
			printf("Match!\n");
			break;
		}
	}

	DestroyDFA(dfa);
	DestroyDFA(dfa2);
}

