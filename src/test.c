/*
 * Name:        test.c
 * Description: SV Regular Expression launcher.
 * Author:      cosh.cage#hotmail.com
 * File ID:     1024231324C1203231047L00044
 * License:     GPLv2.
 */
#define _CRT_SECURE_NO_WARNINGS 1
#include <stdio.h>
#include <wchar.h>
#include "svregex.h"


int main(int argc, char ** argv)
{
	size_t i, j, k, l;
	wchar_t wcs[BUFSIZ] = { 0 };
	wchar_t pattern[BUFSIZ] = L"(a|b)*abb";

	P_DFA dfa = CompileRegex2DFA(pattern), dfa2;

	dfa2 = MinimizeDFA(dfa);

	PrintDFA(dfa);

	(void)wscanf(L"%ls", wcs);
	wprintf(L"%ls\n", wcs);

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

