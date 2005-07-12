/* NUM2.c
 *
 * Copyright (C) 1993-2005 David Weenink
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at
 * your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/*
 djmw 20020819 GPL header
 djmw 20020819 Split nonGLP part off.
 djmw 20001109 Changed stop criteria in NUMsvdcmp and NUMtqli.
 djmw 20020819 Split into GPL and nonGPL part.
 djmw 20021008 Removed SVD_sort.
 djmw 20030619 Removed calls to NRC svd-routines.
 djmw 20030623 Removed tqli en tred calls.
 djmw 20030703 Replaced NUMincompleteBeta with gsl_sf_beta_inc.
 djmw 20030710 NUMminimize_brent now also returns the minimum function value.
 djmw 20030731 NUMridders: better approximation for small d.
 			   NUMinvFisherQ better approximation for p < 0.5
 djmw 20030813 Added NUMmad and NUMstatistics_huber.
 djmw 20030825 Replaced gsl_sf_beta_inc with NUMincompleteBeta
 pb   20030828 Improvements for invFisherQ, ridders, studentP, studentQ, 
 	invStudentQ, invChiSquareQ: modifications for 'undefined' return values.
 djmw 20030830 Coorected a bug in NUMtriangularfilter_amplitude
 djmw 20031111 Added NUMdmatrix_transpose, NUMdmatrix_printMatlabForm
 djmw 20040105 Added NUMmahalanobisDistance_chi
 djmw 20040211 Modified NUMstrings_copyElements: if (form[i]==NULL) then {to[i]= NULL}.
 djmw 20040303 Added NUMstring_containsPrintableCharacter.
 djmw 20050406 NUMprocrutus->NUMprocrustes
*/

#include "SVD.h"
#include "Eigen.h"
#include "NUMclapack.h"
#ifndef _NUM_h_
	#include "NUM.h"
#endif
#include "NUM2.h"
#include "NUMmachar.h"
#include "melder.h"
#include "gsl_sf_gamma.h"
#include <ctype.h>

#define my me ->

#define MAX(m,n) ((m) > (n) ? (m) : (n))
#define MIN(m,n) ((m) < (n) ? (m) : (n))
#define SIGN(a,b) ((b < 0) ? -fabs(a) : fabs(a))
 
extern machar_Table NUMfpp;

struct pdf1_struct { double p; long df; };
struct pdf2_struct { double p; long df1; long df2; };

static double NUMgauss (double x)
{
	return exp (- 0.5 * x * x) / sqrt(2 * NUMpi);
}

int NUMdmatrix_hasInfinities (double **m, long rb, long re, long cb, long ce)
{
	long i, j; double min, max;
	min = max = m[rb][cb];
	for (i=rb; i <= re; i++)
	{
		for (j=cb; j <= ce; j++)
		{
			if (m[i][j] > max) max = m[i][j];
			else if (m[i][j] < min) min = m[i][j];
		}
	}
	if (! NUMfpp) NUMmachar ();
	return max >= NUMfpp -> rmax || min <= - NUMfpp -> rmax;
}

int NUMstring_containsPrintableCharacter (char *s)
{
	long i, len; 
	if (s == NULL || ((len = strlen (s)) == 0)) return 0;
	for (i = 0; i < len; i++)
	{
		int c = s[i];
		if (isgraph (c)) return 1;
	}
	return 0;
}

double *NUMstring_to_numbers (const char *s, long *numbers_found)
{
	char *dup = NULL, *token, *delimiter = " ,\t";
	long capacity = 100, n; double *numbers = NULL;
	
	*numbers_found = n = 0;
	
	if (((dup = Melder_strdup (s)) == NULL) ||
		((numbers = NUMdvector (1, capacity)) == NULL)) goto end;
	token = strtok (dup, delimiter);
	while (token)
	{
		double value = atof (token);
		if (n > capacity) 
		{
			long newsize = 2 * capacity; double *new;
			if (! (new = Melder_realloc (numbers, newsize))) goto end;
			numbers = new; capacity = newsize;
		}
		numbers[++n] = value;
		token = strtok (NULL, delimiter);
	}
end:
	*numbers_found = n;
	Melder_free (dup);
	if (Melder_hasError())
	{
		NUMdvector_free (numbers, 1);
		*numbers_found = 0;
	}
	return numbers;
}

int NUMstrings_equal (char **s1, char **s2, long lo, long hi)
{
	long i;
	for (i=lo; i <= hi; i++)
	{
		if (NUMstrcmp (s1[i], s2[i])) return 0;
	}
	return 1;
}

int NUMstrings_copyElements (char **from, char**to, long lo, long hi)
{
	long i;
	
	for (i = lo; i <= hi; i++)
	{
		Melder_free (to[i]);
		if (from[i] && ((to[i] = Melder_strdup (from[i])) == NULL)) return 0;
	}
	return 1;
}

void NUMstrings_free (char **s, long lo, long hi)
{
	long i;
	if (s == NULL) return;
	for (i=lo; i <= hi; i++) Melder_free (s[i]);
	NUMpvector_free (s, lo);
}

char **NUMstrings_copy (char **from, long lo, long hi)
{
	char **to = (char**) NUMpvector (lo, hi);
	if (to == NULL || ! NUMstrings_copyElements (from, to, lo, hi))
		NUMstrings_free (to, lo, hi);
	return to;
}

static char *appendNumberToString (char *s, long number)
{
	char buf[12], *new;
	long ncharb, nchars = 0;
	
	ncharb = sprintf (buf, "%d", number);
	if (s != NULL) nchars = strlen (s);
	new = Melder_malloc (nchars + ncharb + 1);
	if (new == NULL) return NULL;
	if (nchars > 0) strncpy (new, s, nchars);
	strncpy (new + nchars, buf, ncharb + 1);
	return new;
} 

int NUMstrings_setSequentialNumbering (char **s, long lo, long hi, 
	char *pre, long number, long increment)
{
	long i;
	
	for (i = lo; i <= hi; i++, number += increment)
	{
		char *new = appendNumberToString (pre, number);
		if (new == NULL) return 0;
		Melder_free (s[i]);
		s[i] = new;
	}
	return 1;
}

#define HIGHBYTE(x) ((unsigned char) ((x) & 0xFF))
#define LOWBYTE(x)  ((unsigned char) ((x) >> 8 & 0xFF))

/* a+b=c in radix 256 */
void NUMstring_add (unsigned char *a, unsigned char *b, unsigned char *c, long n);
void NUMstring_add (unsigned char *a, unsigned char *b, unsigned char *c, long n)
{
	int j;
	unsigned short reg = 0;
	
	for (j = n; j > 1; j--)
	{
		reg = a[j] + b[j] + HIGHBYTE (reg);
		c[j+1] = LOWBYTE (reg);
	}
}

/*
int NUMstrings_setSequential (char **s, long lo, long hi, 
	char *pre, char *start, long increment, char *post)
{
	char *proc = "NUMstrings_setSequential";
	long d, i, iset = 0, istart, startlen, nsymbolsets = 3, plast;
	long startnum = 0, prelen, postlen, setsize, maxnum, maxchar = 100;
	int symbolSetSize[4] = {0, 10, 26, 26};
	char *p, *psymbols, *pt, *pinc;
	char *pres = pre == NULL ? "" : pre; 
	char *posts = post == NULL ? "" : post;
	char *number = NULL, *inc = NULL;
	char *symbolSet[] = {"", "0123456789", "abcdefghijklmnopqrstuvwxyz", 
		"ABCDEFGHIJKLMNOPQRSTUVWXYZ"};

	// First char of start determines the symbol set
		
	for (i = 1; i <= nsymbolsets; i++)
	{
		p = strchr (symbolSet[i], start[0]);
		if (p != NULL)
		{
			iset = i; istart = p - start;
			break;
		}
	}
	
	if (iset == 0) return Melder_error ("%s: Illegal first character in start.", proc);
	if (istart == 0) iset = 1; // empty start 
	
	if (((inc = Melder_malloc (maxchar)) == NULL) ||
		((startnum = Melder_malloc (maxchar)) == NULL)) goto end;
	
	// Translate the increment into the symbol system
	
	psymbols = symbolSet[iset];
	setsize = symbolSetSize[iset];
	
	pinc = inc;
	d = increment;
	while (d > 0)
	{
		long mod = d % setsize;
		d /= setsize;
		pt = strchr (psymbols, *pinc);
		pinc++;
	}
	// Get the 'number' of the start symbol in the corresponding
	// number system and check if all start's chars are in the set.
	
	p = start;
	psymbols = symbolSet[iset];
	setsize = symbolSetSize[iset];
	while (*p)
	{
		pt = strchr (psymbols, *p);
		if (pt == NULL) return Melder_error ("%s: Illegal symbol in start.", proc); // mixed symbols
		startnum = startnum * setsize + (psymbols - pt);
		p++;
	}
	startlen = p - start; // length of start (== strlen(start))

	maxchar = MAX (100, startlen);	
	number = Melder_malloc (maxchar); // number in reverse
		
	prelen = strlen (pre);
	postlen = strlen (post);
	inclen =   
	for (i = lo; i <= hi; i++)
	{
		char *new;
		// add the strings
		for (j=1; j <= MAX (
		if (new == NULL) return 0;
		Melder_free (s[i]);
		s[i] = new;
	}
end:
	Melder_free (inc);
	Melder_free (startnum);
	return ! Melder_hasError ();
}
*/ 
char *strstr_regexp (const char *string, const char *search_regexp)
{
	char *charp = NULL, *compileMsg;
	regexp *compiled_regexp = CompileRE (search_regexp, &compileMsg, 0);
	
	if (compiled_regexp == NULL) return Melder_errorp (compileMsg);
	
	if (ExecRE(compiled_regexp, NULL, string, NULL, 0, '\0', '\0', NULL))
		charp = compiled_regexp -> startp[0];
	free (compiled_regexp);
	return charp;
}

char *str_replace_literal (char *string, const char *search, 
	const char *replace, long maximumNumberOfReplaces, long *nmatches)
{
	int len_replace, len_search, len_string, len_result;
	int i, nchar, result_nchar = 0;
	char *pos; 	/* current position / start of current match */
	char *posp; /* end of previous match */
	char *result;
	
	if (string == NULL || search == NULL || replace == NULL) return NULL;
	
	len_search = strlen (search);
	len_replace = strlen (replace);
	
	/*
		To allocate memory for 'result' only once, we have to know how many
		matches will occur.
	*/
	
	pos = string; *nmatches = 0;
	if (maximumNumberOfReplaces <= 0) maximumNumberOfReplaces = LONG_MAX;
	while ((pos = strstr (pos, search)) && *nmatches < maximumNumberOfReplaces)
	{
		pos += len_search; 
		(*nmatches)++;	
	}
	
	len_string = strlen (string);
	len_result = len_string + *nmatches * (len_replace - len_search);
	result = (char *) Melder_malloc (len_result + 1);
	result[len_result] = '\0';
	if (result == NULL) return NULL;
	
	pos = posp = string;
	for (i=1; i <= *nmatches; i++)
	{
		pos = strstr (pos, search);
		
		/* 
			Copy gap between end of previous match and start of current.
		*/
		
		nchar = (pos - posp);
		if (nchar > 0)
		{
			strncpy (result + result_nchar, posp, nchar);
			result_nchar += nchar;
		}
		
		/*
			Insert the replace string in result.
		*/
		
		strncpy (result + result_nchar, replace, len_replace);
		result_nchar += len_replace;
		
		/*
			Next search starts after the match.
		*/
		
		pos += len_search;
		posp = pos;
	}
	
	/*
		Copy gap between end of match and end of string.
	*/
	
	pos = string + len_string;
	nchar = pos - posp;
	if (nchar > 0) strncpy (result + result_nchar, posp, nchar);
	return result; 
}


char *str_replace_regexp (char *string, regexp *compiledSearchRE, 
	const char *replaceRE, long maximumNumberOfReplaces, long *nmatches)
{
	long i = maximumNumberOfReplaces > 0 ? 0 : -214748363;
	int buf_size; 					/* inclusive nul-byte */
	int buf_nchar = 0;				/* # characters in 'buf' */
	int string_length;				/* exclusive 0-byte*/
	int gap_copied = 0;
	int nchar, reverse = 0;
	char *pos; 	/* current position in 'string' / start of current match */
	char *posp; /* end of previous match */
	char *buf, *tbuf;

	*nmatches = 0;	
	if (string == NULL || compiledSearchRE == NULL || replaceRE == NULL)
		return NULL;
		
	string_length = strlen (string);
		  
	/*
		We do not know the size of the replaced string in advance,
		therefor, we allocate a replace buffer twice the size of the 
		original string. After all replaces have taken place we do a
		final realloc to the then exactly known size. 
		If during the replace, the size of the buffer happens to be too
		small (this is signalled when the last byte in the buffer equals 
		'\0' after the replace), we double its size and restart the replace.
		We cannot detect, however, whether the buffer was exactly filled up
		or it was too small. In the former case we are penalized by one 
		extra memory reallocation.
	*/
	  	
	buf_size = 2 * string_length + 1;	
	buf = (char *) Melder_malloc (buf_size);
	if (buf == NULL) return 0;
	
	/*
		The last byte of 'buf' is our buffer_full test.
		Make sure it is not '\0'.
	*/
	
	buf[buf_size - 1] = 'a';
	
	pos = posp = string;
	while (ExecRE(compiledSearchRE, NULL, pos, NULL, reverse, 
		pos == posp ? '\0' : pos[-1], '\0', NULL) && 
		i++ < maximumNumberOfReplaces)
	{
		/*
			Copy gap between the end of the previous match and the start
			of the current match.
			Check buffer overflow. 
		*/
		
		pos = compiledSearchRE -> startp[0];
		nchar = pos - posp;
		if (nchar > 0 && ! gap_copied)
		{
			if (buf_nchar + nchar > buf_size - 1)
			{
				buf_size *= 2;
				tbuf = (char *) Melder_realloc (buf, buf_size);
				if (tbuf == NULL) goto end;
				buf = tbuf;
				buf[buf_size - 1] = 'a';
			}
			strncpy (buf + buf_nchar, posp, nchar);
			buf_nchar += nchar;
		}
		
		gap_copied = 1;
		
		/*
			Do the substitution. We can only check afterwards for buffer
			overflow. SubstituteRE puts null byte at last replaced position.
		*/
		
		SubstituteRE (compiledSearchRE, replaceRE, buf + buf_nchar,
			buf_size - buf_nchar);
		
		/*
			Check buffer overflow; 
		*/
		
		if (buf[buf_size - 1] == '\0')
		{
			buf_size *= 2;
			tbuf = (char *) Melder_realloc (buf, buf_size);
			if (tbuf == NULL) goto end;
			buf = tbuf;
			buf[buf_size - 1] = 'a';
			/* redo */
			i--;
			continue;
		}
		
		/*
			Buffer is not full, get number of characters added;
		*/
		
		nchar = strlen (buf + buf_nchar);
		buf_nchar += nchar;
		
		/*
			Update next start position in search string.
		*/
		
		pos = posp = compiledSearchRE -> endp[0];
		gap_copied = 0;
		(*nmatches)++;
	}
	
	/*
		Were done, final allocation to exact size
	*/
	
	pos = string + string_length;
	nchar = pos - posp;
	buf_size = buf_nchar + nchar;
	tbuf = (char *) Melder_realloc (buf, buf_size + 1);
	if (tbuf == NULL) goto end;
	buf = tbuf;
	
	/*
		Copy the last part of 'string'.
	*/
	
	strncpy (buf + buf_nchar, posp, nchar);
	buf[buf_size] = '\0';
	
end:

	if (Melder_hasError ()) Melder_free (buf);
	return buf;
}

static char **strs_replace_literal (char **from, long lo, long hi, 
	const char *search, const char *replace, int maximumNumberOfReplaces,
	long *nmatches, long *nstringmatches)
{
	char **result;
	long i, nmatches_sub = 0;

	if (search == NULL || replace == NULL) return NULL;
	
	result = (char**) NUMpvector (lo, hi);
	if (result == NULL) goto end;
	
	*nmatches = 0; *nstringmatches = 0;
	for (i = lo; i <= hi; i++)
	{
		if (from[i] == NULL) continue;

		result[i] = str_replace_literal (from[i], search, replace, 
			maximumNumberOfReplaces, &nmatches_sub);
		if (result[i] == NULL) goto end;
		if (nmatches_sub > 0)
		{
			*nmatches += nmatches_sub;
			(*nstringmatches)++;
		}
	}
	
end:

	if (Melder_hasError ()) NUMstrings_free (result, lo, hi);
	return result;
}


static char **strs_replace_regexp (char **from, long lo, long hi, 
	const char *searchRE, const char *replaceRE, int maximumNumberOfReplaces,
	long *nmatches, long *nstringmatches)
{
	regexp *compiledRE;
	char *compileMsg, **result = NULL;
	long i, nmatches_sub = 0;

	if (searchRE == NULL || replaceRE == NULL) return NULL;
				
	compiledRE = CompileRE (searchRE, &compileMsg, 0);
	if (compiledRE == NULL) return Melder_errorp (compileMsg);

	result = (char **) NUMpvector (lo, hi);
	if (result == NULL) goto end;
	 
	*nmatches = 0; *nstringmatches = 0;
	for (i = lo; i <= hi; i++)
	{
		if (from[i] == NULL) continue;
		result[i] = str_replace_regexp (from[i], compiledRE, replaceRE, 
			maximumNumberOfReplaces, &nmatches_sub);
		if (result[i] == NULL) goto end;
		if (nmatches_sub > 0)
		{
			*nmatches += nmatches_sub;
			(*nstringmatches)++;
		}
	}
	
end:
	
	free (compiledRE);
	if (Melder_hasError ()) NUMstrings_free (result, lo, hi);
	return result;	
}

char **strs_replace (char **from, long lo, long hi, 
	const char *search, const char *replace, int maximumNumberOfReplaces,
	long *nmatches, long *nstringmatches, int use_regexp)
{
	if (use_regexp) return strs_replace_regexp (from, lo, hi, search,
		replace, maximumNumberOfReplaces, nmatches, nstringmatches);
	else return strs_replace_literal (from, lo, hi, search, replace,
		maximumNumberOfReplaces, nmatches, nstringmatches);
}

void NUMdmatrix_printMatlabForm (double **m, long nr, long nc, char *name)
{
	long i, j, k, npc = 5;
	div_t n = div (nc, npc);

	Melder_print ("%s=[", name);
	for (i = 1; i <= nr; i++)
	{
		for (j = 1; j <= n.quot; j++)
		{
			for (k = 1; k <= npc; k++)
			{
				Melder_print ("%.12g%s", m[i][(j-1)*npc+k], k < npc ? ", " : "");
			}
			Melder_print ("%s", j < n.quot ? ",\n" : "");
		}

		for (k = 1; k <= n.rem; k++)
		{
			Melder_print ("%.12g%s", m[i][n.quot*npc + k], k < n.rem ? ", " : "");
		}
		Melder_print("%s", i < nr ? ";\n" : "];\n");
	}
}

double **NUMdmatrix_transpose (double **m, long nr, long nc)
{
	long i = 1, j = 1;
	double **to = NUMdmatrix (1, nc, 1, nr);

	if (to == NULL) return NULL;

	for (i = 1; i <= nr; i++)
	{
		for (j = 1; j <= nc; j++)
		{
			to[j][i] = m[i][j];
		}
	}
	return to;
}

void NUMcentreRows_d (double **a, long rb, long re, long cb, long ce)
{
	long i, j;
	for (i=rb; i <= re; i++)
	{
		double rowmean = 0;
		for (j=cb; j <= ce; j++) rowmean += a[i][j];
		rowmean /= (ce - cb + 1);
		for (j=cb; j <= ce; j++) a[i][j] -= rowmean;
	}
}

void NUMcentreColumns_d (double **a, long rb, long re, long cb, long ce, 
	double *centres)
{
	long i, j;
	for (j=cb; j <= ce; j++)
	{
		double colmean = 0;
		for (i=rb; i <= re; i++) colmean += a[i][j];
		colmean /= (re - rb + 1);
		for (i=rb; i <= re; i++) a[i][j] -= colmean;
		if (centres != NULL) centres[j - cb + 1] = colmean;
	}
}

void NUMdoubleCentre_d (double **a, long rb, long re, long cb, long ce)
{
	NUMcentreRows_d (a, rb, re, cb, ce);
	NUMcentreColumns_d (a, rb, re, cb, ce, NULL);
}

void NUMnormalizeColumns_d (double **a, long nr, long nc, double norm)
{
	long i, j; double s;
	Melder_assert (norm > 0);
	for (j=1; j <= nc; j++)
	{
		for (s=0, i=1; i <= nr; i++) s += a[i][j] * a[i][j];
		if (s <= 0) continue;
		s = sqrt (norm / s);
		for (i=1; i <= nr; i++) a[i][j] *= s;
	}
}

void NUMnormalizeRows_d (double **a, long nr, long nc, double norm)
{
	long i, j; double s;
	Melder_assert (norm > 0);
	for (i=1; i <= nr; i++)
	{
		for (s=0, j=1; j <= nc; j++) s += a[i][j] * a[i][j];
		if (s <= 0) continue;
		s = sqrt (norm / s);
		for (j=1; j <= nc; j++) a[i][j] *= s;
	}
}

void NUMnormalize_d (double **a, long nr, long nc, double norm)
{
	double sq; long i, j;
	Melder_assert (norm > 0);
	for (sq=0, i=1; i <= nr; i++)
	{
		for (j=1; j <= nc; j++) sq += a[i][j] * a[i][j];
	}
	if (sq <= 0) return;
	norm = sqrt (norm / sq);
	for (i=1; i <= nr; i++)
	{
		for (j=1; j <= nc; j++) a[i][j] *= norm;
	}
}

void NUMstandardizeColumns (double **a, long rb, long re, long cb, long ce)
{
	long i, j, n = re - rb + 1;
	if (n < 2) return;
	for (j=cb; j <= ce; j++)
	{
		double ep = 0, s = 0, ave, sdev, var = 0;
		for (i=rb; i <= re; i++) s += a[i][j];
		ave = s / n;
		for (i=rb; i <= re; i++)
		{
			s = a[i][j] - ave;
			ep += s;
			var += s * s;
		}
		if (ave != 0) for (i=rb; i <= re; i++) a[i][j] -= ave;
		if (var > 0)
		{
			var = (var - ep * ep / n) / (n - 1);
			sdev = sqrt (var);
			for (i=rb; i <= re; i++) a[i][j] /= sdev;
		}
	}
}

void NUMstandardizeRows (double **a, long rb, long re, long cb, long ce)
{
	long i, j, n = ce - cb + 1;
	if (n < 2) return;
	for (i=rb; i <= re; i++)
	{
		double ep = 0, s = 0, ave, sdev, var = 0;
		for (j=cb; j <= ce; j++) s += a[i][j];
		ave = s / n;
		for (j=cb; j <= ce; j++)
		{
			s = a[i][j] - ave;
			ep += s;
			var += s * s;
		}
		if (ave != 0) for (j=cb; j <= ce; j++) a[i][j] -= ave;
		if (var > 0)
		{
			var = (var - ep * ep / n) / (n - 1);
			sdev = sqrt (var);
			for (j=cb; j <= ce; j++) a[i][j] /= sdev;
		}
	}
}

void NUMaverageColumns (double **a, long rb, long re, long cb, long ce)
{
	long i, j, n = re - rb + 1;
	if (n < 2) return;
	for (j = cb; j <= ce; j++)
	{
		double ave = 0;
		for (i = rb; i <= re; i++) ave += a[i][j];
		ave /= n;
		for (i = rb; i <= re; i++) a[i][j] = ave;
	}
	
}

void NUMcolumn_avevar (double **a, long nr, long nc, long icol, 
	double *average, double *variance)
{
	long i;
	double eps = 0, mean = 0, var = 0;
	
	Melder_assert (nr > 0 && nc > 0 && icol > 0 && icol <= nc);

	for (i = 1; i <= nr; i++)
	{
		mean += a[i][icol];
	}
	
	mean /= nr;
	
	if (average != NULL) *average = mean;
	
	if (variance == NULL) return;
	
	if (nr > 1)
	{
		for (i = 1; i <= nr; i++)
		{
			double s = a[i][icol] - mean;
			eps += s;
			var += s * s;
		}

		var = (var - eps * eps / nr) / (nr -1);
	}
	else
	{
		var = NUMundefined;
	}
	
	*variance = var;
}

void NUMcolumn2_avevar (double **a, long nr, long nc, long icol1, long icol2,
	double *average1, double *variance1, double *average2, double *variance2,
	double *covariance)
{
	long i, ndf = nr - 1;
	double eps1 = 0, eps2 = 0, mean1 = 0, mean2 = 0;
	double var1 = 0, var2 = 0, covar = 0;
	
	Melder_assert (icol1 > 0 && icol1 <= nc && icol2 > 0 && icol2 <= nc);

	for (i = 1; i <= nr; i++)
	{
		mean1 += a[i][icol1];
		mean2 += a[i][icol2];
	}
	
	mean1 /= nr;
	mean2 /= nr;
	
	if (average1 != NULL) *average1 = mean1;
	if (average2 != NULL) *average2 = mean2;

	if (variance1 == NULL && variance2 == NULL && covariance == NULL) return;
	
	if (nr > 1)
	{	
		for (i = 1; i <= nr; i++)
		{
			double s1 = a[i][icol1] - mean1;
			double s2 = a[i][icol2] - mean2;
			eps1 += s1;
			eps2 += s2;
			var1 += s1 * s1;
			var2 += s2 * s2;
			covar += s1 * s2;
		}

		var1 = (var1 - eps1 * eps1 / nr) / ndf;
		var2 = (var2 - eps2 * eps2 / nr) / ndf;
		covar /= ndf;
	}
	else
	{
		var1 = NUMundefined;
		var2 = NUMundefined;
		covar = NUMundefined;
	}
	
	if (variance1 != NULL) *variance1 = var1;
	if (variance2 != NULL) *variance2 = var2;
	if (covariance != NULL) *covariance = covar;
	if (icol1 == icol2) *covariance = *variance1;
}

#define HUBER_MAD NUMmad_f
#define HUBER_STATISTICS_HUBER NUMstatistics_huber_f
#define HUBER_DATA_TYPE float
#define HUBER_VECTOR  NUMfvector
#define HUBER_VECTOR_FREE  NUMfvector_free
#define HUBER_QUANTILE NUMquantile_f
#define HUBER_SORT NUMsort_f
#include "NUMhuber_core.h"
#undef HUBER_MAD
#undef HUBER_STATISTICS_HUBER
#undef HUBER_DATA_TYPE
#undef HUBER_VECTOR
#undef HUBER_VECTOR_FREE
#undef HUBER_QUANTILE
#undef HUBER_SORT

#define HUBER_MAD NUMmad_d
#define HUBER_STATISTICS_HUBER NUMstatistics_huber_d
#define HUBER_DATA_TYPE double
#define HUBER_VECTOR  NUMdvector
#define HUBER_VECTOR_FREE  NUMdvector_free
#define HUBER_QUANTILE NUMquantile_d
#define HUBER_SORT NUMsort_d
#include "NUMhuber_core.h"



void eigenSort_d (double d[], double **v, long n, int sort)
{
    long i, j, k;
	if (sort == 0) return;
    for (i=1; i < n; i++)
    {
        double temp = d[k=i];
        if (sort > 0)
		{
			for (j=i+1; j <= n; j++)
			{
				if (d[j] > temp) temp = d[k=j];
			}
		}
        else
		{
			for (j=i+1; j <= n; j++)
			{
				if (d[j] < temp) temp = d[k=j];
			}
		}
        if (k != i)
        {
            d[k] = d[i];
			d[i] = temp;
            if (v)
			{
				for (j=1; j <= n; j++)
				{
					temp = v[j][i];
					v[j][i] = v[j][k];
					v[j][k] = temp;
				}
			}
        }
    }
}

int NUMstrcmp (const char *s1, const char *s2)
{
	if (s1 == NULL || s1[0] == '\0')
	{
		if (s2 != NULL && s2[0] != '\0') return -1;
		else return 0;
	}
	else
	{
		if (s2 == NULL) return +1;
		else return strcmp (s1, s2);
	}
}

void NUMlocate_f (float *xx, long n, float x, long *index)
{
	long ju = n + 1, jm, jl = 0;
	int ascend = xx[n] >= xx[1];

	while (ju - jl > 1)
	{
		jm = (ju + jl) / 2;
		if (x >= xx[jm] == ascend) jl = jm; else ju = jm;
	}
	if (x == xx[1]) *index = 1;
	else if (x == xx[n]) *index = n - 1;
	else *index = jl;
}

void NUMlocate_d (double *xx, long n, double x, long *index)
{
	long ju = n + 1, jm, jl = 0;
	int ascend = xx[n] >= xx[1];

	while (ju - jl > 1)
	{
		jm = (ju + jl) / 2;
		if (x >= xx[jm] == ascend) jl = jm; else ju = jm;
	}
	if (x == xx[1]) *index = 1;
	else if (x == xx[n]) *index = n - 1;
	else *index = jl;
}

/*
	The following algorithm for monotone regession is on the average
	3.5 times faster than
	Kruskal's algorithm for monotone regression (and much simpler).
	Regression is ascending
*/
void NUMmonotoneRegression_d (const double x[], long n, double xs[])
{
	double sum, xt;
	long i, j, nt;
	
	for (i = 1; i <= n; i++)
	{
		xs[i] = x[i];
	}
	
	for (i = 2; i <= n; i++)
	{
		if (xs[i] >= xs[i-1]) continue;
		sum = xs[i];
		nt = 1;
		for (j = 1; j <= i - 1; j++)
		{
			sum += xs[i-j]; nt++;
			xt = sum / nt;
			if (j < i-1 && xt >= xs[i-j-1]) break;
		}
		for (j = i - nt + 1; j <= i; j++)
		{
			xs[j] = xt;
		}
	}
}

static int NUMdmonotoneRegressionKruskal (const double x[], long n, double xm[], int descending)
{
	long i, current = 1, nBlocks = n, *length, index = 1; int status = 0;
	double low = -1e38, high = 1e38;
	for (i=1; i <= n; i++) xm[i] = x[i];
	if ((length = NUMlvector (1, n)) == NULL) goto end;
	if (descending) { low = 1e38; high = -1e38; } 
	for (i=1; i <= n; i++) length[i] = 1;
	do
	{
		int up_satisfied, down_satisfied;
		index += current > 1 ? length[current-1] : 0;
		do
		{
			long l_current = length[current], l_join;
			long l_prev = current > 1 ? length[current-1] : 0;
			double x_current = xm[index], x_join;
			double x_prev = current > 1 ? xm[index-l_prev] : low;
			double x_next = current < nBlocks ? xm[index+l_current] : high;
			up_satisfied = descending ? x_current > x_next : x_current < x_next;
			if (! up_satisfied) /* join current and next block */
			{
				l_join = l_current + length[current+1];
				x_join = (x_current * l_current + x_next *
					length[current+1]) / l_join;
				for (i=1; i <= l_join; i++) xm[index + i - 1] = x_join;
				length[current] = l_join; nBlocks--;
				for (i=current+1; i <= nBlocks; i++) length[i] = length[i+1];
				x_current = x_join; l_current = l_join;
			}
			down_satisfied = descending ? x_current < x_prev :
				x_current > x_prev;
			if (! down_satisfied) /* join current and previous block */
			{
				l_join = l_current + l_prev;
				x_join = (x_current * l_current + x_prev * l_prev) / l_join;
				index -= l_prev;
				Melder_assert (index > 0);
				for (i=1; i <= l_join; i++) xm[index + i - 1] = x_join;
				current--; length[current] = l_join; nBlocks--;
				for (i=current+1; i <= nBlocks; i++) length[i] = length[i+1];
			}
		} while (! (up_satisfied && down_satisfied));
		current++;
	} while (current < nBlocks);
	status = 1;
end:
	NUMlvector_free (length, 1);
	return status;
}

void NUMfvector_moment2 (float v[], long lo, long hi, double *ave, double *var)
{
	double m, s, mp = v[lo], sp = 0;
	long k;
	
	for (k = lo+1; k <= hi; k++)
	{
		double xk = v[k];
		double tmp = xk - mp;
		m = mp + tmp / k;
		s = sp + tmp * (xk - m);
		mp = m; sp = s;
	}
	*ave = m;
	*var = s / (hi - lo);
}

float NUMfvector_getNorm1 (const float v[], long n)
{
	float norm = 0; long i;
	for (i=1; i <= n; i++) norm += fabs (v[i]);
	return norm;
}

float NUMfvector_getNorm2 (const float v[], long n)
{
	float norm = 0; long i;
	for (i=1; i <= n; i++) norm += v[i] * v[i];
	return sqrt (norm);
}

float NUMfvector_normalize1 (float v[], long n)
{
	float norm = 0; long i;
	for (i=1; i <= n; i++) norm += fabs (v[i]);
	for (i=1; i <= n; i++) v[i] /= norm;
	return norm;
}

float NUMfvector_normalize2 (float v[], long n)
{
	float norm = 0; long i;
	for (i=1; i <= n; i++) norm += v[i] * v[i];
	norm = sqrt (norm);
	for (i=1; i <= n; i++) v[i] /= norm;
	return norm;
}
double NUMdvector_getNorm1 (const double v[], long n)
{
	double norm = 0; long i;
	for (i=1; i <= n; i++) norm += fabs (v[i]);
	return norm;
}

double NUMdvector_getNorm2 (const double v[], long n)
{
	double norm = 0; long i;
	for (i=1; i <= n; i++) norm += v[i] * v[i];
	return sqrt (norm);
}

double NUMdvector_normalize1 (double v[], long n)
{
	double norm = 0; long i;
	for (i=1; i <= n; i++) norm += fabs (v[i]);
	if (norm > 0) for (i=1; i <= n; i++) v[i] /= norm;
	return norm;
}

double NUMdvector_normalize2 (double v[], long n)
{
	double norm = 0; long i;
	for (i = 1; i <= n; i++) norm += v[i] * v[i];
	norm = sqrt (norm);
	if (norm > 0) for (i = 1; i <= n; i++) v[i] /= norm;
	return norm;
}

#undef TINY

void NUMcholeskySolve (double **a, long n, double d[], double b[], double x[])
{
	long i, k;
	double sum;
	
	for (i=1; i <= n; i++) /* Solve L.y=b */
	{
		for (sum = b[i], k = i - 1; k >= 1; k--)
		{
			sum -= a[i][k] * x[k];
		}
		x[i] = sum / d[i];
	}
	for (i = n; i >= 1; i--) /* Solve L^T.x=y */
	{
		for (sum = x[i], k = i +1; k <= n; k++)
		{
			sum -= a[k][i] * x[k];
		}
		x[i] = sum / d[i];
	}
}

int NUMdeterminant_cholesky (double **a, long n, double *lnd)
{
	double *d;
	char uplo = 'U';
	long i, j, lda = n, info;

	/* Save the diagonal */
	
	if ((d = NUMdvector (1, n)) == NULL) return 0;
	for (i = 1; i <= n; i++) d[i] = a[i][i];

	/* Cholesky decomposition in lower, leave upper intact */
	
	(void) NUMlapack_dpotf2 (&uplo, &n, &a[1][1], &lda, &info);
	if (info != 0) goto end;

	/* Determinant from diagonal, restore diagonal */
		
	for (*lnd = 0, i = 1; i <= n; i++)
	{
		*lnd += log (a[i][i]);
		a[i][i] = d[i];
	}
	*lnd *= 2; /* because A = L . L' */
	
end:
	/* Restore lower from upper */
	
	for (i = 1; i < n; i++)
	{
		for (j = i + 1; j <= n; j++) a[j][i] = a[i][j];
	}
	
	NUMdvector_free (d, 1);
	return info == 0;
}
/*
int NUMdeterminant_cholesky2 (double **a, long n, double *lnd)
{
	double *d = NULL; 
	long i, j;
	
	if ((d = NUMdvector (1, n)) == NULL) return 0;
	
	if (NUMcholeskyDecomposition(a, n, d))
	{
		if (lnd != NULL)
		{
			for (*lnd = 0, i = 1; i <= n; i++) *lnd += log (d[i]);
			*lnd *= 2; 
		}
		
		for (i = 2; i <= n; i++)
		{
			for (j = 1; j < i; j++) a[i][j] = a[j][i];	
		}
	}
	NUMdvector_free (d, 1);
	return ! Melder_hasError ();
}
*/

int NUMinverse_cholesky (double **a, long n, double *lnd)
{
	char uplo = 'U', diag = 'N';
	long i, info;

	/* Cholesky decomposition in lower, leave upper intact
		Fortran storage -> use uplo='U' to get 'L'.
	*/

	(void) NUMlapack_dpotf2 (&uplo, &n, &a[1][1], &n, &info);
	if (info != 0) return 0;

	/* Determinant from diagonal, restore diagonal */

	for (*lnd = 0, i = 1; i <= n; i++)
	{
		*lnd += log (a[i][i]);
	}
	*lnd *= 2; /* because A = L . L' */

	/* Get the inverse */
	
	(void) NUMlapack_dtrtri (&uplo, &diag, &n, &a[1][1], &n, &info);

	return info == 0;
}

double NUMmahalanobisDistance_chi (double **linv, double *v, double *m, long n)
{
	long i, j; 
	double chisq = 0;

	for (i = n; i > 0; i--)
	{
		double t = 0;
		for (j = 1; j <= i; j++)
		{
			t += linv[i][j] * (v[j] - m[j]);
		}	
		chisq += t * t;
	}
	return chisq;
}

double NUMtrace (double **a, long n)
{
	long i; double trace = 0;
	for (i=1; i <= n; i++) trace += a[i][i];
	return trace;
}

double NUMtrace2 (double **a1, double **a2, long n)
{
	long i, k; double trace = 0;
	for (i=1; i <= n; i++) for (k=1; k <= n; k++) trace += a1[i][k] * a2[k][i];
	return trace;
}

int NUMeigensystem_d (double **a, long n, double **evec, double eval[])
{
	Eigen me = new (Eigen);
	
	if (me == NULL || ! Eigen_initFromSymmetricMatrix (me, a, n)) goto end;
	if (evec) NUMdmatrix_copyElements (my eigenvectors, evec, 1, n, 1, n);
	if (eval) NUMdvector_copyElements (my eigenvalues, eval, 1, n);
end:
	forget (me);
	return ! Melder_hasError ();
}

int NUMdominantEigenvector_d (double **mns, long n, double *q,
	double *lambda, double tolerance)
{
	long k, l, iter = 0; double val, cval = 0, *z;
	
	if ((z = NUMdvector (1, n)) == NULL) return 0;
	
	for (k=1; k <= n; k++)
	{
		for (l=1; l <= n; l++) cval += q[k] * mns[k][l] * q[l];
	}
	if (cval == 0) goto end;
	do
	{
		double znorm2 = 0;
		for (l=1; l <= n; l++)
		{
			z[l] = 0;
			for (k=1; k <= n; k++) z[l] += mns[l][k] * q[k];
		}
		for (k=1; k <= n; k++) znorm2 += z[k] * z[k];
		znorm2 = sqrt (znorm2);
		for (k=1; k <= n; k++)	q[k] = z[k] / znorm2;
		val = cval; cval = 0;
		for (k=1; k <= n; k++)
		{
			for (l=1; l <= n; l++) cval += q[k] * mns[k][l] * q[l];
		}	
	} while (fabs(cval - val) > tolerance || ++iter > 30);
end:
	*lambda = cval;
	NUMdvector_free (z, 1);
	return 1;
}

int NUMprincipalComponents_d (double **a, long n, long nComponents, double **pc)
{
	long i, j, k; int status = 0; double **evec = NULL;
	if (((evec = NUMdmatrix (1, n, 1, n)) == NULL) ||
		! NUMeigensystem_d (a, n, evec, NULL)) goto end;
    for (i=1; i <= n; i++) for (j=1; j<= nComponents; j++)
    {
    	double s = 0;
		for (k=1; k <= n; k++)
		{
			s += a[k][i] * evec[k][j]; /* times sqrt(eigenvalue) ?? */
		}
		pc[i][j] = s;
	}
    status = 1;
end:
	NUMdmatrix_free (evec, 1, 1);
	return status;
}

int NUMdmatrix_into_principalComponents (double **m, long nrows, long ncols,
	long numberOfComponents, double **pc)
{
	SVD svd = NULL; long i, j, k; double **mc = NULL;
	
	Melder_assert (numberOfComponents > 0 && numberOfComponents <= ncols);
	
	if ((mc = NUMdmatrix_copy (m, 1, nrows, 1, ncols)) == NULL) return 0;
	
	/*NUMcentreColumns_d (mc, nrows, ncols);*/
	if ((svd = SVD_create_d (mc, nrows, ncols)) == NULL) goto end;
	for (i = 1; i <= nrows; i++)
	{
		for (j = 1; j <= numberOfComponents; j++)
		{
			pc[i][j] = 0;
			for (k = 1; k <= ncols; k++)
			{
				pc[i][j] += svd -> v[k][j] * m[i][k]; 
			}
		}
	}
	
end:
	NUMdmatrix_free (mc, 1, 1);
	forget (svd);
	return ! Melder_hasError ();
}

int NUMpseudoInverse_d (double **y, long nr, long nc, double **yinv,
	double tolerance)
{
	SVD me; long i, j, k;
	
	me = SVD_create_d (y, nr, nc);
	if (me == NULL) return 0;
	
	(void) SVD_zeroSmallSingularValues (me, tolerance);
	for (i = 1; i <= nc; i++)
	{
		for (j = 1; j <= nr; j++)
		{
			double s = 0;
			for (k = 1; k <= nc; k++)
			{
				if (my d[k]) s += my v[i][k] * my u[j][k] / my d[k];
			}
			yinv[i][j] = s;
		}
	}
	forget (me);
	return 1;
}

int NUMsolveEquation_d (double **a, long nr, long nc, double *b,
	double tolerance, double *result)
{
	SVD me;
	int status = 0; 
	double tol = tolerance > 0 ? tolerance : NUMfpp -> eps * nr;
	
	if (nr <= 0 || nc <= 0) return 0;
	
	me = SVD_create_d (a, nr, nc);
	if (me == NULL) return 0;
	
	(void) SVD_zeroSmallSingularValues (me, tol);
		
	if (SVD_solve (me, b, result)) status = 1;
	
	forget (me);
	
	return status;
}


int NUMsolveEquations_d (double **a, long nr, long nc, double **b,
	long ncb, double tolerance, double **x)
{
	SVD me;
	double *bt = NULL, *xt;
	double tol = tolerance > 0 ? tolerance : NUMfpp -> eps * nr;
	int status = 0; long k, j; 
	
	if (nr <= 0 || nc <= 0) return 0;
	
	me = SVD_create_d (a, nr, nc);
	if (me == NULL) return 0;
	
	bt = NUMdvector (1, nr + nc);
	if (bt == NULL) goto end;
	xt = & bt[nr];
	
	(void) SVD_zeroSmallSingularValues (me, tol);
	
	for (k = 1; k <= ncb; k++)
	{
		for (j = 1; j <= nr; j++)
		{
			bt[j] = b[j][k];
		}
		
		if (! SVD_solve (me, bt, xt)) goto end;
		
		for (j = 1; j <= nc; j++)
		{
			x[j][k] = xt[j];
		}
	}
	status = 1;
end:
	NUMdvector_free (bt, 1);
	forget (me);
	return status;

}


void NUMsolveNonNegativeLeastSquaresRegression (double **m, long nr, long nc,
	double *d, double tol, long itermax, double *b)
{
	long i, j, l, iter; double difsq, difsqp = 0;
	
	for (iter=1; iter <= itermax; iter++)
	{
		/*
			Fix all weights except b[j]
		*/
		
		for (j=1; j <= nc; j++)
		{
			double mjr = 0, mjmj = 0;
			for (i=1; i <= nr; i++)
			{
				double ri = d[i], mij = m[i][j];
				for (l=1; l <= nc; l++)
				{
					if (l != j) ri -= b[l] * m[i][l];
				}
				mjr += mij * ri;
				mjmj += mij * mij;
			}
			b[j] = mjr / mjmj;
			if (b[j] < 0) b[j] = 0;
		}
		
		/*
			Calculate t(b) and compare with previous result.
		*/
		
		difsq = 0;
		for (i=1; i <= nr; i++)
		{
			double dmb = d[i];
			for (j=1; j <= nc; j++)
			{
				dmb -= m[i][j] * b[j];
			}
			difsq += dmb * dmb;
		}
		if (fabs (difsq - difsqp) / difsq < tol) break;
		difsqp = difsq;
	}
}

struct nr_struct { double *y, *delta; };

/*
	f (lambda) = sum (y[i]^2 delta[i] / (delta[i]-lambda)^2, i=1..3)
	f'(lambda) = 2 * sum (y[i]^2 delta[i] / (delta[i]-lambda)^3, i=1..3)
*/

static void nr_func (double x, double *f, double *df, void *data)
{
	struct nr_struct *me = (struct nr_struct *) data; long i;
	*f = *df = 0;
	for (i = 1; i <= 3; i++)
	{
		double t1 = (my delta[i] - x);
		double t2 = my y[i] / t1;
		double t3 = t2 * t2 * my delta[i];
		*f  += t3;
		*df += t3 * 2 / t1;
	} 	
}

int NUMsolveConstrainedLSQuadraticRegression (double **o, const double d[],
	long n, double *alpha, double *gamma)
{
	int status = 0; long i, j, k, l, n3 = 3, info;  char uplo = 'U';
	double **b = NULL, **ftinvp = NULL, **ftinv = NULL, **g = NULL, eps = 1e-5;
	double **p = NULL, *delta = NULL, *y = NULL, *diag = NULL;
	double t1, t2, t3, *chi = NULL, *otd = NULL, **ptfinv = NULL;
	double **ptfinvc = NULL, *w = NULL;
	if (((ftinv = NUMdmatrix (1, n3, 1, n3)) == NULL) ||
		((b = NUMdmatrix (1, n3, 1, n3)) == NULL) ||
		((g = NUMdmatrix (1, n3, 1, n3)) == NULL) ||
		((p = NUMdmatrix (1, n3, 1, n3)) == NULL) ||
		((delta = NUMdvector (1, n3)) == NULL) ||
		((ftinvp = NUMdmatrix (1, n3, 1, n3)) == NULL) ||
		((ptfinv = NUMdmatrix (1, n3, 1, n3)) == NULL) ||
		((otd = NUMdvector (1, n3)) == NULL) || 
		((ptfinvc = NUMdmatrix (1, n3, 1, n3)) == NULL) ||
		((y = NUMdvector (1, n3)) == NULL) ||
		((w = NUMdvector (1, n3)) == NULL) ||
		((chi = NUMdvector (1, n3)) == NULL) ||
		((diag = NUMdvector (1, n3)) == NULL)) goto end;

	/*
		Construct O'.O     [1..3][1..3].
	*/

	for (i=1; i <= n3; i++)
	{
		for (j=1; j <= n3; j++)
		{
			for (k=1; k <= n; k++)
			{
				ftinv[i][j] += o[k][i] * o[k][j];
			}
		}
	}
	
	/*
		Get lower triangular decomposition from O'.O and
		get F'^-1 from it (eq. (2)) (F^-1 not done ????)
	*/
	
	(void) NUMlapack_dpotf2 (&uplo, &n3, &ftinv[1][1], &n3, &info);
	if (info != 0) goto end;
	ftinv[1][2] = ftinv[1][3] = ftinv[2][3] = 0;
	
/*	
	if (! NUMcholeskyDecomposition (ftinv, n3, diag)) goto end;
	
	for (i=1; i <= 3; i++)
	{
		ftinv[i][i] = diag[i];
		for (j=i+1; j <= 3; j++)
		{
			ftinv[i][j] = 0;
		}
	}
*/	
	/*
		Construct G and its eigen-decomposition (eq. (4,5))
		Sort eigenvalues (& eigenvectors) ascending.
	*/
	
	b[3][1] = b[1][3] = -0.5; b[2][2] = 1;
	
	/*
		G = F^-1 B (F')^-1 (eq. 4)
	*/
	
	for (i=1; i <= 3; i++)
	{
		for (j=1; j <= 3; j++)
		{
			for (k=1; k <= 3; k++)
			{
				if (ftinv[k][i] != 0)
				{ 
					for (l=1; l <= 3; l++)
					{
						g[i][j] += ftinv[k][i] * b[k][l] * ftinv[l][j];
					}
				}
			}
		}
	}
	
	/*
		G's eigen-decomposition with eigenvalues (assumed ascending). (eq. 5)
	*/
	
	if (! NUMeigensystem_d (g, 3, p, delta)) goto end;
	
	NUMsort_d(3, delta); /* ascending */
	
	/*
		Construct y = P'.F'.O'.d ==> Solve (F')^-1 . P .y = (O'.d)    (page 632)
		Get P'F^-1 from the transpose of (F')^-1 . P
	*/
	
	for (i = 1; i <= 3; i++)
	{
		for (j = 1; j <= 3; j++)
		{
			if (ftinv[i][j] != 0)
			{
				for (k = 1; k <= 3; k++)
				{
					ftinvp[i][k] += ftinv[i][j] * p[3+1-j][k]; /* is sorted desc. */
				}
			}
		}
		for (k = 1; k <= n; k++)
		{
			otd[i] += o[k][i] * d[k];
		}
	}
	
	for (i=1; i <= 3; i++)
	{
		for (j=1; j <= 3; j++)
		{
			ptfinvc[j][i] = ptfinv[j][i] = ftinvp[i][j];
		}
	}
	
	if (! NUMsolveEquation_d (ftinvp, 3, 3, otd, 1e-6, y)) goto end;
	
	/*
		The solution (3 cases) 
	*/
	
	if (fabs (y[1]) < eps)
	{
		/*
			Case 1: page 633
		*/
		
		t2 = y[2] / (delta[2] - delta[1]);
		t3 = y[3] / (delta[3] - delta[1]);
		/* +- */
		w[1] = sqrt (- delta[1] * (t2 * t2 * delta[2] + t3 * t3 * delta[3]));
		w[2] = t2 * delta[2];
		w[3] = t3 * delta[3];
		
		if (! NUMsolveEquation_d (ptfinv, 3, 3, w, 1e-6, chi)) goto end;
		
		w[1] = -w[1]; 
		if (fabs (chi[3] / chi[1]) < eps &&
			! NUMsolveEquation_d (ptfinvc, 3, 3, w, 1e-6, chi)) goto end;
	}
	else if (fabs (y[2]) < eps)
	{
		/*
			Case 2: page 633
		*/
		
		t1 = y[1] / (delta[1] - delta[2]);
		t3 = y[3] / (delta[3] - delta[2]);
		w[1] = t1 * delta[1];
		if((delta[2] < delta[3] &&
			(t2 = (t1 * t1 * delta[1] + t3 * t3 * delta[3])) < eps))
		{
			w[2] = sqrt (- delta[2] * t2); /* +- */
			w[3] = t3 * delta[3];
			if (! NUMsolveEquation_d (ptfinv, 3, 3, w, 1e-6, chi)) goto end;
			w[2] = -w[2]; 
			if (fabs (chi[3] / chi[1]) < eps &&
				! NUMsolveEquation_d (ptfinvc, 3, 3, w, 1e-6, chi)) goto end;
		}
		else if (((delta[2] < delta[3] + eps) || (delta[2] > delta[3] - eps))
			&& fabs (y[3]) < eps)
		{
			/*
				choose one value for w[2] from an infinite number
			*/
			
			w[2] = w[1];
			w[3] = sqrt (- t1 * t1 * delta[1] * delta[2] - w[2] * w[2]);
			if (! NUMsolveEquation_d (ptfinv, 3, 3, w, 1e-6, chi)) goto end;
		}
	}
	else 
	{
		/*
			Case 3: page 634 use Newton-Raphson root finder
		*/
		
		struct nr_struct me; 
		double xlambda, eps = (delta[2] - delta[1]) * 1e-6;
		
		me.y = y; me.delta = delta;
		
		if (! NUMnrbis (nr_func, delta[1] + eps, delta[2] - eps, & me,
			& xlambda)) goto end;
		
		for (i = 1; i <= 3; i++)
		{
			w[i] = y[i] / (1 - xlambda / delta[i]);
		}
		if (! NUMsolveEquation_d (ptfinv, 3, 3, w, 1e-6, chi)) goto end;
	}
	
	*alpha = chi[1]; *gamma = chi[3];
	
end:
	NUMdmatrix_free (ftinv, 1, 1);
	NUMdmatrix_free (b, 1, 1);
	NUMdmatrix_free (g, 1, 1);
	NUMdmatrix_free (p, 1, 1);
	NUMdvector_free (delta, 1);
	NUMdmatrix_free (ftinvp, 1, 1);
	NUMdmatrix_free (ptfinv, 1, 1);
	NUMdvector_free (otd, 1);
	NUMdmatrix_free (ptfinvc, 1, 1);
	NUMdvector_free (y, 1);
	NUMdvector_free (w, 1);
	NUMdvector_free (chi, 1);
	NUMdvector_free (diag, 1);
	return status;
}

/*
	f (b) = delta - b / (2 alpha) - sum (x[i]^2 / (c[i] - b)^2, i=1..n)
	f'(b) = - 1 / (2 alpha) + 2 * sum (x[i]^2 / (c[i] - b)^3, i=1..n)
*/
struct nr2_struct { long m; double delta, alpha, *x, *c; };

static void nr2_func (double b, double *f, double *df, void *data)
{
	struct nr2_struct *me = (struct nr2_struct *) data; 
	long i;
	
	*df = - 0.5 / my alpha;
	*f = my delta + *df * b;
	for (i=1; i <= my m; i++)
	{
		double c1 = (my c[i] - b);
		double c2 = my x[i] / c1;
		double c2sq = c2 * c2;
		*f -= c2sq; *df += 2 * c2sq / c1;
	}
}

int NUMsolveWeaklyConstrainedLinearRegression (double **f, long n, long m,
	double phi[], double alpha, double delta, double t[])
{
	SVD svd;
	struct nr2_struct me;
	long i, j, k, q = 1, r = m, *indx = NULL;
	double **ut, **u = NULL;
	double *sqrtc, *c = NULL, *x = NULL;
	double xqsq = 0, tol = 1e-6,  xCx = 0, bmin, b0, eps;
	int status = 0;
	
	if (((u = NUMdmatrix (1, m, 1, m)) == NULL) ||
		((c = NUMdvector (1, m)) == NULL) ||
		((x = NUMdvector (1, n)) == NULL) ||
		((indx = NUMlvector (1, m)) == NULL)) goto end;
		
	for (j=1; j <= m; j++) t[j] = 0;
	
	svd = SVD_create_d (f, n, m);
	if (svd == NULL) goto end;
	
	if (alpha == 0) /* standard least squares */
	{
		if (SVD_solve (svd, phi, t)) status = 1;
		goto end;
	}
	
	/*
		Step 1: Compute U and C from the eigendecomposition F'F = UCU'
		Evaluate q, the multiplicity of the smallest eigenvalue in C
	*/
	
	sqrtc = svd -> d; ut = svd -> v;
		
	NUMindexx_d (sqrtc, m, indx);
	
	for (j=m; j > 0; j--)
	{
		double tmp = sqrtc[ indx[j] ];
		c[m-j+1] = tmp * tmp;
		for (k=1; k <= m; k++)
		{
			u[m-j+1][k] = ut[ indx[j] ][k];
		}
	}
	
	q = 1; while (q < m && (c[m-q] - c[m]) < tol) q++;
	
	/*
		step 2: x = U'F'phi
	*/
	
	for (i=1; i <= m; i++)
	{
		for (j=1; j <= m; j++)
		{
			for (k=1; k <= n; k++)
			{
				x[i] += u[j][i] * f[k][j] * phi[k];
			}
		}
	}
	
	/*
		step 3:
	*/
	
	me.m = m;
	me.delta = delta;
	me.alpha = alpha;
	me.x = x;
	me.c = c;
	for (j=m-q+1; j <= m; j++)
	{
		xqsq += x[j] * x[j];
	}
	if (xqsq < tol) /* xqsq == 0 */
	{
		double fm, df;
		r = m - q;
		me.m = r;
		nr2_func (c[m], &fm, &df, & me);
		if (fm >= 0) /* step 3.b1 */
		{
			x[r+1] = sqrt (fm);
			for (j=1; j <= r; j++)
			{
				x[j] /= c[j] - c[m];
			} 
			for (j=1; j <= r+1; j++)
			{
				for (k=1; k <= r+1; k++)
				{
					t[j] += u[j][k] * x[k];
				}
			}
			status = 1; goto end;
		}
		/* else continue with r = m - q */
	}
	
	/*
		step 3a & 3b2, determine interval lower bound for Newton-Raphson 
		root finder
	*/
	
	for (j=1; j <= r; j++)
	{
		xCx += x[j] * x[j] / c[j];
	}
	bmin = delta > 0 ? - xCx / delta : -2 * sqrt (alpha * xCx);
	eps = (c[m] - bmin) * tol;
	
	/*
		find the root of d(psi(b)/db in interval (bmin, c[m])
	*/
	if (! NUMnrbis (nr2_func, bmin + eps, c[m] - eps, & me, & b0)) goto end;

	for (j=1; j <= r; j++)
	{
		for (k=1; k <= r; k++)
		{
			t[j] += u[j][k] * x[k] / (c[k] - b0);
		}
	}
	status = 1;
end:
	NUMlvector_free (indx, 1); NUMdvector_free (x, 1);
	NUMdvector_free (c, 1);
	NUMdmatrix_free (u, 1, 1);
	forget (svd);
	return status;
}

int NUMProcrustes (double **x, double **y, long nPoints, 
	long nDimensions, double **t, double *v, double *s)
{
	SVD svd = NULL;
	long i, j, k;
	double **c, **yc = NULL, trace, traceXtJYT, traceYtJY;
	int orthogonal = v == NULL || s == NULL; /* else similarity transform */
		
	c = NUMdmatrix (1, nDimensions, 1, nDimensions);
	if (c == NULL) return 0;
	yc = NUMdmatrix_copy (y, 1, nPoints, 1, nDimensions);
	if (yc == NULL) goto end;
	
	/*
		Reference: Borg & Groenen (1997), Modern multidimensional scaling, 
		Springer
		1. Calculate C = X'JY (page 346) for similarity transform 
			else X'Y for othogonal (page 341)
			JY amounts to centering the columns of Y.
	*/
	
	if (! orthogonal) NUMcentreColumns_d (yc, 1, nPoints, 1, nDimensions, NULL);
	for (i = 1; i <= nDimensions; i++)
	{
		for (j = 1; j <= nDimensions; j++)
		{
			for (k = 1; k <= nPoints; k++)
			{
				c[i][j] += x[k][i] * yc[k][j];
			}
		}
	}
	
	/*
		2. Decompose C by SVD:  C = PDQ' (SVD attribute is Q instead of Q'!)
	*/
	
	if (! (svd = SVD_create_d (c, nDimensions, nDimensions))) goto end;
	
	for (trace = 0, i = 1; i <= nDimensions; i++)
	{
		trace += svd -> d[i];
	}
	
	if (trace == 0)
	{
		(void) Melder_error ("NUMProcrustes: degenerate configuration(s).");
		goto end;
	}
	
	/*
		3. T = QP'
	*/
		
	for (i = 1; i <= nDimensions; i++)
	{
		for (j = 1; j <= nDimensions; j++)
		{
			for (t[i][j] = 0, k = 1; k <= nDimensions; k++)
			{
				t[i][j] += svd -> v[i][k] * svd -> u[j][k];
			}
		}
	}
	
	if (! orthogonal)
	{
		double **xc, **yt = NULL;
		if (! (xc = NUMdmatrix_copy (x, 1, nPoints, 1, nDimensions)) ||
			! (yt = NUMdmatrix (1, nPoints, 1, nDimensions))) goto oend;
	
		/*
			4. Dilation factor s = (tr X'JYT) / (tr Y'JY)
			   First we need YT.
		*/
	
		for (i = 1; i <= nPoints; i++)
		{
			for (j = 1; j <= nDimensions; j++)
			{
				for (k = 1; k <= nDimensions; k++)
				{
					yt[i][j] += y[i][k] * t[k][j];
				}
			}
		}
	
		/*
			X'J amount to centering the columns of X
		*/
	
		NUMcentreColumns_d (xc, 1, nPoints, 1, nDimensions, NULL);
	
		/*
			tr X'J YT == tr xc' yt
		*/
	
		for (traceXtJYT = 0, i = 1; i <= nDimensions; i++)
		{
			for (j = 1; j <= nPoints; j++)
			{
				traceXtJYT += xc[j][i] * yt[j][i];
			}
		}
	
		for (traceYtJY = 0, i = 1; i <= nDimensions; i++)
		{
			for (j = 1; j <= nPoints; j++)
			{
				traceYtJY += y[j][i] * yc[j][i];
			}
		}
	
		*s = traceXtJYT / traceYtJY;
		
		/*
			5. Translation vector tr = (X - sYT)'1 / nPoints
		*/

		for (i = 1; i <= nDimensions; i++)
		{
			for (j = 1; j <= nPoints; j++)
			{
				v[i] += x[j][i] - *s * yt[j][i];
			}
			v[i] /= nPoints;
		}
oend:
		NUMdmatrix_free (xc, 1, 1);
		NUMdmatrix_free (yt, 1, 1);
	}
	
end:

	forget (svd);
	NUMdmatrix_free (yc, 1, 1);
	NUMdmatrix_free (c, 1, 1);
	return ! Melder_hasError ();
}


int NUMmspline (double knot[], long nKnots, long order, long i,
	double x, double *y)
{
	long ito = i + order - 1, j, k, nSplines = nKnots - order;
	double *m; 
		
	*y = 0;
	
	/*
		Find the interval where x is located. 
		M-splines of order k have degree k-1. 
		M-splines are zero outside interval [ knot[i], knot[i+order] ).
		First and last 'order' knots are equal, i.e.,
		knot[1] = ... = knot[order] && knot[nKnots-order+1] = ... knot[nKnots].
	*/
	
	Melder_assert (nSplines > 0);
	if (i > nSplines || order < 1) return 0;
	for (j=order; j <= nKnots-order+1; j++)
	{
		if (x < knot[j]) break;
	}
	if (j < i || j > i + order || j == order ||
		j > nKnots - order + 1) return 1;
	
	if (! (m = NUMdvector (i, ito))) return 0;
	
	/*
		Calculate M[i](x|1,t) according to eq.2.
	*/
	
	for (j=i; j <= ito; j++)
	{
		if (x >= knot[j] && x < knot[j+1]) m[j] = 1 / (knot[j+1] - knot[j]);
	}
	
	/*
		Iterate to get M[i](x|k,t)
	*/
	
	for (k=2; k <= order; k++)
	{
		for (j=i; j <= i + order - k; j++)
		{
			double kj = knot[j], kjpk = knot[j+k];
			if (kjpk > kj)
			{
				m[j] = k * ((x - kj) * m[j] + (kjpk - x) * m[j+1]) /
					((k - 1) * (kjpk - kj));
			}
		}
	}
	
	*y = m[i];
	NUMdvector_free (m, i);
	return 1;
}

int NUMispline (double aknot[], long nKnots, long order, long i,
	double x, double *y)
{
	long j, m, orderp1 = order + 1; double r;
	
	*y = 0;
	
	for (j=orderp1; j <= nKnots-order; j++)
	{
		if (x < aknot[j]) break;
	}
	j--;
	if (j < i) return 1;
	if (j > i + order || (j == nKnots - order && x == aknot[j]))
	{
		*y = 1; return 1;
	}
	/*
		Equation 5 in Ramsay's article contains some errors!!!
		1. the interval selection must be 'j-k <= i <= j' instead of
			'j-k+1 <= i <= j'
		2. the summation index m starts at 'i+1' instead of 'i'
	*/
	for (m=i+1; m <=j; m++)
	{
		if (! NUMmspline (aknot, nKnots, orderp1, m, x, &r)) return 0; 
		*y += (aknot[m+orderp1] - aknot[m]) * r;
	}
	*y /= orderp1;
	return 1;
}

double NUMwilksLambda (double *lambda, long from, long to)
{
	long i; double result = 1;
	for (i=from; i <= to; i++) result /= (1 + lambda[i]);
	return result;
}

double NUMfactln (int n)
{
	static double table[101];
	if (n < 0) return NUMundefined;
	if (n <= 1) return 0;
	return n > 100 ? NUMlnGamma (n + 1.0) : table[n] ? table[n] :
		(table[n] = NUMlnGamma (n + 1.0));
}

int NUMnrbis (void (*f) (double x, double *fx, double *dfx, void *closure), 
	double x1, double x2, void *closure, double *root)
{
	double df, dx, dxold, fx, fh, fl, tmp, xh, xl, tol;
	long iter, itermax = 60;
	 
	(*f) (x1, &fl, &df, closure);
	if (fl == 0.0)
	{
		*root = x1;
		return 1;
	}
	
	(*f) (x2, &fh, &df, closure);
	if (fh == 0.0)
	{
		*root = x2;
		return 1;
	}
	
	if ((fl > 0.0 && fh > 0.0) || (fl < 0.0 && fh < 0.0))
	{
		*root = NUMundefined;
		return Melder_error("NUMnrbis: root must be bracketed.");
	}
	
	if (fl < 0.0)
	{
		xl = x1; xh = x2;
	}
	else
	{
		xh = x1; xl = x2;
	}
	
	dxold = fabs (x2 - x1);
	dx = dxold;
	*root = 0.5 * (x1 + x2);
	(*f) (*root, &fx, &df, closure);
	
	for (iter = 1; iter <= itermax; iter++)
	{
		if ((((*root - xh) * df - fx) * ((*root - xl) * df - fx) >= 0.0) ||
			(fabs (2.0 * fx) > fabs (dxold * df)))
		{
			dxold = dx; 
			dx = 0.5 * (xh - xl); 
			*root = xl + dx;
			if (xl == *root) return 1;
		}
		else
		{
			dxold = dx; 
			dx = fx / df; 
			tmp = *root; 
			*root -= dx;
			if (tmp == *root) return 1;
		}
		tol = NUMfpp -> eps	* (*root == 0 ? 1 : fabs (*root));
		if (fabs(dx) < tol) return 1;
		
		(*f) (*root, &fx, &df, closure);
		
		if (fx < 0.0)
		{
			xl = *root;
		}
		else 
		{
			xh = *root;
		}
	}
	Melder_warning ("NUMnrbis: maximum number of iterations (%d) "
		"exceeded.", itermax);
	return 1;
}

double NUMridders (double (*f) (double x, void *closure), double x1, double x2, void *closure)
{
	/* There is still a problem with this implementation:
		tol may be zero;
	*/
	double x3, x4, d, root = NUMundefined;
	double f1, f2, f3, f4, tol;
	long iter, itermax = 100;
		
	f1 = f (x1, closure);
	if (f1 == 0.0) return x1;
	if (f1 == NUMundefined) return NUMundefined;
	f2 = f (x2, closure);
	if (f2 == 0.0) return x2;
	if (f2 == NUMundefined) return NUMundefined;
	if ((f1 < 0 && f2 < 0) || (f1 > 0 && f2 > 0))
	{
		Melder_warning ("NUMridders: root must be bracketed.");
		return NUMundefined;
	}
	
	for (iter = 1; iter <= itermax; iter++)
	{
		x3 = 0.5 * (x1 + x2);   
		f3 = f (x3, closure);
		if (f3 == 0.0) return x3;
		if (f3 == NUMundefined) return NUMundefined;
		
		/*
			New guess:
			x4 = x3 + (x3 - x1) * sign(f1 - f2) * f3 / sqrt(f3^2 - f1*f2)
		*/
		
		d = f3 * f3 - f1 * f2;
		if (d < 0.0)
		{
			Melder_warning ("d < 0 in ridders; x1=%.17g, x2=%.17g, x3=%.17g, f1=%.17g, f2=%.17g, f3=%.17g, iter=%ld",
				x1, x2, x3, f1, f2, f3, iter);
			return NUMundefined;
		}

		if (d == 0.0)
		{   /* pb test added because f1 f2 f3 may be 1e-170 or so */
			tol = NUMfpp -> eps * fabs (x3);	
			if (iter > 1 && fabs (x3 - root) < tol) return root;
			root = x3;
			/*
			 * Perform bisection.
			 */
			if (f1 > 0.0)
			{   /* falling curve: f1 > 0, f2 < 0 */
				if (f3 > 0.0)
				{
					x1 = x3; f1 = f3;   /* retain invariant: f1 > 0, f2 < 0 */
				}
				else
				{   /* f3 < 0.0 */
					x2 = x3; f2 = f3;   /* retain invariant: f1 > 0, f2 < 0 */
				}
			}
			else
			{   /* rising curve: f1 < 0, f2 > 0 */
				if (f3 > 0.0)
				{
					x2 = x3; f2 = f3;   /* retain invariant: f1 < 0, f2 > 0 */
				}
				else
				{   /* f3 < 0.0 */
					x1 = x3; f1 = f3;   /* retain invariant: f1 < 0, f2 > 0 */
				}
			}
		}
		else
		{
			d = sqrt (d);
			if (isnan (d))
			{   /* pb: square root of denormalized small number fails on some computers */
				tol = NUMfpp -> eps * fabs (x3);	
				if (iter > 1 && fabs (x3 - root) < tol) return root;
				root = x3;
				/*
				 * Perform bisection.
				 */
				if (f1 > 0.0)
				{   /* falling curve: f1 > 0, f2 < 0 */
					if (f3 > 0.0)
					{
						x1 = x3; f1 = f3;   /* retain invariant: f1 > 0, f2 < 0 */
					}
					else
					{   /* f3 < 0.0 */
						x2 = x3; f2 = f3;   /* retain invariant: f1 > 0, f2 < 0 */
					}
				}
				else
				{   /* rising curve: f1 < 0, f2 > 0 */
					if (f3 > 0.0)
					{
						x2 = x3; f2 = f3;   /* retain invariant: f1 < 0, f2 > 0 */
					}
					else
					{   /* f3 < 0.0 */
						x1 = x3; f1 = f3;   /* retain invariant: f1 < 0, f2 > 0 */
					}
				}
			}
			else
			{
				d = (x3 - x1) * f3 / d;
				x4 = f1 - f2 < 0 ? x3 - d : x3 + d;
				tol = NUMfpp -> eps * fabs (x4);	
				if (iter > 1 && fabs (x4 - root) < tol) return root;
				root = x4;
				f4 = f (x4, closure);
				if (f4 == 0.0) return root;
				if (f4 == NUMundefined) return NUMundefined;
				if ((f1 > f2) == (d > 0.0) /* pb: instead of x3 < x4 */)
				{
					if (SIGN (f3, f4) != f3)
					{
						x1 = x3; f1 = f3;
						x2 = x4; f2 = f4;
					}
					else
					{
						x1 = x4; f1 = f4;
					}
				}
				else
				{
					if (SIGN (f3, f4) != f3)
					{
						x1 = x4; f1 = f4;
						x2 = x3; f2 = f3;
					}
					else
					{
						x2 = x4; f2 = f4;
					}	       
				}
			}
		}	       
		if (fabs (x1 - x2) < tol) return root;
	}

	{
		static long nwarnings = 0;
		nwarnings++;
		Melder_warning ("NUMridders: maximum number of iterations (%ld) "
		"exceeded.\n%ld root=%.17g x1=%.17g x2=%.17g x3=%.17g x4=%.17g\n"
		"f1=%.17g f2=%.17g f3=%.17g f4=%.17g d=%.17g", itermax, nwarnings,
		root, x1, x2, x3, x4, f1, f2, f3, f4, d);
	}
	return root;
}

double NUMstudentP (double t, long df)
{
	double ib;
	if (df < 1) return NUMundefined;
	ib = NUMincompleteBeta (0.5 * df, 0.5, df / (df + t * t));
	if (ib == NUMundefined) return NUMundefined;
	ib *= 0.5;
	return t < 0 ? ib : 1 - ib;
}

double NUMstudentQ (double t, long df)
{
	double ib;
	if (df < 1) return NUMundefined;
	ib = NUMincompleteBeta (0.5 * df, 0.5, df / (df + t * t));
	if (ib == NUMundefined) return NUMundefined;
	ib *= 0.5;
	return t > 0 ? ib : 1 - ib;
}

double NUMfisherP (double f, long df1, long df2)
{
	double ib;
	if (f < 0 || df1 < 1 || df2 < 1) return NUMundefined;
	ib = NUMincompleteBeta (0.5 * df2, 0.5 * df1, df2 / (df2 + f * df1));
	if (ib == NUMundefined) return NUMundefined;
	return 1 - ib;
}

double NUMfisherQ (double f, long df1, long df2)
{
	if (f < 0 || df1 < 1 || df2 < 1) return NUMundefined;
	return NUMincompleteBeta (0.5 * df2, 0.5 * df1, df2 / (df2 + f * df1));
}

double NUMinvGaussQ (double p)
{
	double t, pc = p;
	if (p <= 0 || p >= 1.0) return NUMundefined;
	if (p > 0.5) pc = 1 - p;
	t = sqrt (- 2.0 * log (pc));
	t -= (2.515517 + (0.802853 + 0.010328 * t) * t) /
		(1.0 + (1.432788 + (0.189269 + 0.001308 * t) * t) * t);
	return p > 0.5 ? -t : t;
}

static double studentQ_func (double x, void *voidParams)
{
	struct pdf1_struct *params = (struct pdf1_struct *) voidParams;
	double q = NUMstudentQ (x, params -> df);
	return q == NUMundefined ? NUMundefined : q - params -> p;
}

double NUMinvStudentQ (double p, long df)
{
	struct pdf1_struct params;      
	double pc = p > 0.5 ? 1 - p : p, xmin, xmax = 1, x;

	if (p < 0 || p >= 1) return NUMundefined;

	/*
		Bracket the function f(x) = NUMstudentQ (x, df) - p.
	*/
	for (;;)
	{
		double q = NUMstudentQ (xmax, df);
		if (q == NUMundefined) return NUMundefined;
		if (q < pc) break;
		xmax *= 2;
	}

	xmin = xmax > 1 ? xmax / 2 : 0;

	/*
		Find zero of f(x) with Ridders' method.
	*/
	params. df = df;
	params. p = pc;
	x = NUMridders (studentQ_func, xmin, xmax, & params);
	if (x == NUMundefined) return NUMundefined;

	return p > 0.5 ? -x : x;
}
 
static double chiSquareQ_func (double x, void *voidParams)
{
	struct pdf1_struct *params = (struct pdf1_struct *) voidParams;
	double q = NUMchiSquareQ (x, params -> df);
	return q == NUMundefined ? NUMundefined : q - params -> p;
}

double NUMinvChiSquareQ (double p, long df)
{
	struct pdf1_struct params;      
	double xmin, xmax = 1;
        
	if (p < 0 || p >= 1) return NUMundefined;
        
	/*
		Bracket the function f(x) = NUMchiSquareQ (x, df) - p.
	*/
	for (;;)
	{
		double q = NUMchiSquareQ (xmax, df);
		if (q == NUMundefined) return NUMundefined;
		if (q < p) break;
		xmax *= 2;
	}
	xmin = xmax > 1 ? xmax / 2 : 0;
        
	/*
		Find zero of f(x) with Ridders' method.
	*/
	params. df = df;
	params. p = p;
	return NUMridders (chiSquareQ_func, xmin, xmax, & params);
}

static double fisherQ_func (double x, void *voidParams)
{
	struct pdf2_struct *params = (struct pdf2_struct *) voidParams;
	double q = NUMfisherQ (x, params -> df1, params -> df2);
	return q == NUMundefined ? NUMundefined : q - params -> p;
}

double NUMinvFisherQ (double p, long df1, long df2)
{
	struct pdf2_struct params;
	double top = 1000.0;
	if (p <= 0.0 || p > 1.0 || df1 < 1 || df2 < 1) return NUMundefined;
	if (p == 1.0) return 0.0;
	params. p = p;
	params. df1 = df1;
	params. df2 = df2;
	for (;;)
	{
		double q = NUMfisherQ (top, df1, df2);
		if (q == NUMundefined) return NUMundefined;
		if (q < p) break;
		if (top > 0.9e300) return NUMundefined;
		top *= 1e9;
	}
	return NUMridders (fisherQ_func, 0.0, p > 0.5 ? 2.2 : top, & params);
}


/*************** Hz <--> other freq reps *********************/

double NUMmelToHertz3 (double mel)
{
	if (mel < 0) return NUMundefined;
	return mel < 1000 ? mel : 1000 * (exp (mel * log10(2) / 1000) - 1);
}

double NUMhertzToMel3 (double hz)
{
	if (hz < 0) return NUMundefined;
	return hz < 1000 ? hz : 1000 * log10 (1 + hz / 1000) / log10 (2);
}

double NUMmelToHertz2 (double mel)
{
	if (mel < 0) return NUMundefined;
	return 700 * (pow (10.0, mel / 2595.0) - 1);
}

double NUMhertzToMel2 (double hz)
{
	if (hz < 0) return NUMundefined;
	return 2595 * log10 (1 + hz/700);
}

double NUMhertzToBark_traunmueller (double hz)
{
	if (hz < 0) return NUMundefined;
	return 26.81 * hz /(1960 + hz) - 0.53;
}

double NUMbarkToHertz_traunmueller (double bark)
{
	if (bark < 0 || bark > 26.28) return NUMundefined; 
	return 1960* (bark + 0.53) / (26.28 - bark);
}

double NUMbarkToHertz_schroeder (double bark)
{
	return 650.0 * sinh (bark / 7.0);
}

double NUMbarkToHertz_zwickerterhardt (double hz)
{
	if (hz < 0) return NUMundefined;
	return 13 * atan (0.00076 * hz) + 3.5 * atan (hz / 7500);
}

double NUMhertzToBark_schroeder (double hz)
{
	double h650;
	if (hz < 0) return NUMundefined;
	h650 = hz / 650;
	return 7.0 * log (h650 + sqrt (1 + h650 * h650));
}

double NUMbarkToHertz2 (double bark)
{
	if (bark < 0) return NUMundefined; 
	return 650.0 * sinh (bark / 7.0);
}

double NUMhertzToBark2 (double hz)
{
	double h650;
	if (hz < 0) return NUMundefined;
	h650 = hz / 650;
	return 7.0 * log (h650 + sqrt (1 + h650 * h650));
}

double NUMbladonlindblomfilter_amplitude (double zc, double z)
{
	double dz = zc - z + 0.474;
	return pow (10, 1.581 + 0.75 * dz - 1.75 * sqrt (1 + dz * dz));
}

double NUMsekeyhansonfilter_amplitude (double zc, double z)
{
	double dz = zc - z - 0.215;
	return pow (10, 0.7 - 0.75 * dz - 1.75 * sqrt (0.196 + dz * dz));
}

double NUMtriangularfilter_amplitude (double fl, double fc, double fh, double f)
{
	double a = 0;
	if (f > fl && f < fh)
	{
		a = f < fc ? (f - fl) / (fc - fl) : (fh - f) / (fh - fc);
		
		/* Normalize such that area under the filter is always 1. ???
		
		a /= 2 * (fh - fl);*/
	}
	return a;
}

double NUMformantfilter_amplitude (double fc, double bw, double f)
{
	double dq = (fc * fc - f * f) / (bw * f);
	return 1.0 / (dq * dq + 1.0);
}

/* Childers (1978), Modern Spectrum analysis, IEEE Press, 252-255) */
/* work[1..n+n+n];
b1 = & work[1];
b2 = & work[n+1];
aa = & work[n+n+1];
for (i=1; i<=n+n+n; i++) work[i]=0;
*/
int NUMburg (float x[], long n, float a[], int m, float *xms)
{
	long i = 1, j; int status = 0;
	float p = 0.0;
	float *b1 , *b2 = NULL, *aa = NULL;
	
	if (((b1 = NUMfvector (1, n)) == NULL) ||
		((b2 = NUMfvector (1, n)) == NULL) ||
		((aa = NUMfvector (1, m)) == NULL)) goto end;
	
	/* (3) */
		
	for (j = 1; j <= n; j++)
	{
		p += x[j] * x[j];
	}
	
	*xms = p / n;
	if (*xms <= 0) goto end;
	
	/* (9) */
	
	b1[1] = x[1];
	b2[n - 1] = x[n];
	for (j = 2; j <= n - 1; j++)
	{
		b1[j] = b2[j - 1] = x[j];
	}
	
	for (i = 1; i <= m; i++)
	{
		/* (7) */
		
		float num = 0.0, denum = 0.0;
		for (j = 1; j <= n - i; j++)
		{
			num += b1[j] * b2[j];
			denum += b1[j] * b1[j] + b2[j] * b2[j];
		}
		
		if (denum <= 0) goto end;
		
		a[i] = 2.0 * num / denum;
		
		/* (10) */
		
		*xms *= 1.0 - a[i] * a[i];
		
		/* (5) */
		
		for (j = 1; j <= i - 1; j++)
		{
			a[j] = aa[j] - a[i] * aa[i - j];
		}
		
		if (i < m)
		{
		
			/* (8) */
			/* Watch out: i -> i+1 */
		
			for (j = 1; j <= i; j++)
			{
				aa[j] = a[j];
			}
			for (j = 1; j <= n - i - 1; j++)
			{
				b1[j] -= aa[i] * b2[j]; 
				b2[j] = b2[j + 1] - aa[i] * b1[j + 1];
			}
		}
	}
	
	status = 1;
	
end:	
	NUMfvector_free (aa, 1);
	NUMfvector_free (b2, 1);
	NUMfvector_free (b1, 1);
	for (j = i; j <= m; j++) a[j] = 0;
	return status;
}

static int NUMfmatrix_to_dBs2 (float **m, long rb, long re, long cb, long ce, 
	double ref, double factor, double dynamic_range_db)
{
	double ref_db, min2, max_db, min_db, factor10 = factor * 10;
	float max = m[rb][cb], min = max;
	long i, j;
	
	Melder_assert (ref > 0 && factor > 0 && rb <= re && cb <= ce);
	
	for (i=rb; i <= re; i++)
	{
		for (j=cb; j <= ce; j++)
		{
			if (m[i][j] > max) max = m[i][j];
			else if (m[i][j] < min) min = m[i][j];
		}
	}
	
	if (max < 0 || min < 0) return Melder_error ("NUMdmatrix_to_dBs: all "
		"matrix elements must be positive.");
	
	ref_db = factor10 * log10 (ref);
	max_db = factor10 * log10 (max) - ref_db;
	min_db = max_db - fabs (dynamic_range_db);
	min2 = ref * pow (10, min_db / factor10);
	
	if (min2 < min)
	{
		min2 = min; min_db = factor10 * log10 (min2) - ref_db;
	} 
	
	for (i=rb; i <= re; i++)
	{
		for (j=cb; j <= ce; j++)
		{
			double mij = m[i][j];
			if (mij <= min2) mij = min_db;
			else mij = factor10 * log10 (mij) - ref_db;
			m[i][j] = mij;
		}
	}	
	return 1;
}

int NUMfmatrix_to_dBs (float **m, long rb, long re, long cb, long ce, 
	double ref, double factor, double floor)
{
	double ref_db, factor10 = factor * 10;
	float max = m[rb][cb], min = max;
	long i, j;
	
	Melder_assert (ref > 0 && factor > 0 && rb <= re && cb <= ce);
	
	for (i=rb; i <= re; i++)
	{
		for (j=cb; j <= ce; j++)
		{
			if (m[i][j] > max) max = m[i][j];
			else if (m[i][j] < min) min = m[i][j];
		}
	}
	
	if (max < 0 || min < 0) return Melder_error ("NUMdmatrix_to_dBs: all "
		"matrix elements must be positive.");

	ref_db = factor10 * log10 (ref);
		
	for (i=rb; i <= re; i++)
	{
		for (j=cb; j <= ce; j++)
		{
			double mij = floor;
			if (m[i][j] > 0)
			{
				mij = factor10 * log10 (m[i][j]) - ref_db;
				if (mij < floor) mij = floor;
			}
			m[i][j] = mij;
		}
	}	
	return 1;
}

double **NUMcosinesTable_d (long first, long last, long npoints)
{
	double **m;
	long i, j;
	
	Melder_assert (0 < first && first <= last && npoints > 1);
	
	if ((m = NUMdmatrix (first, last, 1, npoints)) == NULL) return NULL;
	
	for (i = first; i <= last; i++)
	{
		double f = i * NUMpi / npoints;
		for (j = 1; j <= npoints; j++)
		{
			m[i][j] = cos (f * (j - 0.5));
		}
	}
	return m;
}

int NUMspline_f (float x[], float y[], long n, float yp1, float ypn, float y2[])
{
	float p, qn, sig, un, *u;
	long i,k;

	if ((u = NUMfvector (1, n - 1)) == NULL) return 0;
	
	if (yp1 > 0.99e30)
	{
		y2[1] = u[1] = 0.0;
	}
	else
	{
		y2[1] = -0.5;
		u[1] = (3.0 / (x[2] - x[1])) * ((y[2] - y[1]) / (x[2] - x[1]) - yp1);
	}
	
	for (i=2; i <= n-1; i++)
	{
		sig = (x[i] - x[i-1]) / (x[i+1] - x[i-1]);
		p = sig * y2[i-1] + 2.0;
		y2[i] = (sig - 1.0) / p;
		u[i] = (y[i+1] - y[i]) / (x[i+1] - x[i]) - (y[i] - y[i-1]) /
			(x[i] - x[i-1]);
		u[i] = (6.0 * u[i] / (x[i+1] - x[i-1]) - sig * u[i-1]) / p;
	}
	
	if (ypn > 0.99e30)
	{
		qn = un = 0.0;
	}
	else
	{
		qn = 0.5;
		un = (3.0 / (x[n] - x[n-1])) * (ypn - (y[n] - y[n-1]) /
			(x[n] - x[n-1]));
	}
	
	y2[n] = (un - qn * u[n-1]) / (qn * y2[n-1] + 1.0);
	for (k=n-1; k >= 1; k--)
	{
		y2[k] = y2[k] * y2[k+1] + u[k];
	}
	
	NUMfvector_free (u, 1);
	
	return 1;
}

int NUMspline_d (double x[], double y[], long n, double yp1, double ypn,
	double y2[])
{
	double p, qn, sig, un, *u;
	long i,k;

	if ((u = NUMdvector (1, n - 1)) == NULL) return 0;
	
	if (yp1 > 0.99e30)
	{
		y2[1] = u[1] = 0.0;
	}
	else
	{
		y2[1] = -0.5;
		u[1] = (3.0 / (x[2] - x[1])) * ((y[2] - y[1]) / (x[2] - x[1]) - yp1);
	}
	
	for (i=2; i <= n-1; i++)
	{
		sig = (x[i] - x[i-1]) / (x[i+1] - x[i-1]);
		p = sig * y2[i-1] + 2.0;
		y2[i] = (sig - 1.0) / p;
		u[i] = (y[i+1] - y[i]) / (x[i+1] - x[i]) - (y[i] - y[i-1]) /
			(x[i] - x[i-1]);
		u[i] = (6.0 * u[i] / (x[i+1] - x[i-1]) - sig * u[i-1]) / p;
	}
	
	if (ypn > 0.99e30)
	{
		qn = un = 0.0;
	}
	else
	{
		qn = 0.5;
		un = (3.0 / (x[n] - x[n-1])) * (ypn - (y[n] - y[n-1]) /
			(x[n] - x[n-1]));
	}
	
	y2[n] = (un - qn * u[n-1]) / (qn * y2[n-1] + 1.0);
	for (k=n-1; k >= 1; k--)
	{
		y2[k] = y2[k] * y2[k+1] + u[k];
	}
	
	NUMdvector_free (u, 1);
	
	return 1;
}

int NUMsplint_f (float xa[], float ya[], float y2a[], long n, float x, float *y)
{
	long klo, khi, k;
	float h, b, a;

	klo = 1; khi = n;
	while (khi-klo > 1)
	{
		k = (khi + klo) >> 1;
		if (xa[k] > x) khi = k;
		else klo = k;
	}
	h = xa[khi] - xa[klo];
	if (h == 0.0) return Melder_error ("NUMsplint_f: bad input value");
	a = (xa[khi] - x) / h;
	b = (x - xa[klo]) / h;
	*y = a * ya[klo] + b * ya[khi]+((a * a * a - a) * y2a[klo] +
		(b * b * b - b) * y2a[khi]) * (h * h) / 6.0;
	return 1;
}

int NUMsplint_d (double xa[], double ya[], double y2a[], long n,
	double x, double *y)
{
	long klo, khi, k;
	double h, b, a;

	klo = 1; khi = n;
	while (khi-klo > 1)
	{
		k = (khi + klo) >> 1;
		if (xa[k] > x) khi = k;
		else klo = k;
	}
	h = xa[khi] - xa[klo];
	if (h == 0.0) return Melder_error ("NUMsplint_d: bad input value");
	a = (xa[khi] - x) / h;
	b = (x - xa[klo]) / h;
	*y = a * ya[klo] + b * ya[khi]+((a * a * a - a) * y2a[klo] +
		(b * b * b - b) * y2a[khi]) * (h * h) / 6.0;
	return 1;
}

#define MACRO_NUMvector_extrema(TYPE) \
{ \
	long i; \
	TYPE tmin = v[lo]; \
	TYPE tmax = tmin; \
	for (i = lo + 1; i <= hi; i++) \
	{ \
		if (v[i] < tmin) tmin = v[i]; \
		else if (v[i] > tmax) tmax = v[i]; \
	} \
	*min = tmin; *max = tmax; \
}

void NUMlvector_extrema (long v[], long lo, long hi, double *min, double *max)
MACRO_NUMvector_extrema (long)

void NUMfvector_extrema (float v[], long lo, long hi, double *min, double *max)
MACRO_NUMvector_extrema (float)

void NUMivector_extrema (int v[], long lo, long hi, double *min, double *max)
MACRO_NUMvector_extrema (int)

void NUMdvector_extrema (double v[], long lo, long hi, double *min, double *max)
MACRO_NUMvector_extrema (double)

void NUMsvector_extrema (short v[], long lo, long hi, double *min, double *max)
MACRO_NUMvector_extrema (short)

#undef MACRO_NUMvector_extrema

#define MACRO_NUMclip(TYPE)\
{\
	long i; \
	TYPE tmin = min, tmax = max;\
	for (i = lo; i <= hi; i++)\
	{\
		if (v[i] < tmin) v[i] = tmin;\
		else if (v[i] > tmax) v[i] = tmax;\
	}\
}


void NUMlvector_clip (long v[], long lo, long hi, double min, double max)
MACRO_NUMclip (long)

void NUMfvector_clip (float v[], long lo, long hi, double min, double max)
MACRO_NUMclip (float)

void NUMivector_clip (int v[], long lo, long hi, double min, double max)
MACRO_NUMclip (int)

void NUMdvector_clip (double v[], long lo, long hi, double min, double max)
MACRO_NUMclip (double)

void NUMsvector_clip (short v[], long lo, long hi, double min, double max)
MACRO_NUMclip (short)

#undef MACRO_NUMclip

#define MACRO_NUMmatrix_extrema(TYPE)\
{\
	long i, j;\
	TYPE xmin, xmax;\
	xmax = xmin = x[rb][cb];\
	for (i = rb; i <= re; i++)\
	{\
		for (j = cb; j <= ce; j++)\
		{\
			TYPE t = x[i][j];\
			if (t < xmin) xmin = t; \
			else if (t > xmax) xmax = t;\
		}\
	}\
	*min = xmin; *max = xmax;\
}

void NUMdmatrix_extrema (double **x, long rb, long re, long cb, long ce, 
	double *min, double *max)
MACRO_NUMmatrix_extrema(double)

void NUMfmatrix_extrema (float **x, long rb, long re, long cb, long ce, 
	double *min, double *max)
MACRO_NUMmatrix_extrema(float)

void NUMlmatrix_extrema (long **x, long rb, long re, long cb, long ce, 
	double *min, double *max)
MACRO_NUMmatrix_extrema(long)

void NUMimatrix_extrema (int **x, long rb, long re, long cb, long ce, 
	double *min, double *max)
MACRO_NUMmatrix_extrema(int)

void NUMsmatrix_extrema (short **x, long rb, long re, long cb, long ce, 
	double *min, double *max)
MACRO_NUMmatrix_extrema(short)

#undef MACRO_NUMmatrix_extrema

/* Does the line segment from (x1,y1) to (x2,y2) intersect with the line segment from (x3,y3) to (x4,y4)? */
int NUMdoLineSegmentsIntersect (double x1, double y1, double x2, double y2, double x3, double y3,
	double x4, double y4)
{
	int o11 = NUMgetOrientationOfPoints (x1, y1, x2, y2, x3, y3);
	int o12 = NUMgetOrientationOfPoints (x1, y1, x2, y2, x4, y4);
	int o21 = NUMgetOrientationOfPoints (x3, y3, x4, y4, x1, y1);
	int o22 = NUMgetOrientationOfPoints (x3, y3, x4, y4, x2, y2);
	return ((o11 * o12 < 0) && (o21 * o22 < 0)) || (o11 * o12 * o21 * o22 == 0);
}

int NUMgetOrientationOfPoints (double x1, double y1, double x2, double y2, double x3, double y3)
{
	int orientation;
	double dx2 = x2 - x1, dy2 = y2 - y1;
	double dx3 = x3 - x1, dy3 = y3 - y1;
	if (dx2 * dy3 > dy2 * dx3) orientation = 1;
	else if (dx2 * dy3 < dy2 * dx3) orientation = -1;
	else
	{
		if ((dx2 * dx3 < 0) || (dy2 * dy3 < 0)) orientation = -1;
		else if ((dx2*dx2+dy2*dy2) >= (dx3*dx3+dy3*dy3)) orientation = 0;
		else orientation = 1;
	}
	return orientation;
}

int NUMgetIntersectionsWithRectangle (double x1, double y1, double x2, double y2, 
	double xmin, double ymin, double xmax, double ymax, double *xi, double *yi)
{
	double x[6], y[6], t;
	long i, ni = 0;
	
	x[1] = x[4] = x[5] = xmin;
	x[2] = x[3] = xmax;
	y[1] = y[2] = y[5] = ymin;
	y[3] = y[4] = ymax;
/*
	Calculate intersection of line segment through p1=(x1,y1) to p2(x2,y2) with line segment
	through p3=(x3,y3) to p4=(x4,y4).
	Parametrisation of the lines:
	l1 = p1 + s (p2 - p1), s in (-inf,+inf)
	l2 = p3 + t (p4 - p3), t in (-inf,+inf).
	When s and t are in [0,1] we have line segments between the points.
	At the intersection l1 == l2. We get for the x and y coordinates:
		x1 + s (x2 - x1) = x3 + t (x4 - x3).............(1)
		y1 + s (y2 - y1) = y3 + t (y4 - y3).............(2)
	Multiply (1)*(y2 - y1) and (2)*(x2 - x1):
		x1 (y2 - y1) + s (x2 - x1)(y2 - y1) = x3 (y2 - y1) + t (x4 - x3)(y2 - y1).......(3)
		y1 (x2 - x1) + s (y2 - y1)(x2 - x1) = y3 (x2 - x1) + t (y4 - y3)(x2 - x1).......(4)
	(3)-(4) with y21 = y2 -y1, x21 = x2 - x1, x43 = x4 - x3, ...
		x1 y21 - y1 x21 = x3 y21 - y3 x21 +t (x43 y21 - y43 x21)
	Combining:
		y31 x21 - x31 y21 = t (x43 y21 - y43 x21)
	Therefor at the intersection we have:
	
		t = (y31 x21 - x31 y21) / (x43 y21 - y43 x21)
		
	If (x43 y21 - y43 x21) == 0
		There is no intersection.
	If (t < 0 || t >= 1)
		No intersection in the segment l2 
		To count intersections in a corner only once we have t < 0 instead of t <= 0!
*/
	
	for (i = 1; i <= 4; i++)
	{
		double denom = (x[i+1] - x[i]) * (y2 - y1) - (y[i+1] - y[i]) * (x2 - x1);
		if (denom == 0) continue;
		/* We have an intersection. */
		t = ((y[i] - y1) * (x2 - x1) - (x[i] - x1) * (y2 - y1)) / denom;
		if (t < 0 || t >= 1) continue;
		/* Intersection is within rectangle side. */
		ni++;
		if (ni > 2)
		{
			(void) Melder_error ("Too many intersections.");
			return 2;
		}
		xi[ni] = x[i] + t * (x[i+1] - x[i]);
		yi[ni] = y[i] + t * (y[i+1] - y[i]);
	}
	return ni;
}

void NUMgetEllipseBoundingBox (double a, double b, double cospsi, 
	double *width, double *height)
{

	Melder_assert (cospsi>= -1 && cospsi <= 1);
		
	if (cospsi == 1)
	{
		/*
			a-axis along x-axis
		*/
		
		*width = a;
		*height = b;
	}
	else if (cospsi == 0)
	{
		/*
			a-axis along y-axis
		*/
		
		*width = b;
		*height = a;
	}
	else
	{
		double psi = acos (cospsi), sn = sin (psi);
		double phi = atan2 (-b * sn, a * cospsi);
		*width = fabs (a * cospsi * cos (phi) - b * sn * sin (phi));
		phi = atan2 (b *cospsi , a * sn);
		*height = fabs (a * sn * cos (phi) + b * cospsi * sin (phi));
	}
}

static void NUMgetLinearTransform (double x1, double x2, double x3, double x4,
	double *a, double *b)
{
	Melder_assert (x1 < x2 && x3 < x4);
	*a = (x3 - x4) / (x1 - x2);
	*b = x3 - *a * x1;
}

static double NUMtransformLinear (double x, double a, double b)
{
	return a * x + b;
}

/*
Closely modelled after the netlib code by Oleg Keselyov.
*/
double NUMminimize_brent (double (*f) (double x, void *closure), double a, double b,
	void *closure, double tol, double *fx)
{
	double x, v, fv, w, fw;
	const double golden = 1 - NUM_goldenSection;
	const double sqrt_epsilon = sqrt (NUMfpp -> eps);
	long iter, itermax = 60;
	
	Melder_assert (tol > 0 && a < b);
  
	/* First step - golden section */
	
	v = a + golden * (b - a);
	fv = (*f)(v, closure);
	x = v;  w = v;
	*fx = fv;  fw = fv;

	for(iter = 1; iter <= itermax; iter++)
	{
		double range = b - a;
		double middle_range = (a + b) / 2;
		double tol_act = sqrt_epsilon * fabs(x) + tol / 3;
		double new_step; /* Step at this iteration */

       

		if (fabs(x - middle_range) + range / 2 <= 2 * tol_act)
		{
      		return x;
		}

		/* Obtain the golden section step */
		
		new_step = golden * (x < middle_range ? b - x : a - x);


		/* Decide if the parabolic interpolation can be tried	*/
		
		if (fabs(x - w) >= tol_act)
		{
			/*
				Interpolation step is calculated as p/q; 
				division operation is delayed until last moment.
			*/
			
			double p, q, t;

			t = (x - w) * (*fx - fv);
			q = (x - v) * (*fx - fw);
			p = (x - v) * q - (x - w) * t;
			q = 2 * (q - t);

			if( q > 0)
			{
				p = -p;
			}
			else
			{
				q = -q;
			}

			/*
				If x+p/q falls in [a,b], not too close to a and b,
				and isn't too large, it is accepted.
				If p/q is too large then the golden section procedure can
				reduce [a,b] range.
			*/
			
			if( fabs (p) < fabs (new_step * q) &&
				p > q * (a - x + 2 * tol_act) &&
				p < q * (b - x - 2 * tol_act))
			{
				new_step = p / q;
			}
		}

		/* Adjust the step to be not less than tolerance. */
		
		if (fabs(new_step) < tol_act)
		{
			new_step = new_step > 0 ? tol_act : - tol_act;
		}

		/* Obtain the next approximation to min	and reduce the enveloping range */
		
		{
			double t = x + new_step;	/* Tentative point for the min	*/
			double ft = (*f)(t, closure);

			/*
				If t is a better approximation, reduce the range so that
				t would fall within it. If x remains the best, reduce the range
				so that x falls within it.
			*/
						
			if (ft <= *fx)
			{
				if( t < x )
				{
					b = x;
				}
				else
				{
					a = x;
				}
      
				v = w;  w = x;  x = t;
				fv = fw;  fw = *fx;  *fx = ft;
			}
			else
			{        		             
				if (t < x)
				{
					a = t;
				}                   
				else
				{
					b = t;
				}
      
				if (ft <= fw || w == x)
				{
					v = w; w = t;
					fv = fw; fw = ft;
				}
				else if (ft <= fv || v == x || v == w)
				{
					v = t;
					fv=ft;
				}
			}
		}
	}
	Melder_warning ("NUMminimize_brent: maximum number of iterations (%d) "
		"exceeded.", itermax);
	return x;
}

#undef MAX
#undef MIN
#undef SIGN

/* End of file NUM2.c */