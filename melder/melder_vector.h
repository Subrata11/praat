#ifndef _melder_vector_h_
#define _melder_vector_h_
/* melder_vector.h
 *
 * Copyright (C) 1992-2018 Paul Boersma
 *
 * This code is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at
 * your option) any later version.
 *
 * This code is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this work. If not, see <http://www.gnu.org/licenses/>.
 */

/********** Arrays with one index (NUMarrays.cpp) **********/

byte * NUMvector_generic (integer elementSize, integer lo, integer hi, bool zero);
/*
	Function:
		create a vector [lo...hi]; if `zero`, then all values are initialized to 0.
	Preconditions:
		hi >= lo;
*/

void NUMvector_free_generic (integer elementSize, byte *v, integer lo) noexcept;
/*
	Function:
		destroy a vector v that was created with NUMvector.
	Preconditions:
		lo must have the same values as with the creation of the vector.
*/

byte * NUMvector_copy_generic (integer elementSize, byte *v, integer lo, integer hi);
/*
	Function:
		copy (part of) a vector v, which need not have been created with NUMvector, to a new one.
	Preconditions:
		if v != nullptr, the values v [lo..hi] must exist.
*/

void NUMvector_copyElements_generic (integer elementSize, byte *v, byte *to, integer lo, integer hi);
/*
	copy the vector elements v [lo..hi] to those of a vector 'to'.
	These vectors need not have been created by NUMvector.
*/

bool NUMvector_equal_generic (integer elementSize, byte *v1, byte *v2, integer lo, integer hi);
/*
	return true if the vector elements v1 [lo..hi] are equal
	to the corresponding elements of the vector v2; otherwise, return false.
	The vectors need not have been created by NUMvector.
*/

void NUMvector_append_generic (integer elementSize, byte **v, integer lo, integer *hi);
void NUMvector_insert_generic (integer elementSize, byte **v, integer lo, integer *hi, integer position);
/*
	add one element to the vector *v.
	The new element is initialized to zero.
	On success, *v points to the new vector, and *hi is incremented by 1.
	On failure, *v and *hi are not changed.
*/

/********** Arrays with two indices (NUMarrays.cpp) **********/

void * NUMmatrix_generic (integer elementSize, integer row1, integer row2, integer col1, integer col2, bool zero);
/*
	Function:
		create a matrix [row1...row2] [col1...col2]; if `zero`, then all values are initialized to 0.
	Preconditions:
		row2 >= row1;
		col2 >= col1;
*/

void NUMmatrix_free_generic (integer elementSize, byte **m, integer row1, integer col1) noexcept;
/*
	Function:
		destroy a matrix m created with NUM...matrix.
	Preconditions:
		if m != nullptr: row1 and col1
		must have the same value as with the creation of the matrix.
*/

void * NUMmatrix_copy_generic (integer elementSize, void *m, integer row1, integer row2, integer col1, integer col2);
/*
	Function:
		copy (part of) a matrix m, wich does not have to be created with NUMmatrix, to a new one.
	Preconditions:
		if m != nullptr: the values m [rowmin..rowmax] [colmin..colmax] must exist.
*/

void NUMmatrix_copyElements_generic (integer elementSize, char **mfrom, char **mto, integer row1, integer row2, integer col1, integer col2);
/*
	copy the matrix elements m [r1..r2] [c1..c2] to those of a matrix 'to'.
	These matrices need not have been created by NUMmatrix.
*/

bool NUMmatrix_equal_generic (integer elementSize, void *m1, void *m2, integer row1, integer row2, integer col1, integer col2);
/*
	return 1 if the matrix elements m1 [r1..r2] [c1..c2] are equal
	to the corresponding elements of the matrix m2; otherwise, return 0.
	The matrices need not have been created by NUM...matrix.
*/

byte *** NUMtensor3_generic (integer elementSize, integer pla1, integer pla2, integer row1, integer row2, integer col1, integer col2, bool initializeToZero);
void NUMtensor3_free_generic (integer elementSize, byte ***t, integer pla1, integer row1, integer col1) noexcept;

integer NUM_getTotalNumberOfArrays ();   // for debugging

template <class T>
T* NUMvector (integer from, integer to) {
	T* result = reinterpret_cast <T*> (NUMvector_generic (sizeof (T), from, to, true));
	return result;
}

template <class T>
T* NUMvector (integer from, integer to, bool initializeToZero) {
	T* result = reinterpret_cast <T*> (NUMvector_generic (sizeof (T), from, to, initializeToZero));
	return result;
}

template <class T>
void NUMvector_free (T* ptr, integer from) noexcept {
	NUMvector_free_generic (sizeof (T), reinterpret_cast <byte *> (ptr), from);
}

template <class T>
T* NUMvector_copy (T* ptr, integer lo, integer hi) {
	T* result = reinterpret_cast <T*> (NUMvector_copy_generic (sizeof (T), reinterpret_cast <byte *> (ptr), lo, hi));
	return result;
}

template <class T>
bool NUMvector_equal (T* v1, T* v2, integer lo, integer hi) {
	return NUMvector_equal_generic (sizeof (T), reinterpret_cast <byte *> (v1), reinterpret_cast <byte *> (v2), lo, hi);
}

template <class T>
void NUMvector_copyElements (T* vfrom, T* vto, integer lo, integer hi) {
	NUMvector_copyElements_generic (sizeof (T), reinterpret_cast <byte *> (vfrom), reinterpret_cast <byte *> (vto), lo, hi);
}

template <class T>
void NUMvector_append (T** v, integer lo, integer *hi) {
	NUMvector_append_generic (sizeof (T), reinterpret_cast <byte **> (v), lo, hi);
}

template <class T>
void NUMvector_insert (T** v, integer lo, integer *hi, integer position) {
	NUMvector_insert_generic (sizeof (T), reinterpret_cast <byte **> (v), lo, hi, position);
}

template <class T>
class autoNUMvector {
	T* d_ptr;
	integer d_from;
public:
	autoNUMvector<T> (integer from, integer to) : d_from (from) {
		d_ptr = NUMvector<T> (from, to, true);
	}
	autoNUMvector<T> (integer from, integer to, bool zero) : d_from (from) {
		d_ptr = NUMvector<T> (from, to, zero);
	}
	autoNUMvector (T *ptr, integer from) : d_ptr (ptr), d_from (from) {
	}
	autoNUMvector () : d_ptr (nullptr), d_from (1) {
	}
	~autoNUMvector<T> () {
		if (d_ptr) NUMvector_free (d_ptr, d_from);
	}
	T& operator[] (integer i) {
		return d_ptr [i];
	}
	T* peek () const {
		return d_ptr;
	}
	T* transfer () {
		T* temp = d_ptr;
		d_ptr = nullptr;   // make the pointer non-automatic again
		return temp;
	}
	void reset (integer from, integer to) {
		if (d_ptr) {
			NUMvector_free (d_ptr, d_from);
			d_ptr = nullptr;
		}
		d_from = from;
		d_ptr = NUMvector<T> (from, to, true);
	}
	void reset (integer from, integer to, bool zero) {
		if (d_ptr) {
			NUMvector_free (d_ptr, d_from);
			d_ptr = nullptr;
		}
		d_from = from;
		d_ptr = NUMvector<T> (from, to, zero);
	}
};

template <class T>
T** NUMmatrix (integer row1, integer row2, integer col1, integer col2) {
	T** result = static_cast <T**> (NUMmatrix_generic (sizeof (T), row1, row2, col1, col2, true));
	return result;
}

template <class T>
T** NUMmatrix (integer row1, integer row2, integer col1, integer col2, bool zero) {
	T** result = static_cast <T**> (NUMmatrix_generic (sizeof (T), row1, row2, col1, col2, zero));
	return result;
}

template <class T>
void NUMmatrix_free (T** ptr, integer row1, integer col1) noexcept {
	NUMmatrix_free_generic (sizeof (T), reinterpret_cast <byte **> (ptr), row1, col1);
}

template <class T>
T** NUMmatrix_copy (T** ptr, integer row1, integer row2, integer col1, integer col2) {
	#if 1
	T** result = static_cast <T**> (NUMmatrix_copy_generic (sizeof (T), ptr, row1, row2, col1, col2));
	#else
	T** result = static_cast <T**> (NUMmatrix_generic (sizeof (T), row1, row2, col1, col2));
	for (integer irow = row1; irow <= row2; irow ++)
		for (integer icol = col1; icol <= col2; icol ++)
			result [irow] [icol] = ptr [irow] [icol];
	#endif
	return result;
}

template <class T>
bool NUMmatrix_equal (T** m1, T** m2, integer row1, integer row2, integer col1, integer col2) {
	return NUMmatrix_equal_generic (sizeof (T), m1, m2, row1, row2, col1, col2);
}

template <class T>
void NUMmatrix_copyElements (T** mfrom, T** mto, integer row1, integer row2, integer col1, integer col2) {
	NUMmatrix_copyElements_generic (sizeof (T), reinterpret_cast <char **> (mfrom), reinterpret_cast <char **> (mto), row1, row2, col1, col2);
}

template <class T>
class autoNUMmatrix {
	T** d_ptr;
	integer d_row1, d_col1;
public:
	autoNUMmatrix (integer row1, integer row2, integer col1, integer col2) : d_row1 (row1), d_col1 (col1) {
		d_ptr = NUMmatrix<T> (row1, row2, col1, col2, true);
	}
	autoNUMmatrix (integer row1, integer row2, integer col1, integer col2, bool zero) : d_row1 (row1), d_col1 (col1) {
		d_ptr = NUMmatrix<T> (row1, row2, col1, col2, zero);
	}
	autoNUMmatrix (T **ptr, integer row1, integer col1) : d_ptr (ptr), d_row1 (row1), d_col1 (col1) {
	}
	autoNUMmatrix () : d_ptr (nullptr), d_row1 (0), d_col1 (0) {
	}
	~autoNUMmatrix () {
		if (d_ptr)
			NUMmatrix_free_generic (sizeof (T), reinterpret_cast <byte **> (d_ptr), d_row1, d_col1);
	}
	T*& operator[] (integer row) {
		return d_ptr [row];
	}
	T** peek () const {
		return d_ptr;
	}
	T** transfer () {
		T** temp = d_ptr;
		d_ptr = nullptr;
		return temp;
	}
	void reset (integer row1, integer row2, integer col1, integer col2) {
		if (d_ptr) {
			NUMmatrix_free_generic (sizeof (T), reinterpret_cast <byte **> (d_ptr), d_row1, d_col1);
			d_ptr = nullptr;
		}
		d_row1 = row1;
		d_col1 = col1;
		d_ptr = NUMmatrix<T> (row1, row2, col1, col2, true);
	}
	void reset (integer row1, integer row2, integer col1, integer col2, bool zero) {
		if (d_ptr) {
			NUMmatrix_free_generic (sizeof (T), reinterpret_cast <byte **> (d_ptr), d_row1, d_col1);
			d_ptr = nullptr;
		}
		d_row1 = row1;
		d_col1 = col1;
		d_ptr = NUMmatrix<T> (row1, row2, col1, col2, zero);
	}
};

#pragma mark - TENSOR
/*
	numvec and nummat: the type declarations are in melder.h, the function declarations in tensor.h

	Initialization (tested in praat.cpp):
		numvec x;                  // does not initialize x
		numvec x { };              // initializes x.at to nullptr and x.size to 0
		numvec x { 100, false };   // initializes x to 100 uninitialized values
		numvec x { 100, true };    // initializes x to 100 zeroes
		NUMvector<double> a (1, 100);
		numvec x { a, 100 };       // initializes x to 100 values from a base-1 array

		autonumvec y;                  // initializes y.at to nullptr and y.size to 0
		autonumvec y { 100, false };   // initializes y to 100 uninitialized values, having ownership
		autonumvec y { 100, true };    // initializes y to 100 zeroes, having ownership
		autonumvec y { x };            // initializes y to the content of x, taking ownership (explicit, so not "y = x")
		numvec z = releaseToAmbiguousOwner();   // releases ownership, x.at becoming nullptr
		"}"                            // end of scope destroys x.at if not nullptr
		autonumvec z = y.move()        // moves the content of y to z, emptying y

	To return an autonumvec from a function, transfer ownership like this:
		autonumvec foo () {
			autonumvec x { 100, false };
			... // fill in the 100 values
			return x;
		}
*/

class autonumvec;   // forward declaration, needed in the declaration of numvec

enum class kTensorInitializationType { RAW = 0, ZERO = 1 };

class numvec {
public:
	double *at;
	integer size;
public:
	numvec () = default;   // for use in a union
	numvec (double *givenAt, integer givenSize): at (givenAt), size (givenSize) { }
	numvec (const numvec& other) = default;
	numvec (const autonumvec& other) = delete;
	numvec& operator= (const numvec&) = default;
	numvec& operator= (const autonumvec&) = delete;
	double& operator[] (integer i) {
		return our at [i];
	}
	void reset () noexcept {   // on behalf of ambiguous owners (otherwise this could be in autonumvec)
		if (our at) {
			our _freeAt ();
			our at = nullptr;
		}
		our size = 0;
	}
protected:
	void _initAt (integer givenSize, kTensorInitializationType initializationType);
	void _freeAt () noexcept;
};

#define empty_numvec  numvec { nullptr, 0 }

/*
	An autonumvec is the sole owner of its payload, which is a numvec.
	When the autonumvec ends its life (goes out of scope),
	it should destroy its payload (if it has not sold it),
	because keeping a payload alive when the owner is dead
	would continue to use some of the computer's resources (namely, memory).
*/
class autonumvec : public numvec {
public:
	autonumvec (): numvec (nullptr, 0) { }   // come into existence without a payload
	autonumvec (integer givenSize, kTensorInitializationType initializationType) {   // come into existence and manufacture a payload
		our _initAt (givenSize, initializationType);
		our size = givenSize;
	}
	~autonumvec () {   // destroy the payload (if any)
		if (our at) our _freeAt ();
	}
	numvec get () const { return { our at, our size }; }   // let the public use the payload (they may change the values of the elements but not the at-pointer or the size)
	void adoptFromAmbiguousOwner (numvec given) {   // buy the payload from a non-autonumvec
		our reset();
		our at = given.at;
		our size = given.size;
	}
	numvec releaseToAmbiguousOwner () {   // sell the payload to a non-autonumvec
		double *oldAt = our at;
		our at = nullptr;   // disown ourselves, preventing automatic destruction of the payload
		return { oldAt, our size };
	}
	/*
		Disable copying via construction or assignment (which would violate unique ownership of the payload).
	*/
	autonumvec (const autonumvec&) = delete;   // disable copy constructor
	autonumvec& operator= (const autonumvec&) = delete;   // disable copy assignment
	/*
		Enable moving of temporaries or (for variables) via an explicit move().
		This implements buying a payload from another autonumvec (which involves destroying our current payload).
	*/
	autonumvec (autonumvec&& other) noexcept : numvec { other.get() } {   // enable move constructor for r-values (temporaries)
		other.at = nullptr;   // disown source
	}
	autonumvec& operator= (autonumvec&& other) noexcept {   // enable move assignment for r-values (temporaries)
		if (other.at != our at) {
			if (our at) our _freeAt ();
			our at = other.at;
			our size = other.size;
			other.at = nullptr;   // disown source
			other.size = 0;   // needed only if you insist on keeping the source in a valid state
		}
		return *this;
	}
	autonumvec&& move () noexcept { return static_cast <autonumvec&&> (*this); }   // enable constriction and assignment for l-values (variables) via explicit move()
};

class autonummat;   // forward declaration, needed in the declaration of nummat

class nummat {
public:
	double **at;
	integer nrow, ncol;
public:
	nummat () = default;   // for use in a union
	nummat (double **givenAt, integer givenNrow, integer givenNcol): at (givenAt), nrow (givenNrow), ncol (givenNcol) { }
	nummat (const nummat& other) = default;
	nummat (const autonummat& other) = delete;
	nummat& operator= (const nummat&) = default;
	nummat& operator= (const autonummat&) = delete;
	double *& operator[] (integer i) {
		return our at [i];
	}
	void reset () noexcept {   // on behalf of ambiguous owners (otherwise this could be in autonummat)
		if (our at) {
			our _freeAt ();
			our at = nullptr;
		}
		our nrow = 0;
		our ncol = 0;
	}
protected:
	void _initAt (integer givenNrow, integer givenNcol, kTensorInitializationType initializationType);
	void _freeAt () noexcept;
};

#define empty_nummat  nummat { nullptr, 0, 0 }

/*
	An autonummat is the sole owner of its payload, which is a nummat.
	When the autonummat ends its life (goes out of scope),
	it should destroy its payload (if it has not sold it),
	because keeping a payload alive when the owner is dead
	would continue to use some of the computer's resources (namely, memory).
*/
class autonummat : public nummat {
public:
	autonummat (): nummat { nullptr, 0, 0 } { }   // come into existence without a payload
	autonummat (integer givenNrow, integer givenNcol, kTensorInitializationType initializationType) {   // come into existence and manufacture a payload
		our _initAt (givenNrow, givenNcol, initializationType);
		our nrow = givenNrow;
		our ncol = givenNcol;
	}
	~autonummat () {   // destroy the payload (if any)
		if (our at) our _freeAt ();
	}
	nummat get () { return { our at, our nrow, our ncol }; }   // let the public use the payload (they may change the values in the cells but not the at-pointer, nrow or ncol)
	void adoptFromAmbiguousOwner (nummat given) {   // buy the payload from a non-autonummat
		our reset();
		our at = given.at;
		our nrow = given.nrow;
		our ncol = given.ncol;
	}
	nummat releaseToAmbiguousOwner () {   // sell the payload to a non-autonummat
		double **oldAt = our at;
		our at = nullptr;   // disown ourselves, preventing automatic destruction of the payload
		return { oldAt, our nrow, our ncol };
	}
	/*
		Disable copying via construction or assignment (which would violate unique ownership of the payload).
	*/
	autonummat (const autonummat&) = delete;   // disable copy constructor
	autonummat& operator= (const autonummat&) = delete;   // disable copy assignment
	/*
		Enable moving of temporaries or (for variables) via an explicit move().
		This implements buying a payload from another autonummat (which involves destroying our current payload).
	*/
	autonummat (autonummat&& other) noexcept : nummat { other.get() } {   // enable move constructor for r-values (temporaries)
		other.at = nullptr;   // disown source
	}
	autonummat& operator= (autonummat&& other) noexcept {   // enable move assignment for r-values (temporaries)
		if (other.at != our at) {
			if (our at) our _freeAt ();
			our at = other.at;
			our nrow = other.nrow;
			our ncol = other.ncol;
			other.at = nullptr;   // disown source
			other.nrow = 0;   // needed only if you insist on keeping the source in a valid state
			other.ncol = 0;
		}
		return *this;
	}
	autonummat&& move () noexcept { return static_cast <autonummat&&> (*this); }
};

conststring32 Melder_numvec (numvec value);
conststring32 Melder_nummat (nummat value);

/* End of file melder_vector.h */
#endif
