 /*
  * Name:        svregex.c
  * Description: SV Regular Expression.
  * Author:      cosh.cage#hotmail.com
  * File ID:     1022231324A1024232001L00933
  * License:     GPLv2.
  */
#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include "svregex.h"
#include "StoneValley/src/svstack.h"
#include "StoneValley/src/svqueue.h"
#include "StoneValley/src/svtree.h"
#include "StoneValley/src/svset.h"

typedef enum en_Terminator
{
	T_LeftBracket = -1,
	T_Jumpover,
	T_Character,
	T_Selection,
	T_Concatenate,
	T_Closure,
	T_RightBracket,
} TERMINATOR;

typedef struct st_Lexicon
{
	TERMINATOR type;
	wchar_t    ch;
	BOOL       nullable;
	P_SET_T    firstpos;
	P_SET_T    lastpos;
} LEXICON, * P_LEXICON;

typedef struct st_DStates
{
	P_SET_T pset;
	size_t  mark;
	size_t  label;
} DSTATES, * P_DSTATES;

typedef struct st_LVFNDTBL
{
	wchar_t ch;
	size_t  i;
} LVFNDTBL, * P_LVFNDTBL;

STACK_L stkOperand;
STACK_L stkOperator;

LEXICON Splitter(wchar_t ** pwc, BOOL * pbt)
{
	wchar_t wc;
	LEXICON lex = { 0 };

	wc = *((*pwc)++);
	if (L'\0' == wc)
		wc = WEOF;

	lex.ch = wc;

	if (L'\\' == lex.ch)
	{
		*pbt = !*pbt;
		if (FALSE == *pbt)
			lex.type = T_Character;
		else
			lex.type = T_Jumpover;
	}
	else
	{
		if (*pbt)
		{
			switch (lex.ch)
			{
			case L'e':
				lex.ch = L'\0';
				lex.type = T_Character;
				*pbt = FALSE;
				break;
			case L'n':
				lex.ch = L'\n';
				lex.type = T_Character;
				*pbt = FALSE;
				break;
			case L't':
				lex.ch = L'\t';
				lex.type = T_Character;
				*pbt = FALSE;
				break;
			case L'a':
				lex.ch = L'\a';
				lex.type = T_Character;
				*pbt = FALSE;
				break;
			case L'r':
				lex.ch = L'\r';
				lex.type = T_Character;
				*pbt = FALSE;
				break;
			case L'v':
				lex.ch = L'\v';
				lex.type = T_Character;
				*pbt = FALSE;
				break;
			case L'f':
				lex.ch = L'\f';
				lex.type = T_Character;
				*pbt = FALSE;
				break;
			case L'b':
				lex.ch = L'\b';
				lex.type = T_Character;
				*pbt = FALSE;
				break;
			case L'.':
			case L'|':
			case L'*':
			case L'(':
			case L')':
				lex.type = T_Character;
				*pbt = FALSE;
				break;
			default:
				lex.type = T_Character;
				*pbt = FALSE;
			}
		}
		else /* if FALSE == bt. */
		{
			switch (lex.ch)
			{
			case L'.':
				lex.type = T_Concatenate;
				break;
			case L'|':
				lex.type = T_Selection;
				break;
			case L'*':
				lex.type = T_Closure;
				break;
			case L'(':
				lex.type = T_LeftBracket;
				break;
			case L')':
				lex.type = T_RightBracket;
				break;
			default:
				lex.type = T_Character;
			}
		}
	}
	return lex;
}

int cbftvsPrintSet(void * pitem, size_t param)
{
	wprintf(L"%zd, ", *(size_t *)(P2P_TNODE_BY(pitem)->pdata));
	return CBF_CONTINUE;
}

void PrintLexicon(LEXICON lex)
{
	P_SET_T p, q;
	p = lex.firstpos;
	q = lex.lastpos;
	switch (lex.type)
	{
	case T_Character:
		if (WEOF == lex.ch)
			wprintf(L"CHAR: \'(#)\'  ");
		else if (L'\0' == lex.ch)
			wprintf(L"CHAR: \'\\e\' ");
		else
			wprintf(L"CHAR: \'%c\' ", lex.ch);
		break;
	case T_Selection:
		wprintf(L"| ");
		break;
	case T_Closure:
		wprintf(L"* ");
		break;
	case T_LeftBracket:
		wprintf(L"( ");
		break;
	case T_RightBracket:
		wprintf(L") ");
		break;
	case T_Concatenate:
		wprintf(L". ");
		break;
	case T_Jumpover:
		wprintf(L"Jump ");
		break;
	}
	wprintf(L"nulabl:%ls {", lex.nullable ? L"TRUE" : L"FALSE");
	setTraverseT(p, cbftvsPrintSet, 0, ETM_INORDER);
	wprintf(L"} {");
	setTraverseT(q, cbftvsPrintSet, 0, ETM_INORDER);
	wprintf(L"}\n");
}

void PrintSyntaxTree(P_TNODE_BY pnode, size_t space)
{
	size_t i;
	if (NULL == pnode)
		return;

	space += TREE_NODE_SPACE_COUNT;

	// Process right child first
	PrintSyntaxTree(pnode->ppnode[RIGHT], space);

	// Print current node after space
	// count
	printf("\n");
	for (i = TREE_NODE_SPACE_COUNT; i < space; ++i)
		printf(" ");

	PrintLexicon(*(P_LEXICON)((P_TNODE_BY)pnode)->pdata);
	PrintSyntaxTree(pnode->ppnode[LEFT], space);
}

extern int _grpCBFCompareInteger(const void * px, const void * py);

int cbftvsComputeNullableAndPos(void * pitem, size_t param)
{
	P_TNODE_BY pnode = P2P_TNODE_BY(pitem);

	if (NULL != pnode->ppnode[LEFT]) /* pnode is not a leaf node. */
	{
		switch (((P_LEXICON)pnode->pdata)->type)
		{
		case T_Selection:
			/* Nullable. */
			((P_LEXICON)pnode->pdata)->nullable =
				((P_LEXICON)pnode->ppnode[LEFT]->pdata)->nullable ||
				((P_LEXICON)pnode->ppnode[RIGHT]->pdata)->nullable;

			/* Firstpos. */
			((P_LEXICON)pnode->pdata)->firstpos =
				setCreateUnionT
				(
					((P_LEXICON)pnode->ppnode[LEFT]->pdata)->firstpos,
					((P_LEXICON)pnode->ppnode[RIGHT]->pdata)->firstpos,
					sizeof(size_t), _grpCBFCompareInteger
				);

			/* Lastpos. */
			((P_LEXICON)pnode->pdata)->lastpos =
				setCreateUnionT
				(
					((P_LEXICON)pnode->ppnode[LEFT]->pdata)->lastpos,
					((P_LEXICON)pnode->ppnode[RIGHT]->pdata)->lastpos,
					sizeof(size_t), _grpCBFCompareInteger
				);
			break;
		case T_Concatenate:
			/* Nullable. */
			((P_LEXICON)pnode->pdata)->nullable =
				((P_LEXICON)pnode->ppnode[LEFT]->pdata)->nullable &&
				((P_LEXICON)pnode->ppnode[RIGHT]->pdata)->nullable;

			/* Firstpos. */
			if (((P_LEXICON)pnode->ppnode[LEFT]->pdata)->nullable)
				((P_LEXICON)pnode->pdata)->firstpos =
				setCreateUnionT
				(
					((P_LEXICON)pnode->ppnode[LEFT]->pdata)->firstpos,
					((P_LEXICON)pnode->ppnode[RIGHT]->pdata)->firstpos,
					sizeof(size_t), _grpCBFCompareInteger
				);
			else
				((P_LEXICON)pnode->pdata)->firstpos =
				NULL != ((P_LEXICON)pnode->ppnode[LEFT]->pdata)->firstpos ?
				setCopyT(((P_LEXICON)pnode->ppnode[LEFT]->pdata)->firstpos, sizeof(size_t)) :
				NULL;

			/* Lastpos. */
			if (((P_LEXICON)pnode->ppnode[RIGHT]->pdata)->nullable)
				((P_LEXICON)pnode->pdata)->lastpos =
				setCreateUnionT
				(
					((P_LEXICON)pnode->ppnode[LEFT]->pdata)->lastpos,
					((P_LEXICON)pnode->ppnode[RIGHT]->pdata)->lastpos,
					sizeof(size_t), _grpCBFCompareInteger
				);
			else
				((P_LEXICON)pnode->pdata)->lastpos =
				NULL != ((P_LEXICON)pnode->ppnode[RIGHT]->pdata)->lastpos ?
				setCopyT(((P_LEXICON)pnode->ppnode[RIGHT]->pdata)->lastpos, sizeof(size_t)) :
				NULL;
			break;
		case T_Closure:
			/* Nullable. */
			((P_LEXICON)pnode->pdata)->nullable = TRUE;

			/* Firstpos. */
			((P_LEXICON)pnode->pdata)->firstpos =
				NULL != ((P_LEXICON)pnode->ppnode[LEFT]->pdata)->firstpos ?
				setCopyT(((P_LEXICON)pnode->ppnode[LEFT]->pdata)->firstpos, sizeof(size_t)) :
				NULL;

			/* Lastpos. */
			((P_LEXICON)pnode->pdata)->lastpos =
				NULL != ((P_LEXICON)pnode->ppnode[LEFT]->pdata)->lastpos ?
				setCopyT(((P_LEXICON)pnode->ppnode[LEFT]->pdata)->lastpos, sizeof(size_t)) :
				NULL;
			break;
		}
	}
	return CBF_CONTINUE;
}

int cbftvsCleanStruct(void * pitem, size_t param)
{
	P_LEXICON plex = (P_LEXICON)(P2P_TNODE_BY(pitem)->pdata);

	if (NULL != plex->firstpos)
		setDeleteT(plex->firstpos);
	if (NULL != plex->lastpos)
		setDeleteT(plex->lastpos);

	return CBF_CONTINUE;
}

void DestroySyntaxTree(P_TNODE_BY pnode)
{
	treTraverseBYPost(pnode, cbftvsCleanStruct, 0);
	treFreeBY(&pnode);
}

P_TNODE_BY Parse(wchar_t ** pwc, size_t * pleaves)
{
	BOOL bt = FALSE;
	size_t posCtr = 1;
	LEXICON lex = { 0 }, tl = { 0 }, ttl = { 0 };
	P_TNODE_BY pnode, pnode1, pnode2, pnode3;
	size_t i = 1, j;

	stkInitL(&stkOperator);
	stkInitL(&stkOperand);

	do
	{
		i = 1;
		tl = lex;
		lex = Splitter(pwc, &bt);

		if (T_Jumpover == lex.type)
			lex = tl;
		else
		{
			/* We need to insert concatenation in the following circumstances:
			 * a & b
			 * a & (
			 * ) & a
			 * ) & (
			 * * & a
			 * * & (
			 */
			switch (tl.type)
			{
			case T_Character:
				if (T_Character == lex.type || T_LeftBracket == lex.type)
					i = 2;
				break;
			case T_RightBracket:
				if (T_Character == lex.type || T_LeftBracket == lex.type)
					i = 2;
				break;
			case T_Closure:
				if (T_Character == lex.type || T_LeftBracket == lex.type)
					i = 2;
				break;
			}

			for (j = 0; j < i; ++j)
			{
				if (2 == i && 0 == j)
				{
					ttl = lex;
					lex.type = T_Concatenate;
				}
				else if (2 == i && 1 == j)
					lex = ttl;
#ifdef DEBUG
				PrintLexicon(lex);
#endif
				switch (lex.type)
				{
				case T_Character:
					if (L'\0' != lex.ch)
					{
						lex.firstpos = setCreateT();
						setInsertT(lex.firstpos, &posCtr, sizeof(size_t), _grpCBFCompareInteger);
						lex.lastpos = setCreateT();
						setInsertT(lex.lastpos, &posCtr, sizeof(size_t), _grpCBFCompareInteger);
						++posCtr;
					}
					else
						lex.firstpos = NULL;

					lex.nullable = L'\0' == lex.ch ? TRUE : FALSE;

					pnode = strCreateNodeD(&lex, sizeof(LEXICON));
					stkPushL(&stkOperand, &pnode, sizeof(P_TNODE_BY));
					break;
				case T_Selection:
				case T_Concatenate:
				case T_Closure:
					if (!stkIsEmptyL(&stkOperator))
					{
						stkPeepL(&pnode1, sizeof(P_TNODE_BY), &stkOperator);
						if (((P_LEXICON)pnode1->pdata)->type >= lex.type)
						{
							stkPopL(&pnode1, sizeof(P_TNODE_BY), &stkOperator);
							switch (((P_LEXICON)pnode1->pdata)->type)
							{
							case T_Selection:
							case T_Concatenate:
								stkPopL(&pnode2, sizeof(P_TNODE_BY), &stkOperand);
								stkPopL(&pnode3, sizeof(P_TNODE_BY), &stkOperand);

								pnode1->ppnode[RIGHT] = pnode2;
								pnode1->ppnode[LEFT] = pnode3;
								break;
							case T_Closure:
								stkPopL(&pnode2, sizeof(P_TNODE_BY), &stkOperand);
								pnode1->ppnode[LEFT] = pnode2;

								break;
							}
							stkPushL(&stkOperand, &pnode1, sizeof(P_TNODE_BY));
						}
					}
					lex.lastpos = lex.firstpos = NULL;
					pnode1 = strCreateNodeD(&lex, sizeof(LEXICON));
					stkPushL(&stkOperator, &pnode1, sizeof(P_TNODE_BY));
					break;
				case T_LeftBracket:
					pnode = strCreateNodeD(&lex, sizeof(LEXICON));
					stkPushL(&stkOperator, &pnode, sizeof(P_TNODE_BY));
					break;
				case T_RightBracket:
					for (;;)
					{
						if (!stkIsEmptyL(&stkOperator))
						{
							stkPeepL(&pnode1, sizeof(P_TNODE_BY), &stkOperator);
							if (T_LeftBracket != ((P_LEXICON)pnode1->pdata)->type)
							{
								stkPopL(&pnode1, sizeof(P_TNODE_BY), &stkOperator);
								switch (((P_LEXICON)pnode1->pdata)->type)
								{
								case T_Selection:
								case T_Concatenate:
									stkPopL(&pnode2, sizeof(P_TNODE_BY), &stkOperand);
									stkPopL(&pnode3, sizeof(P_TNODE_BY), &stkOperand);

									pnode1->ppnode[RIGHT] = pnode2;
									pnode1->ppnode[LEFT] = pnode3;
									break;
								case T_Closure:
									stkPopL(&pnode2, sizeof(P_TNODE_BY), &stkOperand);
									pnode1->ppnode[LEFT] = pnode2;
									break;
								}
								stkPushL(&stkOperand, &pnode1, sizeof(P_TNODE_BY));
							}
							else
								break;
						}
						else
							break;
					}
					stkPopL(&pnode1, sizeof(P_TNODE_BY), &stkOperator);
					strDeleteNodeD(pnode1);
					break;
				}
			}
		}
	} while (WEOF != lex.ch);

	for (;;)
	{
		if (!stkIsEmptyL(&stkOperator))
		{
			stkPopL(&pnode1, sizeof(P_TNODE_BY), &stkOperator);
			switch (((P_LEXICON)pnode1->pdata)->type)
			{
			case T_Selection:
			case T_Concatenate:
				stkPopL(&pnode2, sizeof(P_TNODE_BY), &stkOperand);
				stkPopL(&pnode3, sizeof(P_TNODE_BY), &stkOperand);

				pnode1->ppnode[RIGHT] = pnode2;
				pnode1->ppnode[LEFT] = pnode3;
				break;
			case T_Closure:
				stkPopL(&pnode2, sizeof(P_TNODE_BY), &stkOperand);
				pnode1->ppnode[LEFT] = pnode2;
				break;
			}
			stkPushL(&stkOperand, &pnode1, sizeof(P_TNODE_BY));
		}
		else
			break;
	}

	/* Return a syntax tree. */
	if (1 == strLevelLinkedListSC(stkOperand))
		stkPeepL(&pnode, sizeof(P_TNODE_BY), &stkOperand);
	else
	{
#ifdef DEBUG
		printf("Error! Invalid regular expression.\n");
#endif
		while (!stkIsEmptyL(&stkOperand))
		{
			stkPopL(&pnode, sizeof(P_TNODE_BY), &stkOperand);
			DestroySyntaxTree(pnode);
		}
		*pleaves = posCtr - 1;
		pnode = NULL;
	}

	stkFreeL(&stkOperand);
	stkFreeL(&stkOperator);

	*pleaves = posCtr - 1;
	return pnode;
}

int cbftvsStarTravFirstpos(void * pitem, size_t param)
{
	P_ARRAY_Z parr = (P_ARRAY_Z)0[(size_t *)param];

	P_SET_T * ppset = (P_SET_T *)strLocateItemArrayZ(parr, sizeof(P_SET_T), 1[(size_t *)param] - 1);

	if (NULL == *ppset)
		*ppset = setCreateT();

	setInsertT(*ppset, (size_t *)P2P_TNODE_BY(pitem)->pdata, sizeof(size_t), _grpCBFCompareInteger);

	return CBF_CONTINUE;
}

int cbftvsStarTravLastpos(void * pitem, size_t param)
{
	size_t a[2];

	P_SET_T firstpos = (P_SET_T)1[(size_t *)param];

	a[0] = 0[(size_t *)param];
	a[1] = *(size_t *)P2P_TNODE_BY(pitem)->pdata;

	setTraverseT(firstpos, cbftvsStarTravFirstpos, (size_t)a, ETM_INORDER);
	return CBF_CONTINUE;
}

int cbftvsComputeFollowpos(void * pitem, size_t param)
{
	size_t a[2];
	P_TNODE_BY pnode = P2P_TNODE_BY(pitem);

	a[0] = param;

	if (T_Closure == ((P_LEXICON)pnode->pdata)->type)
	{
		a[1] = (size_t)((P_LEXICON)pnode->pdata)->firstpos;
		setTraverseT(((P_LEXICON)pnode->pdata)->lastpos, cbftvsStarTravLastpos, (size_t)a, ETM_INORDER);
	}
	else if (T_Concatenate == ((P_LEXICON)pnode->pdata)->type)
	{
		a[1] = (size_t)((P_LEXICON)pnode->ppnode[RIGHT]->pdata)->firstpos;
		setTraverseT(((P_LEXICON)pnode->ppnode[LEFT]->pdata)->lastpos, cbftvsStarTravLastpos, (size_t)a, ETM_INORDER);
	}

	return CBF_CONTINUE;
}

P_ARRAY_Z CreateFollowPosArray(P_TNODE_BY pnode, size_t inodes)
{
	P_SET_T nil = NULL;

	P_ARRAY_Z parr = strCreateArrayZ(inodes, sizeof(P_SET_T));

	strSetArrayZ(parr, &nil, sizeof(P_SET_T));

	treTraverseBYPost(pnode, cbftvsComputeFollowpos, (size_t)parr);

	return parr;
}

int cbftvsPrintFollowposArray(void * pitem, size_t param)
{
	P_SET_T pset = *(P_SET_T *)pitem;
	wprintf(L"%zd\t{", ++0[(size_t *)param]);
	setTraverseT(pset, cbftvsPrintSet, 0, ETM_INORDER);
	wprintf(L"}\n");
	return CBF_CONTINUE;
}

int cbftvsClearSetT(void * pitem, size_t param)
{
	P_SET_T pset = *(P_SET_T *)pitem;
	if (NULL != pset)
		setDeleteT(pset);
	return CBF_CONTINUE;
}

void DestroyFollowposArray(P_ARRAY_Z parr)
{
	strTraverseArrayZ(parr, sizeof(P_SET_T), cbftvsClearSetT, 0, FALSE);
	strDeleteArrayZ(parr);
}

int cbftvsConstructLeafNodeTable(void * pitem, size_t param)
{
	P_TNODE_BY pnode = P2P_TNODE_BY(pitem);

	if
		(
			NULL == pnode->ppnode[LEFT] &&
			NULL == pnode->ppnode[RIGHT] &&
			'\0' != ((P_LEXICON)pnode->pdata)->ch
			&& WEOF != ((P_LEXICON)pnode->pdata)->ch
			)
	{
		(*(P_LVFNDTBL *)param)->ch = ((P_LEXICON)pnode->pdata)->ch;
		(*(P_LVFNDTBL *)param)->i = *(size_t *)(*((P_SET_T)((P_LEXICON)pnode->pdata)->firstpos))->knot.pdata;
		++*(P_LVFNDTBL *)param;
	}

	return CBF_CONTINUE;
}

P_ARRAY_Z ConstructLeafNodeTable(P_TNODE_BY pnode, size_t inodes)
{
	P_ARRAY_Z parr = strCreateArrayZ(inodes - 1, sizeof(LVFNDTBL));
	P_LVFNDTBL pl = (P_LVFNDTBL)strLocateItemArrayZ(parr, sizeof(LVFNDTBL), 0);

	treTraverseBYPost(pnode, cbftvsConstructLeafNodeTable, (size_t)&pl);

	return parr;
}

int cbftvsPrintLeafNodeTable(void * pitem, size_t param)
{
	wprintf(L"%c\t%ld\n", ((P_LVFNDTBL)pitem)->ch, ((P_LVFNDTBL)pitem)->i);

	return CBF_CONTINUE;
}

int cbfcmpWChar_t(const void * px, const void * py)
{
	if (*(wchar_t *)px > *(wchar_t *)py) return  1;
	if (*(wchar_t *)px < *(wchar_t *)py) return -1;
	return 0;
}

int cbftvsCompressInputSymbols(void * pitem, size_t param)
{
	setInsertT((P_SET_T)param, &(((P_LVFNDTBL)pitem)->ch), sizeof(wchar_t), cbfcmpWChar_t);
	return CBF_CONTINUE;
}

int cbftvsFillDFAHeader(void * pitem, size_t param)
{
	**(size_t **)param = (size_t) * (wchar_t *)P2P_TNODE_BY(pitem)->pdata;
	++*(size_t **)param;
	return CBF_CONTINUE;
}

int cbftvsFindUnmarked(void * pitem, size_t param)
{
	if (TRUE != ((P_DSTATES)(P2P_TNODE_BY(pitem)->pdata))->mark)
	{
		*(P_DSTATES *)param = (P_DSTATES)(P2P_TNODE_BY(pitem)->pdata);
		return CBF_TERMINATE;
	}
	return CBF_CONTINUE;
}

int cbftvsCmpTwoSets(void * pitem, size_t param)
{
	if (setIsEqualT(((P_DSTATES)P2P_TNODE_BY(pitem)->pdata)->pset, (P_SET_T)0[(size_t *)param], _grpCBFCompareInteger))
	{
		1[(size_t *)param] = ((P_DSTATES)P2P_TNODE_BY(pitem)->pdata)->label;
		2[(size_t *)param] = TRUE;
		return CBF_TERMINATE;
	}
	return CBF_CONTINUE;
}

void PrintDFA(P_DFA pmtx)
{
	size_t i, j, k;
	for (j = 0; j < pmtx->col; ++j)
		printf("%ld\t", *(size_t *)strGetValueMatrix(NULL, pmtx, 0, j, sizeof(size_t)));
	printf("\n");
	for (i = 1; i < pmtx->ln; ++i)
	{
		for (j = 0; j < pmtx->col; ++j)
		{
			strGetValueMatrix(&k, pmtx, i, j, sizeof(size_t));
			if (k & SIGN)
				printf("*%c\t", (char)((k & (~0UL >> 1)) + 'A' - 1));
			else
				printf("%c\t", (char)(k + 'A' - 1));
		}
		printf("\n");
	}
}

int cbftvsDestroyDstates(void * pitem, size_t param)
{
	P_DSTATES pd;
	pd = (P_DSTATES)P2P_TNODE_BY(pitem)->pdata;
	if (pd->pset)
		setDeleteT(pd->pset);
	return CBF_CONTINUE;
}

void DestroyDstates(P_SET_T pds)
{
	setTraverseT(pds, cbftvsDestroyDstates, 0, ETM_POSTORDER);
	setDeleteT(pds);
}

int cbftvsPrintDstates(void * pitem, size_t param)
{
	P_DSTATES pd;
	pd = (P_DSTATES)P2P_TNODE_BY(pitem)->pdata;
	if (pd->pset)
	{
		printf("{ ");
		setTraverseT(pd->pset, cbftvsPrintSet, 0, ETM_INORDER);
		printf(" }, ");
	}
	return CBF_CONTINUE;
}

void PrintDstates(P_SET_T pset)
{
	printf("DSTATES:{ ");
	setTraverseT(pset, cbftvsPrintDstates, 0, ETM_INORDER);
	printf(" }\n");
}

P_MATRIX ConstructDFA(P_ARRAY_Z parflps, P_ARRAY_Z parlvfndtbl, P_TNODE_BY proot, size_t iend)
{
	P_MATRIX dfa = NULL;
	DSTATES d;
	P_DSTATES pd;
	size_t m = 1, n;
	P_SET_T psetDstates;
	/* Construct table header first. */
	{
		/* Compress input symbols. */
		P_SET_T pset;
		size_t * p;
		pset = setCreateT();

		strTraverseArrayZ(parlvfndtbl, sizeof(LVFNDTBL), cbftvsCompressInputSymbols, (size_t)pset, FALSE);

		dfa = strCreateMatrix(2, treArityBY(P2P_TNODE_BY(*pset)) + 1, sizeof(size_t));

		n = 0;
		strSetMatrix(dfa, &n, sizeof(size_t));

		p = (size_t *)strGetValueMatrix(NULL, dfa, 0, 1, sizeof(size_t));

		setTraverseT(pset, cbftvsFillDFAHeader, (size_t)&p, ETM_INORDER);

		setDeleteT(pset);
	}
	psetDstates = setCreateT();

	d.pset = setCopyT(((P_LEXICON)proot->pdata)->firstpos, sizeof(size_t));
	d.mark = FALSE;
	d.label = m;
	++m;

	setInsertT(psetDstates, &d, sizeof(DSTATES), _grpCBFCompareInteger);

	n = 1;
	strSetValueMatrix(dfa, 1, 0, &n, sizeof(size_t));

	pd = NULL;
	setTraverseT(psetDstates, cbftvsFindUnmarked, (size_t)&pd, ETM_LEVELORDER);

	while (pd)
	{
		size_t a[3];
		size_t i, j, k;
		P_SET_T u1 = NULL, u2 = setCreateT();
		pd->mark = TRUE;

		for (i = 1; i < dfa->col; ++i)
		{
			size_t iwc;
			strGetValueMatrix(&iwc, dfa, 0, i, sizeof(size_t));

			for (j = 0; j < strLevelArrayZ(parlvfndtbl); ++j)
			{
				if (((P_LVFNDTBL)strLocateItemArrayZ(parlvfndtbl, sizeof(LVFNDTBL), j))->ch == (wchar_t)iwc)
				{
					k = ((P_LVFNDTBL)strLocateItemArrayZ(parlvfndtbl, sizeof(LVFNDTBL), j))->i;
					if (setIsMemberT(pd->pset, &k, _grpCBFCompareInteger))
					{
						u1 = setCreateUnionT(u2, *(P_SET_T *)strLocateItemArrayZ(parflps, sizeof(P_SET_T), k - 1), sizeof(size_t), _grpCBFCompareInteger);
						setDeleteT(u2);
						u2 = u1;
#ifdef DEBUG
						printf("{ ");
						setTraverseT(u1, cbftvsPrintSet, 0, ETM_INORDER);
						printf(" }\n");
#endif
					}
				}
			}

			/* Whether u1 is not in Dstates. */
			a[0] = (size_t)u1;
			a[1] = pd->label;
			a[2] = FALSE;

#ifdef DEBUG
			PrintDstates(psetDstates);
#endif
			if (NULL != u1)
				setTraverseT(psetDstates, cbftvsCmpTwoSets, (size_t)a, ETM_LEVELORDER);
			else
				a[2] = TRUE;
			if (FALSE == a[2])
			{
				d.pset = setIsEmptyT(u1) ? setCreateT() : setCopyT(u1, sizeof(size_t));
				d.mark = FALSE;
				d.label = m;
				++m;

				setInsertT(psetDstates, &d, sizeof(DSTATES), _grpCBFCompareInteger);
				a[1] = d.label;

				strResizeMatrix(dfa, m, dfa->col, sizeof(size_t));

				n = m - 1;
				if (setIsMemberT(d.pset, &iend, _grpCBFCompareInteger))
					n |= SIGN;
				strSetValueMatrix(dfa, m - 1, 0, &n, sizeof(size_t));
#ifdef DEBUG
				printf("\n");
				PrintDFA(dfa);
#endif
			}

			strSetValueMatrix(dfa, pd->label, i, &a[1], sizeof(size_t));
#ifdef DEBUG
			printf("\n");
			PrintDFA(dfa);
#endif
			if (NULL != u1)
			{
				setDeleteT(u1);
				u1 = NULL;
			}
			u2 = setCreateT();
		}
		setDeleteT(u2);
		pd = NULL;
		setTraverseT(psetDstates, cbftvsFindUnmarked, (size_t)&pd, ETM_LEVELORDER);
	}

	DestroyDstates(psetDstates);
	return dfa;
}

size_t NextState(P_DFA dfa, size_t s, wchar_t a)
{
	if (NULL != dfa && s > 0 && s < dfa->ln)
	{
		size_t i = a;
		size_t * r = (size_t *)svBinarySearch(&i, (size_t *)dfa->arrz.pdata + 1, dfa->col, sizeof(size_t), _grpCBFCompareInteger);
		if (NULL != r)
			return *(size_t *)strGetValueMatrix(NULL, dfa, s, (size_t)(r - (size_t *)dfa->arrz.pdata), sizeof(size_t));
	}
	return 0;
}

P_DFA CompileRegex2DFA(wchar_t ** ppwc)
{
	size_t i, j = 0;
	P_TNODE_BY pnode;
	P_DFA dfa = NULL;

	pnode = Parse(ppwc, &i);;
	if (NULL != pnode)
	{
		P_ARRAY_Z parrfollowpos, parrlvfndtbl;
		treTraverseBYPost(pnode, cbftvsComputeNullableAndPos, 0);
#ifdef DEBUG
		PrintSyntaxTree(pnode, 0);
#endif

		parrfollowpos = CreateFollowPosArray(pnode, i);
#ifdef DEBUG
		strTraverseArrayZ(parrfollowpos, sizeof(P_SET_T), cbftvsPrintFollowposArray, (size_t)&j, FALSE);
#endif

		parrlvfndtbl = ConstructLeafNodeTable(pnode, i);
#ifdef DEBUG
		strTraverseArrayZ(parrlvfndtbl, sizeof(LVFNDTBL), cbftvsPrintLeafNodeTable, 0, FALSE);
#endif
		
		dfa = ConstructDFA(parrfollowpos, parrlvfndtbl, pnode, i);
#ifdef DEBUG
		PrintDFA(dfa);
#endif
		DestroyFollowposArray(parrfollowpos);
		strDeleteArrayZ(parrlvfndtbl);
		DestroySyntaxTree(pnode);
	}
	return dfa;
}

void DestroyDFA(P_DFA dfa)
{
	strDeleteMatrix(dfa);
}
