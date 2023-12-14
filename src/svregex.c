 /*
  * Name:        svregex.c
  * Description: SV Regular Expression.
  * Author:      cosh.cage#hotmail.com
  * File ID:     1022231324A1214230350L01620
  * License:     GPLv2.
  */
#include <stdio.h>
#include <string.h>
#include "svregex.h"
#include "svstack.h"
#include "svqueue.h"
#include "svtree.h"
#include "svset.h"

/* A structure describes lexicon type. */
typedef enum en_Terminator
{
	T_LeftBracket = -1, /* ( */
	T_Jumpover,         /* \ */
	T_Character,        /* n */
	T_Selection,        /* | */
	T_Concatenate,      /* . */
	T_Closure,          /* * */
	T_RightBracket,     /* ) */
} TERMINATOR;

/* A structure describes the lexicon in a syntax tree. */
typedef struct st_Lexicon
{
	TERMINATOR type;
	wchar_t    ch;       /* Character. */
	BOOL       nullable;
	P_SET_T    firstpos;
	P_SET_T    lastpos;
} LEXICON, * P_LEXICON;

/* Dstates set of sets. */
typedef struct st_DStates
{
	P_SET_T pset;
	size_t  mark;
	size_t  label;
} DSTATES, * P_DSTATES;

/* Leaf node table. */
typedef struct st_LVFNDTBL
{
	wchar_t ch;
	size_t  i;
} LVFNDTBL, * P_LVFNDTBL;

/* Group state enumeration. */
typedef enum en_GROUPSTATE
{
	EGS_NORMAL,
	EGS_START,
	EGS_END
} GROUPSTATE;

/* Structure of the element of set PI. */
typedef struct st_STATEGROUP
{
	P_SET_T    pset;
	size_t     rep;
	BOOL       bsplit;
	GROUPSTATE egs;
} STATEGROUP, * P_STATEGROUP;

STACK_L stkOperand;  /* Operand stack. */
STACK_L stkOperator; /* Operator stack. */

/* Attention:     This Is An Internal Function. No Interface for Library Users.
 * Function name: Splitter
 * Description:   Lexical analyzer.
 * Parameters:
 *        pwc Pointer to a wide character string.
 *        pbt Pointer to a boolean variable that is used to store converting state.
 * Return value:  A LEXICON structure.
 */
static LEXICON Splitter(wchar_t ** pwc, BOOL * pbt)
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
			case L'e':	/* Epsilon. */
				lex.ch = L'\0';
				lex.type = T_Character;
				*pbt = FALSE;
				break;
			case L'n':	/* New line. */
				lex.ch = L'\n';
				lex.type = T_Character;
				*pbt = FALSE;
				break;
			case L't':	/* Table. */
				lex.ch = L'\t';
				lex.type = T_Character;
				*pbt = FALSE;
				break;
			case L'a':	/* Alarm. */
				lex.ch = L'\a';
				lex.type = T_Character;
				*pbt = FALSE;
				break;
			case L'r':	/* Return. */
				lex.ch = L'\r';
				lex.type = T_Character;
				*pbt = FALSE;
				break;
			case L'v': /* Vertical table. */
				lex.ch = L'\v';
				lex.type = T_Character;
				*pbt = FALSE;
				break;
			case L'f':	/* Form feed. */
				lex.ch = L'\f';
				lex.type = T_Character;
				*pbt = FALSE;
				break;
			case L'b':	/* Back space. */
				lex.ch = L'\b';
				lex.type = T_Character;
				*pbt = FALSE;
				break;
			case L'.':	/* Operators. */
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

/* Attention:     This Is An Internal Function. No Interface for Library Users.
 * Function name: cbftvsPrintSet
 * Description:   Print size_t element set.
 * Parameters:
 *      pitem Pointer to a each binary tree node in set.
 *      param N/A.
 * Return value:  CBF_CONTINUE only.
 */
static int cbftvsPrintSet(void * pitem, size_t param)
{
	DWC4100(param);
	wprintf(L"%zd, ", *(size_t *)(P2P_TNODE_BY(pitem)->pdata));
	return CBF_CONTINUE;
}

/* Attention:     This Is An Internal Function. No Interface for Library Users.
 * Function name: PrintLexicon
 * Description:   Print LEXICON structure.
 * Parameter:
 *       lex A lexicon structure to be printed.
 * Return value:  N/A.
 */
static void PrintLexicon(LEXICON lex)
{
	P_SET_T p, q;
	p = lex.firstpos;
	q = lex.lastpos;
	switch (lex.type)
	{
	case T_Character:
		if (WEOF == (BOOL)lex.ch)
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

/* Attention:     This Is An Internal Function. No Interface for Library Users.
 * Function name: PrintSyntaxTree
 * Description:   Print the syntax tree of a regular expression.
 * Parameters:
 *      pnode Pointer to a binary tree node.
 *      space Spaces to be inserted.
 * Return value:  N/A.
 */
static void PrintSyntaxTree(P_TNODE_BY pnode, size_t space)
{
	size_t i;
	if (NULL == pnode)
		return;

	space += TREE_NODE_SPACE_COUNT;

	/* Process right child first. */
	PrintSyntaxTree(pnode->ppnode[RIGHT], space);

	/* Print current node after space count. */
	printf("\n");
	for (i = TREE_NODE_SPACE_COUNT; i < space; ++i)
		printf(" ");

	PrintLexicon(*(P_LEXICON)((P_TNODE_BY)pnode)->pdata);
	PrintSyntaxTree(pnode->ppnode[LEFT], space);
}

/* Extern function to be invoked. */
extern int _grpCBFCompareInteger(const void * px, const void * py);

/* Attention:     This Is An Internal Function. No Interface for Library Users.
 * Function name: cbftvsComputeNullableAndPos
 * Description:   Calculate the nullable parameter and POS sets.
 * Parameters:
 *      pitem Pointer to a binary tree node.
 *      param N/A.
 * Return value:  CBF_CONTINUE only.
 */
static int cbftvsComputeNullableAndPos(void * pitem, size_t param)
{
	P_TNODE_BY pnode = P2P_TNODE_BY(pitem);

	DWC4100(param);

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
		case T_LeftBracket:
		case T_Jumpover:
		case T_Character:
		case T_RightBracket:
			break;
		}
	}
	return CBF_CONTINUE;
}

/* Attention:     This Is An Internal Function. No Interface for Library Users.
 * Function name: cbftvsCleanStruct
 * Description:   Clean structures in a syntax tree.
 * Parameters:
 *      pitem Pointer to a binary tree node.
 *      param N/A.
 * Return value:  CBF_CONTINUE only.
 */
static int cbftvsCleanStruct(void * pitem, size_t param)
{
	P_LEXICON plex = (P_LEXICON)(P2P_TNODE_BY(pitem)->pdata);

	DWC4100(param);

	if (NULL != plex->firstpos)
		setDeleteT(plex->firstpos);
	if (NULL != plex->lastpos)
		setDeleteT(plex->lastpos);

	return CBF_CONTINUE;
}

/* Attention:     This Is An Internal Function. No Interface for Library Users.
 * Function name: DestroySyntaxTree
 * Description:   Deallocate a syntax tree.
 * Parameter:
 *     pnode Pointer to the root node of syntax tree.
 * Return value:  N/A.
 */
static void DestroySyntaxTree(P_TNODE_BY pnode)
{
	treTraverseBYPost(pnode, cbftvsCleanStruct, 0);
	treFreeBY(&pnode);
}

/* Attention:     This Is An Internal Function. No Interface for Library Users.
 * Function name: Parse
 * Description:   The parser that is used to convert a regular expression to syntax tree uses Dijkstra's two stack algorithm.
 * Parameters:
 *        pwc Pointer to wide character string.
 *    pleaves Return how many leaves there are in the tree.
 * Return value:  The root node of syntax tree.
 */
static P_TNODE_BY Parse(wchar_t ** pwc, size_t * pleaves)
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
			 * a . b
			 * a . (
			 * ) . a
			 * ) . (
			 * * . a
			 * * . (
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
			case T_Jumpover:
			case T_Selection:
			case T_Concatenate:
			case T_LeftBracket:
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
							case T_Jumpover:
							case T_Character:
							case T_RightBracket:
							case T_LeftBracket:
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
								case T_Jumpover:
								case T_Character:
								case T_RightBracket:
								case T_LeftBracket:
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
				case T_Jumpover:
					break;
				}
			}
		}
	} while (WEOF != (BOOL)lex.ch);

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
			case T_Jumpover:
			case T_Character:
			case T_RightBracket:
			case T_LeftBracket:
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

/* Attention:     This Is An Internal Function. No Interface for Library Users.
 * Function name: cbftvsStarTravFirstpos
 * Description:   Traverse the firstpos set.
 * Parameters:
 *      pitem Pointer to each size_t element in the set.
 *      param Pointer to a size_t[2] array.
 * Return value:  CBF_CONTINUE only.
 */
static int cbftvsStarTravFirstpos(void * pitem, size_t param)
{
	P_ARRAY_Z parr = (P_ARRAY_Z)0[(size_t *)param];

	P_SET_T * ppset = (P_SET_T *)strLocateItemArrayZ(parr, sizeof(P_SET_T), 1[(size_t *)param] - 1);

	if (NULL == *ppset)
		*ppset = setCreateT();

	setInsertT(*ppset, (size_t *)P2P_TNODE_BY(pitem)->pdata, sizeof(size_t), _grpCBFCompareInteger);

	return CBF_CONTINUE;
}

/* Attention:     This Is An Internal Function. No Interface for Library Users.
 * Function name: cbftvsStarTravLastpos
 * Description:   Traverse the lastpos set.
 * Parameters:
 *      pitem Pointer to each size_t element in the set.
 *      param Pointer to a size_t[2] array.
 * Return value:  CBF_CONTINUE only.
 */
static int cbftvsStarTravLastpos(void * pitem, size_t param)
{
	size_t a[2];

	P_SET_T firstpos = (P_SET_T)1[(size_t *)param];

	a[0] = 0[(size_t *)param];
	a[1] = *(size_t *)P2P_TNODE_BY(pitem)->pdata;

	setTraverseT(firstpos, cbftvsStarTravFirstpos, (size_t)a, ETM_INORDER);
	return CBF_CONTINUE;
}

/* Attention:     This Is An Internal Function. No Interface for Library Users.
 * Function name: cbftvsComputeFollowpos
 * Description:   Calculate the followpos set.
 * Parameters:
 *      pitem Pointer to each size_t element in the set.
 *      param Pointer to a size_t[2] array.
 * Return value:  CBF_CONTINUE only.
 */
static int cbftvsComputeFollowpos(void * pitem, size_t param)
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

/* Attention:     This Is An Internal Function. No Interface for Library Users.
 * Function name: CreateFollowPosArray
 * Description:   Create followpos array.
 * Parameters:
 *      pnode Pointer to the root node of a syntax tree.
 *     inodes Sign how many nodes there are in the array.
 * Return value:  New allocated array.
 */
static P_ARRAY_Z CreateFollowPosArray(P_TNODE_BY pnode, size_t inodes)
{
	P_SET_T nil = NULL;

	P_ARRAY_Z parr = strCreateArrayZ(inodes, sizeof(P_SET_T));

	strSetArrayZ(parr, &nil, sizeof(P_SET_T));

	treTraverseBYPost(pnode, cbftvsComputeFollowpos, (size_t)parr);

	return parr;
}

/* Attention:     This Is An Internal Function. No Interface for Library Users.
 * Function name: cbftvsPrintFollowposArray
 * Description:   Print followpos array.
 * Parameters:
 *      pitem Pointer to each element of the array.
 *      param size_t variable to print numbers.
 * Return value:  CBF_CONTINUE only.
 */
static int cbftvsPrintFollowposArray(void * pitem, size_t param)
{
	P_SET_T pset = *(P_SET_T *)pitem;
	wprintf(L"%zd\t{", ++0[(size_t *)param]);
	setTraverseT(pset, cbftvsPrintSet, 0, ETM_INORDER);
	wprintf(L"}\n");
	return CBF_CONTINUE;
}

/* Attention:     This Is An Internal Function. No Interface for Library Users.
 * Function name: cbftvsPrintFollowposArray
 * Description:   Print followpos array.
 * Parameters:
 *      pitem Pointer to each element of the array.
 *      param N/A.
 * Return value:  CBF_CONTINUE only.
 */
static int cbftvsClearSetT(void * pitem, size_t param)
{
	DWC4100(param);
	P_SET_T pset = *(P_SET_T *)pitem;
	if (NULL != pset)
		setDeleteT(pset);
	return CBF_CONTINUE;
}

/* Attention:     This Is An Internal Function. No Interface for Library Users.
 * Function name: DestroyFollowposArray
 * Description:   Deallocate followpos array.
 * Parameter:
 *     pitem Pointer to followpos array.
 * Return value:  N/A.
 */
static void DestroyFollowposArray(P_ARRAY_Z parr)
{
	strTraverseArrayZ(parr, sizeof(P_SET_T), cbftvsClearSetT, 0, FALSE);
	strDeleteArrayZ(parr);
}

/* Attention:     This Is An Internal Function. No Interface for Library Users.
 * Function name: cbftvsConstructLeafNodeTable
 * Description:   Build the leaf node table.
 * Parameters:
 *      pitem Pointer to each element of binary tree.
 *      param Pointer to an element of leaf node table.
 * Return value:  CBF_CONTINUE only.
 */
static int cbftvsConstructLeafNodeTable(void * pitem, size_t param)
{
	P_TNODE_BY pnode = P2P_TNODE_BY(pitem);

	if
		(
			NULL == pnode->ppnode[LEFT] &&
			NULL == pnode->ppnode[RIGHT] &&
			'\0' != ((P_LEXICON)pnode->pdata)->ch
			&& WEOF != (BOOL)((P_LEXICON)pnode->pdata)->ch
			)
	{
		(*(P_LVFNDTBL *)param)->ch = ((P_LEXICON)pnode->pdata)->ch;
		(*(P_LVFNDTBL *)param)->i = *(size_t *)(*((P_SET_T)((P_LEXICON)pnode->pdata)->firstpos))->knot.pdata;
		++*(P_LVFNDTBL *)param;
	}

	return CBF_CONTINUE;
}

/* Attention:     This Is An Internal Function. No Interface for Library Users.
 * Function name: cbftvsConstructLeafNodeTable
 * Description:   Build the leaf node table.
 * Parameters:
 *      pitem Pointer to each element of binary tree.
 *     inodes Node nambers in the table.
 * Return value:  New built leaf node table.
 */
static P_ARRAY_Z ConstructLeafNodeTable(P_TNODE_BY pnode, size_t inodes)
{
	P_ARRAY_Z parr = strCreateArrayZ(inodes - 1, sizeof(LVFNDTBL));
	P_LVFNDTBL pl = (P_LVFNDTBL)strLocateItemArrayZ(parr, sizeof(LVFNDTBL), 0);

	treTraverseBYPost(pnode, cbftvsConstructLeafNodeTable, (size_t)&pl);

	return parr;
}

/* Attention:     This Is An Internal Function. No Interface for Library Users.
 * Function name: cbftvsPrintLeafNodeTable
 * Description:   Print the leaf node table.
 * Parameters:
 *      pitem Pointer to each element of leaf node table.
 *      param N/A.
 * Return value:  CBF_CONTINUE only.
 */
static int cbftvsPrintLeafNodeTable(void * pitem, size_t param)
{
	DWC4100(param);

	wprintf(L"%c\t%ld\n", ((P_LVFNDTBL)pitem)->ch, ((P_LVFNDTBL)pitem)->i);

	return CBF_CONTINUE;
}

/* Attention:     This Is An Internal Function. No Interface for Library Users.
 * Function name: cbfcmpWChar_t
 * Description:   Compare wchar_ts.
 * Parameters:
 *         px Pointer to a wchar_t value.
 *         py Pointer to another wchar_t value.
 * Return value:  1, -1, 0 respectively.
 */
static int cbfcmpWChar_t(const void * px, const void * py)
{
	if (*(wchar_t *)px > *(wchar_t *)py) return  1;
	if (*(wchar_t *)px < *(wchar_t *)py) return -1;
	return 0;
}

/* Attention:     This Is An Internal Function. No Interface for Library Users.
 * Function name: cbftvsCompressInputSymbols
 * Description:   Use a set to absorb input symbols.
 * Parameters:
 *      pitem Pointer to each element of leaf node table.
 *      param Pointer to a set.
 * Return value:  CBF_CONTINUE only.
 */
static int cbftvsCompressInputSymbols(void * pitem, size_t param)
{
	setInsertT((P_SET_T)param, &(((P_LVFNDTBL)pitem)->ch), sizeof(wchar_t), cbfcmpWChar_t);
	return CBF_CONTINUE;
}

/* Attention:     This Is An Internal Function. No Interface for Library Users.
 * Function name: cbftvsFillDFAHeader
 * Description:   Fill DFA header by input symbols.
 * Parameters:
 *      pitem Pointer to each node of a set.
 *      param Pointer to a pointer of a size_t.
 * Return value:  CBF_CONTINUE only.
 */
static int cbftvsFillDFAHeader(void * pitem, size_t param)
{
	**(size_t **)param = (size_t) * (wchar_t *)P2P_TNODE_BY(pitem)->pdata;
	++*(size_t **)param;
	return CBF_CONTINUE;
}

/* Attention:     This Is An Internal Function. No Interface for Library Users.
 * Function name: cbftvsFindUnmarked
 * Description:   Find unmarked set in set Dstates.
 * Parameters:
 *      pitem Pointer to each node of set Dstates.
 *      param Pointer to DSTATE structure.
 * Return value:  CBF_CONTINUE and CBF_TERMINATE respectively.
 */
static int cbftvsFindUnmarked(void * pitem, size_t param)
{
	if (TRUE != ((P_DSTATES)(P2P_TNODE_BY(pitem)->pdata))->mark)
	{
		*(P_DSTATES *)param = (P_DSTATES)(P2P_TNODE_BY(pitem)->pdata);
		return CBF_TERMINATE;
	}
	return CBF_CONTINUE;
}

/* Attention:     This Is An Internal Function. No Interface for Library Users.
 * Function name: cbftvsCmpTwoSets
 * Description:   Compare two size_t element sets.
 * Parameters:
 *      pitem Pointer to each node of a set.
 *      param Pointer to a size_t[3] array.
 * Return value:  CBF_CONTINUE and CBF_TERMINATE respectively.
 */
static int cbftvsCmpTwoSets(void * pitem, size_t param)
{
	if (setIsEqualT(((P_DSTATES)P2P_TNODE_BY(pitem)->pdata)->pset, (P_SET_T)0[(size_t *)param], _grpCBFCompareInteger))
	{
		1[(size_t *)param] = ((P_DSTATES)P2P_TNODE_BY(pitem)->pdata)->label;
		2[(size_t *)param] = TRUE;
		return CBF_TERMINATE;
	}
	return CBF_CONTINUE;
}

/* Function name: PrintDFA
 * Description:   Print DFA to the console.
 * Parameters:
 *       pmtx Pointer to a DFA.
 * Return value:  N/A.
 */
void PrintDFA(P_DFA pmtx)
{
	size_t i, j, k;
	printf("0\t");
	for (j = 1; j < pmtx->col; ++j)
		wprintf(L"%\'%c\'\t", (wchar_t)*(size_t *)strGetValueMatrix(NULL, pmtx, 0, j, sizeof(size_t)));
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

/* Attention:     This Is An Internal Function. No Interface for Library Users.
 * Function name: cbftvsDestroyDstates
 * Description:   Deallocate set Dstates.
 * Parameters:
 *      pitem Pointer to each node of set PI.
 *      param N/A.
 * Return value:  CBF_CONTINUE only.
 */
static int cbftvsDestroyDstates(void * pitem, size_t param)
{
	P_DSTATES pd;
	DWC4100(param);
	pd = (P_DSTATES)P2P_TNODE_BY(pitem)->pdata;
	if (pd->pset)
		setDeleteT(pd->pset);
	return CBF_CONTINUE;
}

/* Attention:     This Is An Internal Function. No Interface for Library Users.
 * Function name: cbftvsDestroyDstates
 * Description:   Deallocate set PI.
 * Parameters:
 *        pds Pointer to each node of set Dstates.
 * Return value:  N/A.
 */
static void DestroyDstates(P_SET_T pds)
{
	setTraverseT(pds, cbftvsDestroyDstates, 0, ETM_POSTORDER);
	setDeleteT(pds);
}

/* Attention:     This Is An Internal Function. No Interface for Library Users.
 * Function name: cbftvsPrintDstates
 * Description:   Print set Dstates.
 * Parameters:
 *      pitem Pointer to each node of set Dstates.
 *      param N/A.
 * Return value:  CBF_CONTINUE only.
 */
int cbftvsPrintDstates(void * pitem, size_t param)
{
	P_DSTATES pd;
	DWC4100(param);
	pd = (P_DSTATES)P2P_TNODE_BY(pitem)->pdata;
	if (pd->pset)
	{
		printf("{ ");
		setTraverseT(pd->pset, cbftvsPrintSet, 0, ETM_INORDER);
		printf(" }, ");
	}
	return CBF_CONTINUE;
}

/* Attention:     This Is An Internal Function. No Interface for Library Users.
 * Function name: PrintDstates
 * Description:   Print set Dstates.
 * Parameters:
 *       pset Pointer to set Dstates.
 * Return value:  N/A.
 */
static void PrintDstates(P_SET_T pset)
{
	printf("DSTATES:{ ");
	setTraverseT(pset, cbftvsPrintDstates, 0, ETM_INORDER);
	printf(" }\n");
}

/* Attention:     This Is An Internal Function. No Interface for Library Users.
 * Function name: ConstructDFA
 * Description:   Construct a DFA.
 * Parameters:
 *    parflps Pointer to followpos array.
 *parlvfndtbl Pointer to leaf node table.
 *      proot Pointer to the root of syntax tree.
 *       iend End state.
 * Return value:  Pointer to a new DFA.
 */
static P_MATRIX ConstructDFA(P_ARRAY_Z parflps, P_ARRAY_Z parlvfndtbl, P_TNODE_BY proot, size_t iend)
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

/* Attention:     This Is An Internal Function. No Interface for Library Users.
 * Function name: _cbfcmpSize_t
 * Description:   Compare two size_ts with their sign. 
 * Parameters:
 *         px Pointer to a size_t.
 *         py Pointer to another size_t.
 * Return value:  1, -1 and 0 respectively.
 */
static int _cbfcmpSize_t(const void * px, const void * py)
{
	size_t x, y;
	x = *(size_t *)px;
	y = *(size_t *)py;
	x &= (~SIGN);
	y &= (~SIGN);
	return _grpCBFCompareInteger(&x, &y);
}

/* Function name: NextState
 * Description:   Get next state for a DFA.
 * Parameters:
 *        dfa Pointer to a dfa.
 *          s Current state.
 *          a Input symbol.
 * Return value:  The next state.
 */
size_t NextState(P_DFA dfa, size_t s, wchar_t a)
{
	if (NULL != dfa && s > 0 && s < dfa->ln)
	{
		size_t i = a;
		size_t * r = (size_t *)svBinarySearch(&i, (size_t *)dfa->arrz.pdata, dfa->col, sizeof(size_t), _grpCBFCompareInteger);
		if (NULL != r)
			return *(size_t *)strGetValueMatrix(NULL, dfa, s, (size_t)(r - (size_t *)dfa->arrz.pdata), sizeof(size_t));
	}
	return 0;
}

/* Function name: NextStateM
 * Description:   Get next state for a minimized DFA.
 * Parameters:
 *        dfa Pointer to a dfa.
 *          s Current state.
 *          a Input symbol.
 * Return value:  The next state.
 */
size_t NextStateM(P_DFA dfa, size_t s, wchar_t a)
{
	if (NULL != dfa && s > 0 && s < dfa->ln)
	{
		size_t i = a;
		size_t * r = (size_t *)svBinarySearch(&i, (size_t *)dfa->arrz.pdata, dfa->col, sizeof(size_t), _grpCBFCompareInteger);
		if (NULL != r)
		{
			size_t k = (size_t)(r - (size_t *)dfa->arrz.pdata), * l;
			l = svBinarySearch(&k, dfa->arrz.pdata, dfa->ln, sizeof(size_t) * dfa->col, _cbfcmpSize_t);
			if (NULL != l)
				return *(size_t *)strGetValueMatrix(NULL, dfa, s, *l, sizeof(size_t));
		}
	}
	return 0;
}

/* Function name: CompileRegex2DFA
 * Description:   Compile a regular expression to a DFA.
 * Parameter:
 *       pwc Pointer to a regular expression.
 * Return value:  A new allocated DFA.
 */
P_DFA CompileRegex2DFA(wchar_t * pwc)
{
	size_t i, j = 0;
	P_TNODE_BY pnode;
	P_DFA dfa = NULL;
	wchar_t ** ppwc = &pwc;

	pnode = Parse(ppwc, &i);
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

/* Attention:     This Is An Internal Function. No Interface for Library Users.
 * Function name: cbftvsPickRep
 * Description:   Pick a representation.
 * Parameters:
 *      pitem Pointer to each node of a set.
 *      param Pointer to a size_t.
 * Return value:  CBF_CONTINUE and CBF_TERMINATE respectively.
 */
static int cbftvsPickRep(void * pitem, size_t param)
{
	P_TNODE_BY pnode = P2P_TNODE_BY(pitem);
	if (NULL == pnode->ppnode[LEFT] && NULL == pnode->ppnode[RIGHT])
	{
		*(size_t *)param = *(size_t *)pnode->pdata;
		return CBF_TERMINATE;
	}
	return CBF_CONTINUE;
}

/* Attention:     This Is An Internal Function. No Interface for Library Users.
 * Function name: cbftvsDestroyPsetPI
 * Description:   Deallocate set PI.
 * Parameters:
 *      pitem Pointer to each node of a set.
 *      param N/A.
 * Return value:  CBF_CONTINUE only.
 */
static int cbftvsDestroyPsetPI(void * pitem, size_t param)
{
	DWC4100(param);
	setDeleteT(((P_STATEGROUP)P2P_TNODE_BY(pitem)->pdata)->pset);
	return CBF_CONTINUE;
}

/* Attention:     This Is An Internal Function. No Interface for Library Users.
 * Function name: DestroyPsetPI
 * Description:   Deallocate set PI.
 * Parameter:
 *      pset Pointer to set PI.
 * Return value:  N/A.
 */
static void DestroyPsetPI(P_SET_T pset)
{
	setTraverseT(pset, cbftvsDestroyPsetPI, 0, ETM_POSTORDER);
	setDeleteT(pset);
	pset = NULL;
}

/* Attention:     This Is An Internal Function. No Interface for Library Users.
 * Function name: cbftvsSplitSetGroup
 * Description:   Split set group.
 * Parameters:
 *      pitem Pointer to each node of a set.
 *      param Pointer to a size_t[4] array.
 * Return value:  CBF_CONTINUE only.
 */
static int cbftvsSplitSetGroup(void * pitem, size_t param)
{
	size_t k = *(size_t *)P2P_TNODE_BY(pitem)->pdata;
	P_SET_T psetPI = (P_SET_T)0[(size_t *)param];
	P_DFA dfa = (P_DFA)1[(size_t *)param];
	P_STATEGROUP psg = (P_STATEGROUP)2[(size_t *)param];
	P_SET_T psetEND = (P_SET_T)3[(size_t *)param];

	size_t i, j, l;
	BOOL bfound = FALSE;

	for (i = 1; i < dfa->col; ++i)
	{
		strGetValueMatrix(&j, dfa, k, i, sizeof(size_t));

#ifdef DEBUG
		printf("{ ");
		setTraverseT(psg->pset, cbftvsPrintSet, 0, ETM_INORDER);
		printf(" }\n");
#endif

		if (!setIsMemberT(psg->pset, &j, _grpCBFCompareInteger))
		{	/* Split. */
			if (1 != k)
			{
				STATEGROUP sg;
				sg.pset = setCreateT();
				sg.bsplit = FALSE;
				sg.rep = k;

				/* Alter psg->egs. */
				if (setIsMemberT(psetEND, &k, _grpCBFCompareInteger))
					sg.egs = EGS_END;
				else
					sg.egs = EGS_NORMAL;

				setRemoveT(psg->pset, &k, sizeof(size_t), _grpCBFCompareInteger);

#ifdef DEBUG
				printf("R:{ ");
				setTraverseT(psg->pset, cbftvsPrintSet, 0, ETM_INORDER);
				printf(" }\n");
#endif

				setInsertT(sg.pset, &k, sizeof(size_t), _grpCBFCompareInteger);

				setInsertT(psetPI, &sg, sizeof(STATEGROUP), _grpCBFCompareInteger);

				/* Alter psg->rep. */
				if (psg->rep == k)
				{
					setTraverseT(psg->pset, cbftvsPickRep, (size_t)&l, ETM_INORDER);
					psg->rep = l;
				}

				bfound = TRUE;
				break;
			}
		}
	}
	if (treArityBY(P2P_TNODE_BY(*psg->pset)) > 1)
		if (FALSE == bfound)
			psg->bsplit = FALSE;

	return CBF_CONTINUE;
}

/* Attention:     This Is An Internal Function. No Interface for Library Users.
 * Function name: cbftvsIsSplittable
 * Description:   Judge whether a set is able to split.
 * Parameters:
 *      pitem Pointer to each node of a set.
 *      param Pointer to a pointer of STATEGROUP.
 * Return value:  CBF_CONTINUE and CBF_TERMINATE respectively.
 */
static int cbftvsIsSplittable(void * pitem, size_t param)
{
	P_STATEGROUP psg = (P_STATEGROUP)P2P_TNODE_BY(pitem)->pdata;
	if (psg->bsplit)
	{
		*(P_STATEGROUP *)param = psg;
		return CBF_TERMINATE;
	}
	*(P_STATEGROUP *)param = NULL;
	return CBF_CONTINUE;
}

/* Attention:     This Is An Internal Function. No Interface for Library Users.
 * Function name: cbftvsFillStates
 * Description:   Fill state.
 * Parameters:
 *      pitem Pointer to each node of a set.
 *      param Pointer to a size_t[2] array.
 * Return value:  CBF_CONTINUE only.
 */
static int cbftvsFillStates(void * pitem, size_t param)
{
	P_STATEGROUP psg = (P_STATEGROUP)P2P_TNODE_BY(pitem)->pdata;
	P_DFA dfar = (P_DFA)0[(size_t *)param];

	if (EGS_START == psg->egs)
		strSetValueMatrix(dfar, 1, 0, &psg->rep, sizeof(size_t));
	else
	{
		if (EGS_END != psg->egs)
			strSetValueMatrix(dfar, 1[(size_t *)param]++, 0, &psg->rep, sizeof(size_t));
		else
		{
			size_t k;
			k = psg->rep | SIGN;
			strSetValueMatrix(dfar, 1[(size_t *)param]++, 0, &k, sizeof(size_t));
		}
	}

	return CBF_CONTINUE;
}

/* Attention:     This Is An Internal Function. No Interface for Library Users.
 * Function name: cbftvsFillImageArray
 * Description:   Fill image array.
 * Parameters:
 *      pitem Pointer to each node of a set.
 *      param Pointer to a size_t[2] array.
 * Return value:  CBF_CONTINUE and CBF_TERMINATE respectively.
 */
static int cbftvsFillImageArray(void * pitem, size_t param)
{
	P_STATEGROUP psg = (P_STATEGROUP)P2P_TNODE_BY(pitem)->pdata;
	if (setIsMemberT(psg->pset, &0[(size_t *)param], _grpCBFCompareInteger))
	{
		1[(size_t *)param] = psg->rep;
		return CBF_TERMINATE;
	}
	return CBF_CONTINUE;
}

/* Attention:     This Is An Internal Function. No Interface for Library Users.
 * Function name: cbftvsPrintPsetPI
 * Description:   Print set PI.
 * Parameters:
 *      pitem Pointer to each node of a set.
 *      param N/A.
 * Return value:  CBF_CONTINUE only.
 */
static int cbftvsPrintPsetPI(void * pitem, size_t param)
{
	P_STATEGROUP psg = (P_STATEGROUP)P2P_TNODE_BY(pitem)->pdata;
	DWC4100(param);
	printf("PI: bsplit:%d. egs:%d. rep:%zd. { ", psg->bsplit, psg->egs, psg->rep);
	setTraverseT(psg->pset, cbftvsPrintSet, 0, ETM_INORDER);
	printf(" }\n");
	return CBF_CONTINUE;
}

/* Function name: MinimizeDFA
 * Description:   Minimize a DFA.
 * Parameter:
 *       dfa Pointer to a DFA.
 * Return value:  New allocated minimized DFA.
 */
P_DFA MinimizeDFA(P_DFA dfa)
{
	STATEGROUP sgs = { 0 }, sgf = { 0 }, * psg = NULL;
	P_SET_T psetPI, psetEND;
	P_DFA dfar = NULL;
	P_ARRAY_Z parr;
	size_t i, j, k;

	sgs.pset = setCreateT();
	sgf.pset = setCreateT();
	psetPI = setCreateT();
	psetEND = setCreateT();
	
	sgs.bsplit = sgf.bsplit = TRUE;

	sgs.egs = EGS_START;
	sgf.egs = EGS_END;

	for (i = 1; i < dfa->ln; ++i)
	{
		strGetValueMatrix(&j, dfa, i, 0, sizeof(size_t));
		if (SIGN & j)
		{	/* End state. */
			j &= (~SIGN);
			setInsertT(sgf.pset, &j, sizeof(size_t), _grpCBFCompareInteger);
			if (0 == sgf.rep)
				sgf.rep = j;
			setInsertT(psetEND, &j, sizeof(size_t), _grpCBFCompareInteger);
		}
		else /* Normal state. */
		{
			setInsertT(sgs.pset, &j, sizeof(size_t), _grpCBFCompareInteger);
			if (0 == sgs.rep)
				sgs.rep = j;
		}
	}

	setInsertT(psetPI, &sgs, sizeof(STATEGROUP), _grpCBFCompareInteger);
	setInsertT(psetPI, &sgf, sizeof(STATEGROUP), _grpCBFCompareInteger);

#ifdef DEBUG
	setTraverseT(psetPI, cbftvsPrintPsetPI, 0, ETM_INORDER);
#endif

	setTraverseT(psetPI, cbftvsIsSplittable, (size_t)&psg, ETM_LEVELORDER);
	while (psg)
	{
		size_t a[4];
		a[0] = (size_t)psetPI;
		a[1] = (size_t)dfa;
		a[2] = (size_t)psg;
		a[3] = (size_t)psetEND;

		if (treArityBY(P2P_TNODE_BY(*psg->pset)) <= 1)
			psg->bsplit = FALSE;

		if (psg->bsplit)
			setTraverseT(psg->pset, cbftvsSplitSetGroup, (size_t)a, ETM_POSTORDER); /* Must be post-order. */

#ifdef DEBUG
		setTraverseT(psetPI, cbftvsPrintPsetPI, 0, ETM_INORDER);
#endif
		setTraverseT(psetPI, cbftvsIsSplittable, (size_t)&psg, ETM_LEVELORDER);
	}

#ifdef DEBUG
	setTraverseT(psetPI, cbftvsPrintPsetPI, 0, ETM_INORDER);
#endif

	/* Fill DFA. */
	dfar = strCreateMatrix(treArityBY(P2P_TNODE_BY(*psetPI)) + 1, dfa->col, sizeof(size_t));
	memcpy(dfar->arrz.pdata, dfa->arrz.pdata, sizeof(size_t) * dfa->col);
	
	/* Fill states. */
	{
		size_t a[3];
		a[0] = (size_t)dfar;
		a[1] = 2;
		a[2] = (size_t)psetEND;

		setTraverseT(psetPI, cbftvsFillStates, (size_t)a, ETM_INORDER);
	}

	/* Build an array. */
	parr = strCreateArrayZ(dfa->ln, sizeof(size_t) * 2);
	for (i = 1; i < dfa->ln; ++i)
	{
		size_t a[2];

		strGetValueMatrix(&j, dfa, i, 0, sizeof(size_t));
		if (SIGN & j)
			j &= (~SIGN);
		*(size_t *)strLocateItemArrayZ(parr, sizeof(size_t) * 2, i) = j;
		a[0] = j;
		setTraverseT(psetPI, cbftvsFillImageArray, (size_t)a, ETM_LEVELORDER);
		*((size_t *)strLocateItemArrayZ(parr, sizeof(size_t) * 2, i) + 1) = a[1];
	}

	/* Fill table. */
	for (i = 1; i < dfar->ln; ++i)
	{
		strGetValueMatrix(&k, dfar, i, 0, sizeof(size_t));
		if (SIGN & k)
			k &= (~SIGN);
		for (j = 1; j < dfar->col; ++j)
		{
			size_t l;
			strGetValueMatrix(&l, dfa, k, j, sizeof(size_t));
			strSetValueMatrix(dfar, i, j, ((size_t *)strBinarySearchArrayZ(parr, &l, sizeof(size_t) * 2, _grpCBFCompareInteger) + 1), sizeof(size_t));
		}
	}

	svQuickSort(dfar->arrz.pdata + sizeof(size_t) * dfar->col * 2, dfar->ln - 2, sizeof(size_t) * dfar->col, _cbfcmpSize_t);

	DestroyPsetPI(psetPI);
	setDeleteT(psetEND);
	strDeleteArrayZ(parr);

	return dfar;
}

/* Function name: DestroyDFA
 * Description:   Deallocate a DFA.
 * Parameter:
 *       dfa Pointer to a DFA.
 * Return value:  N/A.
 */
void DestroyDFA(P_DFA dfa)
{
	strDeleteMatrix(dfa);
}
