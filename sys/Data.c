/* Data.c
 *
 * Copyright (C) 1992-2004 Paul Boersma
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at
 * your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/*
 * pb 2002/03/07 GPL
 * pb 2003/07/02 Data_copy returns NULL if argument is NULL
 * pb 2003/09/14 old ClassTextFile formant readable across systems
 * pb 2004/10/16 C++ compatible struct tags
 */

#include "Collection.h"

structMelderFile Data_fileBeingRead;

static int copy (Any data1, Any data2) {
	(void) data1;
	(void) data2;
	return 1;
}

static int equal (Any data1, Any data2) {
	(void) data1;
	(void) data2;
	return 1;
}   /* Names may be different. */

static int writeAscii (Any data, FILE *f) {
	(void) data;
	(void) f;
	return 1;
}

static int readAscii (Any data, FILE *f) {
	(void) data;
	(void) f;
	return 1;
}

static int writeBinary (Any data, FILE *f) {
	(void) data;
	(void) f;
	return 1;
}

static int readBinary (Any data, FILE *f) {
	(void) data;
	(void) f;
	return 1;
}

static int writeCache (Any data, CACHE *f) {
	(void) data;
	(void) f;
	return 1;
}

static int readCache (Any data, CACHE *f) {
	(void) data;
	(void) f;
	return 1;
}

static int writeLisp (Any data, FILE *f) {
	(void) data;
	(void) f;
	return 1;
}

static int readLisp (Any data, FILE *f) {
	(void) data;
	(void) f;
	return 1;
}

class_methods (Data, Thing)
	class_method (copy)
	class_method (equal)
	class_method (writeAscii)
	class_method (readAscii)
	class_method (writeBinary)
	class_method (readBinary)
	class_method (writeCache)
	class_method (readCache)
	class_method (writeLisp)
	class_method (readLisp)
class_methods_end

Any Data_copy (I) {
	iam (Data);
	Data thee;
	if (me == NULL) return NULL;
	if (our copy == classData -> copy) return Melder_errorp ("(Data_copy:) Class %s cannot be copied.", our _className);
	thee = Thing_new (my methods);
	if (! thee) return NULL;
	if (! our copy (me, thee)) {
		forget (thee);
		return Melder_errorp ("(Data_copy:) Object of class %s not copied.", our _className);
	}
	Thing_setName (thee, my name);
	return thee;
}

int Data_equal (I, thou) {
	iam (Data); thouart (Data);
	int offset;
	if (my methods != thy methods) return 0;   /* Different class: not equal. */
	offset = sizeof (struct structData);   /* We already compared the methods, and are going to skip the names. */
	if (! memcmp ((char *) me + offset, (char *) thee + offset, our _size - offset))
		return 1;   /* No shallow differences. */
	return our equal (me, thee);
}

int Data_canWriteAscii (I) {
	iam (Data);
	return our writeAscii != classData -> writeAscii;
}

int Data_writeAscii (I, FILE *f) {
	iam (Data);
	if (! our writeAscii (me, f)) return 0;
	if (ferror (f)) return Melder_error ("(Data_writeAscii:) I/O error.");
	return 1;
}

int Data_writeToConsole (I) {
	iam (Data);
	int result;
	if (! Data_canWriteAscii (me)) return Melder_error ("(Data_writeToConsole:) Class %s cannot be written.", our _className);
	printf ("Write to console: class %s,  name \"%s\".\n",
		Thing_className (me), my name ? my name : "<none>");
	result = Data_writeAscii (me, stdout);
	putchar ('\n');
	return result;
}

int Data_writeToTextFile (I, MelderFile fs) {
	iam (Data);
	char className [100];
	FILE *f;
	if (! Data_canWriteAscii (me)) return Melder_error ("(Data_writeToTextFile:) Objects of class %s cannot be written to a text file.", our _className);
	if ((f = Melder_fopen (fs, "w")) == NULL) return 0;
	MelderFile_setMacTypeAndCreator (fs, 'TEXT', 0);
	if (our version)
		sprintf (className, "%s %ld", our _className, our version);
	else
		strcpy (className, our _className);
	if (fprintf (f, "File type = \"ooTextFile\"\nObject class = \"%s\"\n", className) == EOF || ! Data_writeAscii (me, f)) {
		fclose (f);
		return Melder_error ("(Data_writeToTextFile:) Cannot write file \"%.200s\".", MelderFile_messageName (fs));
	}
	putc ('\n', f);
	fclose (f);
	MelderFile_setMacTypeAndCreator (fs, 'TEXT', 0);
	return 1;
}

int Data_writeToShortTextFile (I, MelderFile fs) {
	iam (Data);
	char className [100];
	FILE *f;
	if (! Data_canWriteAscii (me)) return Melder_error ("(Data_writeToShortTextFile:) Objects of class %s cannot be written to a text file.", our _className);
	if ((f = Melder_fopen (fs, "w")) == NULL) return 0;
	if (our version)
		sprintf (className, "%s %ld", our _className, our version);
	else
		strcpy (className, our _className);
	ascio_verbose (FALSE);
	if (fprintf (f, "File type = \"ooTextFile short\"\n\"%s\"\n", className) == EOF || ! Data_writeAscii (me, f)) {
		fclose (f);
		ascio_verbose (TRUE);
		return Melder_error ("(Data_writeToShortTextFile:) Cannot write file \"%.200s\".", MelderFile_messageName (fs));
	}
	putc ('\n', f);
	fclose (f);
	ascio_verbose (TRUE);
	MelderFile_setMacTypeAndCreator (fs, 'TEXT', 0);
	return 1;
}

int Data_canWriteBinary (I) {
	iam (Data);
	return our writeBinary != classData -> writeBinary;
}

int Data_writeBinary (I, FILE *f) {
	iam (Data);
	if (! our writeBinary (me, f)) return 0;
	if (ferror (f)) return Melder_error ("I/O error.");
	return 1;
}

int Data_writeToBinaryFile (I, MelderFile fs) {
	iam (Data);
	char className [100];
	FILE *f;
	if (! Data_canWriteBinary (me)) return Melder_error ("(Data_writeToBinaryFile:) Objects of class %s cannot be written to a generic binary file.", our _className);
	if ((f = Melder_fopen (fs, "wb")) == NULL) return 0;
	if (fprintf (f, "ooBinaryFile") < 0) goto error;
	if (our version)
		sprintf (className, "%s %ld", our _className, our version);
	else
		strcpy (className, our _className);
	binputs1 (className, f);
	if (! Data_writeBinary (me, f)) goto error;
	fclose (f);
	MelderFile_setMacTypeAndCreator (fs, 'BINA', 0);
	return 1;
error:
	fclose (f);
	return Melder_error ("(Data_writeToBinaryFile:) Cannot write file \"%.200s\".", MelderFile_messageName (fs));
}

int Data_canWriteLisp (I) {
	iam (Data);
	return our writeLisp != classData -> writeLisp;
}

int Data_writeLisp (I, FILE *f) {
	iam (Data);
	if (! our writeLisp (me, f)) return 0;
	if (ferror (f)) return Melder_error ("(Data_writeLisp:) I/O error.");
	return 1;
}

int Data_writeLispToConsole (I) {
	iam (Data);
	if (! Data_canWriteLisp (me)) return Melder_error ("(Data_writeLispToConsole:) Class %s cannot be written as LISP.", our _className);
	printf ("Write as LISP sequence to console: class %s,  name \"%s\".\n",
		Thing_className (me), my name ? my name : "<none>");
	return Data_writeLisp (me, stdout);
}

int Data_writeToLispFile (I, MelderFile fs) {
	iam (Data);
	FILE *f;
	if (! Data_canWriteLisp (me)) return Melder_error ("(Data_writeToLispFile:) Class %s cannot be written as LISP.", our _className);
	if ((f = Melder_fopen (fs, "w")) == NULL) return 0;
	if (fprintf (f, "%sLispFile\n", our _className) == EOF || ! Data_writeLisp (me, f)) {
		fclose (f);
		return Melder_error ("(Data_writeToLispFile:) Error while writing file \"%.200s\". Disk probably full.", MelderFile_messageName (fs));
	}
	fclose (f);
	MelderFile_setMacTypeAndCreator (fs, 'TEXT', 0);
	return 1;
}

int Data_canReadAscii (I) {
	iam (Data);
	return our readAscii != classData -> readAscii;
}

int Data_readAscii (I, FILE *f) {
	iam (Data);
	if (! our readAscii (me, f) || Melder_hasError ())
		return Melder_error ("(Data_readAscii:) %s not read.", Thing_className (me));
	if (feof (f))
		return Melder_error ("(Data_readAscii:) Early end of file. %s not read.", Thing_className (me));
	if (ferror (f))
		return Melder_error ("(Data_readAscii:) I/O error. %s not read.", Thing_className (me));
	return 1;
}

Any Data_readFromTextFile (MelderFile file) {
	Data me;
	FILE *f;
	char line [200], *end;
	if ((f = Melder_fopen (file, "r")) == NULL) return NULL;
	fgets (line, 199, f);
	end = strstr (line, "ooTextFile");   /* oo format? */
	if (end) {
		char *klas;
		fseek (f, (end - line) + 11, SEEK_SET);   /* HACK: in order to be able to read classic Mac text files on Windows, Unix, or MacOS X. */
		klas = ascgets1 (f);
		if (! klas || ! (me = Thing_newFromClassName (klas))) { fclose (f); return 0; }
		Melder_free (klas);
	} else {
		end = strstr (line, "TextFile");
		fseek (f, (end - line) + 8, SEEK_SET);   /* HACK: in order to be able to read classic Mac text files on Windows, Unix, or MacOS X. */
		if (! end || ! (*end = '\0', me = Thing_newFromClassName (line))) {
			fclose (f);
			return NULL;
		}
		Thing_version = -1;   /* Old version: override version number, which was set to 0 by newFromClassName. */
	}
	MelderFile_copy (file, & Data_fileBeingRead);
	if (! Data_readAscii (me, f)) forget (me);
	fclose (f);
	return me;
}

int Data_canReadBinary (I) {
	iam (Data);
	return our readBinary != classData -> readBinary;
}

int Data_readBinary (I, FILE *f) {
	iam (Data);
	if (! our readBinary (me, f)) return Melder_error ("(Data_readBinary:) %s not read.", Thing_className (me));
	if (feof (f))
		return Melder_error ("(Data_readBinary:) Early end of file. %s not read.", Thing_className (me));
	if (ferror (f)) return Melder_error ("(Data_readBinary:) I/O error. %s not read.", Thing_className (me));
	return 1;
}

Any Data_readFromBinaryFile (MelderFile file) {
	Data me;
	int n;
	FILE *f;
	char line [200], *end;
	if ((f = Melder_fopen (file, "rb")) == NULL) return NULL;
	n = fread (line, 1, 199, f); line [n] = '\0';
	end = strstr (line, "ooBinaryFile");
	if (end) {
		char *klas;
		fseek (f, strlen ("ooBinaryFile"), 0);
		klas = bingets1 (f);
		if (! klas || ! (me = Thing_newFromClassName (klas))) { fclose (f); return 0; }
		Melder_free (klas);
	} else {
		end = strstr (line, "BinaryFile");
		if (! end || ! (*end = '\0', me = Thing_newFromClassName (line))) {
			fclose (f);
			return Melder_errorp ("(Data_readFromBinaryFile:) File \"%.200s\" is not a Data binary file.", MelderFile_messageName (file));
		}
		Thing_version = -1;   /* Old version: override version number, which was set to 0 by newFromClassName. */
		rewind (f);
		fread (line, 1, end - line + strlen ("BinaryFile"), f);
	}
	MelderFile_copy (file, & Data_fileBeingRead);
	if (! Data_readBinary (me, f)) forget (me);
	fclose (f);
	return me;
}

int Data_canReadLisp (I) {
	iam (Data);
	return our readLisp != classData -> readLisp;
}

int Data_readLisp (I, FILE *f) {
	iam (Data);
	if (! our readLisp (me, f)) return Melder_error ("(Data_readLisp:) %s not read.", Thing_className (me));
	/* (Do not check for end-of-file) */
	if (ferror (f)) return Melder_error ("(Data_readLisp:) I/O error. %s not read.", Thing_className (me));
	return 1;
}

Any Data_readFromLispFile (MelderFile file) {
	Data me;
	FILE *f;
	char line [200], *end;
	if ((f = Melder_fopen (file, "r")) == NULL) return NULL;
	fgets (line, 199, f);
	end = strstr (line, "LispFile");
	if (! end || ! (*end = '\0', me = Thing_newFromClassName (line))) {
		fclose (f);
		return Melder_errorp ("(Data_readFromLispFile:) File \"%.200s\" is not a Data LISP file.", MelderFile_messageName (file));
	}
	MelderFile_copy (file, & Data_fileBeingRead);
	if (! Data_readLisp (me, f)) forget (me);
	fclose (f);
	return me;
}

/* Generic reading. */

static int numFileTypeRecognizers = 0;
static Any (*fileTypeRecognizers [100]) (int nread, const char *header, MelderFile fs);
void Data_recognizeFileType (Any (*recognizer) (int nread, const char *header, MelderFile fs)) {
	Melder_assert (numFileTypeRecognizers < 100);
	fileTypeRecognizers [++ numFileTypeRecognizers] = recognizer;
}

Any Data_readFromFile (MelderFile file) {
	int nread, i;
	char header [513];
	FILE *f = Melder_fopen (file, "rb");
	if (! f) return NULL;
	nread = fread (& header [0], 1, 512, f);
	fclose (f);
	header [nread] = 0;

	/***** 1. Is this file a text file as defined in Data.c? *****/

	if (nread > 11) {
		char *p = strstr (header, "TextFile");
		if (p != NULL && p - header < nread - 8 && p - header < 40)
			return Data_readFromTextFile (file);
	}

	/***** 2. Is this file a binary file as defined in Data.c? *****/

	if (nread > 13) {
		char *p = strstr (header, "BinaryFile");
		if (p != NULL && p - header < nread - 10 && p - header < 40)
			return Data_readFromBinaryFile (file);
	}

	/***** 3. Is this file a LISP file as defined in Data.c? *****/

	if (nread > 11) {
		char *p = strstr (header, "LispFile");
		if (p != NULL && p - header < nread - 8 && p - header < 40)
			return Data_readFromLispFile (file);
	}

	/***** 4. Is this file of a type for which a recognizer has been installed? *****/

	MelderFile_copy (file, & Data_fileBeingRead);
	for (i = 1; i <= numFileTypeRecognizers; i ++) {
		Data object = fileTypeRecognizers [i] (nread, header, file);
		if (Melder_hasError ()) return NULL;
		if (object) return object;
	}

	/***** 5. Is this a common text file? *****/

	for (i = 0; i < nread; i ++)
		if (header [i] < 32 || header [i] > 126)   /* Not ASCII? */
			break;
	if (i >= nread) return Data_readFromTextFile (file);

	return Melder_errorp ("(Data_readFromFile:) File %.200s not recognized.", MelderFile_messageName (file));
}

/* Recursive routines for working with struct members. */

int Data_Description_countMembers (Data_Description structDescription) {
	Data_Description desc;
	int count = 0;
	for (desc = structDescription; desc -> name; desc ++)
		count ++;
	if (structDescription [0]. type == inheritwa) {
		Data_Description parentDescription = * (Data_Description *) structDescription [0]. tagType;
		if (parentDescription)
			return count + Data_Description_countMembers (parentDescription);
	}
	return count;
}

Data_Description Data_Description_findMatch (Data_Description structDescription, const char *name) {
	Data_Description desc;
	for (desc = structDescription; desc -> name; desc ++)
		if (strequ (name, desc -> name)) return desc;
	if (structDescription [0]. type == inheritwa) {
		Data_Description parentDescription = * (Data_Description *) structDescription [0]. tagType;
		if (parentDescription)
			return Data_Description_findMatch (parentDescription, name);
	}
	return NULL;   /* Not found. */
}

Data_Description Data_Description_findNumberUse (Data_Description structDescription, const char *string) {
	Data_Description desc;
	for (desc = structDescription; desc -> name; desc ++) {
		if (desc -> max1 && strequ (desc -> max1, string)) return desc;
		if (desc -> max2 && strequ (desc -> max2, string)) return desc;
	}
	if (structDescription [0]. type == inheritwa) {
		Data_Description parentDescription = * (Data_Description *) structDescription [0]. tagType;
		if (parentDescription)
			return Data_Description_findNumberUse (parentDescription, string);
	}
	return NULL;
}

/* Retrieving data from object + description. */

long Data_Description_integer (void *address, Data_Description description) {
	switch (description -> type) {
		case bytewa: return * (signed char *) ((char *) address + description -> offset);
		case shortwa: return * (short *) ((char *) address + description -> offset);
		case intwa: return * (int *) ((char *) address + description -> offset);
		case longwa: return * (long *) ((char *) address + description -> offset);
		case ubytewa: return * (unsigned char *) ((char *) address + description -> offset);
		case ushortwa: return * (unsigned short *) ((char *) address + description -> offset);
		case uintwa: return * (unsigned int *) ((char *) address + description -> offset);
		case ulongwa: return * (unsigned long *) ((char *) address + description -> offset);
		case charwa: return * (char *) ((char *) address + description -> offset);
		case collectionwa: return (* (Collection *) ((char *) address + description -> offset)) -> size;
		case objectwa: return (* (Collection *) ((char *) address + description -> offset)) -> size;
		default: return 0;
	}
}

int Data_Description_evaluateInteger (void *structAddress, Data_Description structDescription,
	const char *formula, long *result)
{
	if (formula == NULL) {   /* This was a VECTOR_FROM array. */
		*result = 1;
		return 1;
	}
	if (strnequ (formula, "my ", 3)) {
		char buffer [100], *minus1, *psize;
		Data_Description sizeDescription;
		strcpy (buffer, formula + 3);   /* Skip leading "my ". */
		if ((minus1 = strstr (buffer, " - 1")) != NULL)
			*minus1 = '\0';   /* Strip trailing " - 1", but remember. */
		if ((psize = strstr (buffer, " -> size")) != NULL)
			*psize = '\0';   /* Strip trailing " -> size". */
		if (! (sizeDescription = Data_Description_findMatch (structDescription, buffer))) {
			*result = 0;
			return 0 /*Melder_error ("Cannot find member \"%s\".", buffer)*/;
		}
		*result = Data_Description_integer (structAddress, sizeDescription);
		if (minus1) *result -= 1;
	} else {
		*result = atol (formula);
	}
	return 1;
}

/* End of file Data.c */