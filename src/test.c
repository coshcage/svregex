#define _CRT_SECURE_NO_WARNINGS 1
#include <stdio.h>
#include <wchar.h>
#include "svregex.h"


int main(int argc, char ** argv)
{
	size_t i, j, k, l;
	wchar_t wcs[BUFSIZ] = { 0 };
	wchar_t pattern[BUFSIZ] = L"(a|b)*abb";
	wchar_t * p = pattern;

	P_DFA dfa = CompileRegex2DFA(&p);

	(void)wscanf(L"%ls", wcs);
	wprintf(L"%ls\n", wcs);
	j = *(size_t *)strGetValueMatrix(NULL, dfa, 1, 0, sizeof(size_t));
	k = wcslen(wcs);
	for (i = 0; i < k; ++i)
	{
		j = NextState(dfa, j, wcs[i]);
		strGetValueMatrix(&l, dfa, j, 0, sizeof(size_t));
		if (l & SIGN)
		{
			printf("Match!");
			break;
		}
	}

	DestroyDFA(dfa);
}
