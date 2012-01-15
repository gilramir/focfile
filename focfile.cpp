/*
    focfile.cpp
    -----------
    C++ classes to read Intel-based FOCUS files (PC/FOCUS 6.01)
    FOCUS and PC/FOCUS are trademarks of Information Builders, Inc. (IBI)

    *********************************************************
    This library is in no way related to or supported by IBI.
    IBI's FOCUS is a proprietary database whose format may
    change at any time. If you have any problems, suggestions,
    or comments about the FocFile C++ library, contact
    the author, not Information Builders!
    *********************************************************

    Copyright (C) 1997  Gilbert Ramirez <gram@alumni.rice.edu>
    $Id: focfile.cpp,v 1.21 1997/04/22 01:17:04 gram Exp $

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public
    License along with this library; if not, write to the Free
    Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "focfile.h"

// Flags
// ---------------------------------
//#define DEBUG
#define HAS_STRDUP
//#define IBM_MAINFRAME
#define FAST_CMP
// ---------------------------------

#define DEBUG_PROGRAM_NAME	"FocFile"
#include "debug.h"

// Offsets and other constants
// 4000	= 0xfa0
#define CTRLOFF		4000 - 28	/* 0xf84 */
#define FDTOFF		4000 - 30	/* 0xf86 */

// FOCUS Pointer types
#define PTR_KU		1		/* Keyed Unique */
#define PTR_KM		2		/* Keyed Multiple */
#define	PTR_CHILD	3		/* Descendant or Child instance */
#define PTR_NEXT	4		/* Next instance of same segment */
#define PTR_PARENT	5		/* Parent */
#define PTR_PRIOR	6		/* (Unused at this time: 6/20/87) */
#define PTR_EOFILE	7		/* End of File */
#define PTR_DELETED	8		/* Deleted data */
#define PTR_IGNORED	9		/* ??? */
#define PTR_DKU		10		/* Dynamic keyed unique */
#define PTR_DKM		11		/* Dynamic keyed multiple */
#define PTR_EOCHAIN	0x010000000	/* End of chain */
#define PTR_NORECORD	(short)0xdddd	/* No child exists ???*/ /* -8739 */

#ifdef IBM_MAINFRAME
 #define INDEXTYPE_HASH			0
 #define INDEXTYPE_BTREE		128
#else /* not IBM_MAINFRAME */
 #define INDEXTYPE_BTREE		0
 #define INDEXTYPE_HASH			-1	/* n/a */
#endif /* IBM_MAINFRAME */

#define FIELDTYPE_ALPHA		'A'
#define FIELDTYPE_INTEGER	'I'
#define FIELDTYPE_DOUBLE	'D'
#define FIELDTYPE_FLOAT		'F'
#define FIELDTYPE_SMDATE	'S'

static inline int mkshort(UCHAR* ptr);
static void* xmalloc(char *label, int bytes);

static int intcmp(long *a, long *b);
static int doublecmp(double *a, double *b);
static int floatcmp(float *a, float *b);

#ifndef HAS_STRDUP
static char* strdup(const char *s);
#endif /* ! HAS_STRDUP */

#ifdef DEBUG
static int total_memory_allocated = 0;
void debug_cursor_pos(char *s, CURSOR_POS position_type);
#else /* not DEBUG */
 #define debug_cursor_pos(x,y)
#endif /* DEBUG */

// =============================================================
// CLASS: FOCFILE
// -------------------------------------------------------------
// This class is the one the programmer interacts with. It's
// the high-level class that does everything for the programmer.
// =============================================================
FOCFILE::FOCFILE(char *mfd_string, FILE *fh) {

	Parse_fdt(fh);
	Parse_mfd(mfd_string);

	// Go to the top
	reposition();

};

FOCFILE::FOCFILE(FILE *fh) {
	Parse_fdt(fh);
};

FOCFILE::~FOCFILE() {
	int i;

	// The segments
	for(i=1; i <= Num_segments; i++) {
		delete Segment[i];
	}
	free(Segment);

	// The indices
	for(i=1; i <= Num_indices; i++) {
		delete Index[i];
	}
	free(Index);

	// The joins
	free(Join_list);
};

void FOCFILE::Parse_fdt(FILE *fh) {

	FOCPAGE	*first_page;
	UCHAR	*buffer;
	int	entries_in_fdt;

	first_page = new FOCPAGE(fh);

	// How many segments does FOC contain?
	if (!(buffer = first_page->Return_byte_offset(1, 0))) {
		die("died on first page read\n");
	}

	entries_in_fdt = mkshort(&buffer[FDTOFF]);

	// GRJ: Before all the entries in the FDT, there is one extra
	// FDT. Remember that each entry in the FDT is twentry bytes. The
	// first two bytes of this extra FDT entry indicate the number
	// of __segments__. This is great, since we already know the
	// number of entries in the FDT. Doing the math, we can then figure
	// out the number of __indices__. 
	//
	// ENTRIES_IN_FDT == NUM_OF_SEGMENTS + NUM_OF_INDICES
	Num_segments = mkshort(&buffer[FDTOFF - (entries_in_fdt+1) * 20]);
	Num_indices = entries_in_fdt - Num_segments;

	Root_segment = NULL;	// so we can check for errors later

	// No joins in effect, yet.
	Join_list = NULL;

	Parse_fdt_seg(fh, buffer);
	Parse_fdt_idx(fh, buffer);
	delete first_page;
}

/*
Read the File Directory Table to initialize the segments.

1.  Create an array of segments in order to initialize them all within
	this function, in numerical order

2.  figure out who's the parent of whom. Using pointers, set up the
	segments in a hierarchy. Tell each segment who their children
	are. That way, each segment is in charge of calling its own
	children.

3.  Store the root segment's address in the FOCFILE object. Since
	the first segments is not necessarily the ROOT segment
	(I assume), we point to the segment that has no parents.

*/
void FOCFILE::Parse_fdt_seg(FILE* fh, UCHAR* buffer) {

	UCHAR	*seg_info;
	FOCSEG	*seg_ptr;
	int	seg_num;

	// I allocate one extra pointer (namely, Segment[0])
	// FOCUS arrays are 1-indexed. Instead of doing the
	// right thing and conserving space, I'm wasting space
	// by allocating room for an additional pointer (the 0th pointer)
	// that I don't need. But it makes my program easier to read.
	Segment = (FOCSEG**) xmalloc("Parse_fdt_seg",
			sizeof(FOCSEG*) * (Num_segments + 1));

	// I add this simple offset here so I don't have to do it
	// Num_segments number of times in the following loop.
	buffer += FDTOFF;

	// Initialize the segments
	for(seg_num = 1; seg_num <= Num_segments; seg_num++) {

		// Use this offset to make my (the programmer!) job
		// easier.
		seg_info = buffer - seg_num * 20;

		seg_ptr = new FOCSEG(seg_num, fh, seg_info);
		Segment[seg_num] = seg_ptr;

	}


	// Tell the parents who their children are.
	// And store away the pointer of the root segment
	for (seg_num = 1; seg_num <= Num_segments; seg_num++) {

		if (Segment[seg_num]->Get_parent() > 0) {
			debug("FILE::Adding seg %d -> child seg %d\n",
				Segment[seg_num]->Get_parent(),
				seg_num);
			Segment[Segment[seg_num]->Get_parent()]->
			    Add_child_pointer(Segment[seg_num]);

		}
		else {
			if (Root_segment != NULL) {
				die("Too many root segments.\n");
			}
			Root_segment = Segment[seg_num];
		}
	}
}

// Read the info for each index.
void FOCFILE::Parse_fdt_idx(FILE* fh, UCHAR* buffer) {

	UCHAR		*idx_info;
	FOCINDEX	*idx_ptr;
	int		idx_num;

	// As in Parse_fdt_seg, I waste space and allocate
	// a 0th element, even though I'm never going to use it.
	Index = (FOCINDEX**) xmalloc("Parse_fdt_idx",
			sizeof(FOCINDEX*) * (Num_indices + 1));

	// I add this simple offset here so I don't have to do it
	// Num_segments number of times in the following loop.
	buffer += FDTOFF - (Num_segments * 20);

	int idx_type;
	// Initialize the indices
	for(idx_num = 1; idx_num <= Num_indices; idx_num++) {

		// Use this offset to make my (the programmer!) job
		// easier.
		idx_info = buffer - idx_num * 20;

		// Figure out the type of index and allocate an object
		// for it.
		idx_type = FDT_index_type(idx_info);

		switch (idx_type) {
			case INDEXTYPE_BTREE:
				idx_ptr = new
					FOCINDEX_BTREE(idx_num, fh, idx_info);
				break;

			#ifdef IBM_MAINFRAME
			case INDEXTYPE_HASH:
				idx_ptr = new
					FOCINDEX_HASH(idx_num, fh, idx_info);
				break; */
			#endif /* IBM_MAINFRAME */

			default:
				die("Index %d type %d unknown!\n",
					idx_num, idx_type);
		}

		Index[idx_num] = idx_ptr;
	}
}


void FOCFILE::Parse_mfd(char *mfd_string) {

	char	*mfd;
	char	*string;
	char	*cursor;
	char	*separators = " ";

	mfd = strdup(mfd_string);
	string = mfd;
	debug("Parsing %s with (%s) as separators\n", mfd, separators);

	int	segment = 0;

	while ((cursor = strtok(string, separators)) != NULL) {
		string = NULL;

		switch (cursor[0]) {
		/* select segment n */
		case 's':

			if (sscanf(&cursor[1], "%d", &segment) != 1) {
				die("FILE::Parse_mfd bad s: %s\n", mfd_string);
			}
			debug("FILE::Parse_mfd command seg: %d\n", segment);
			if (segment < 1 || segment > Num_segments) {
				die("FILE::Parse_mfd bad seg: %d\n", segment);
			}
			break;

		case 't':
			debug("FILE::Parse_mfd seg %d type %s\n", segment,
				&cursor[1]);
			Segment[segment]->set_segtype(&cursor[1]);
			break;

		default:
			warn("FILE::Parse_mfd unknown command: %s\n",
				cursor);
		}
	}

	for (int seg_num = 1; seg_num <= Num_segments; seg_num++) {
		Segment[seg_num]->set_unique_children_array();
	}

	free(mfd);
}

// Allocate string space for the width of a field. The programmer
// can use the constants created from the master file description.
// Therefore the width of the field does not have to be hard-coded
// into the C++ program. Yes, I pass some paramters that aren't used.
// It wastes a bit of time, but the ease of the API offsets the
// waste.
char* FOCFILE::string_alloc(int seg, int offset, char type, int length) {

	char*	new_mem;

	if (type != FIELDTYPE_ALPHA) {
		die("string_alloc type not Alpha: %c\n", type);
	}

	new_mem = (char*) xmalloc("string_alloc", length + 1);
	new_mem[length] = 0;

	return new_mem;
}

char* FOCFILE::string_alloc(
		int a_seg, int a_offset, char a_type, int a_length,
		int b_seg, int b_offset, char b_type, int b_length) {

	char*	new_mem;
	int	length = b_offset - a_offset + b_length;

	// Make sure the two mentioned fields lie in the same segment
	if (a_seg != b_seg) {
		die("string_alloc fields lie in different segments: %d %d\n",
			a_seg, b_seg);
	}

	// Make sure field A comes before field B
	if (a_offset >= b_offset) {
		die("string_alloc field 1 does not precede field 2\n");
	}

	new_mem = (char*) xmalloc("string_alloc", length + 1);
	new_mem[length] = 0;

	return new_mem;
}

// Read arbitrary bytes from the current record
int FOCFILE::read_bytes(UCHAR* u, int seg, int offset, char type, int length) {

	return Segment[seg]->read_bytes(u, offset, length);
}


// These next 5 functions read a field from the current record
// Character fields
int FOCFILE::hold(char* s, int seg, int offset, char type, int length) {

	if (type != FIELDTYPE_ALPHA) {
		die("hold type not Alpha: %c\n", type);
	}

	return Segment[seg]->read_bytes((UCHAR*)s, offset, length);
}

// Integer fields
int FOCFILE::hold(long& l, int seg, int offset, char type, int length) {

	long value;

	if (type != FIELDTYPE_INTEGER) {
		die("hold type not Integer: %c\n", type);
	}

	if(Segment[seg]->read_bytes((UCHAR*)&value, offset, sizeof(long))) {
		l = value;
		return 1;
	}
	else {
		return 0;
	}

}

// Double fields
int FOCFILE::hold(double& d, int seg, int offset, char type, int length) {

	double value;

	if (type != FIELDTYPE_DOUBLE) {
		die("hold type not Double: %c\n", type);
	}

	if (Segment[seg]->read_bytes((UCHAR*)&value, offset, sizeof(double))) {
		d = value;
		return 1;
	}
	else {
		return 0;
	}

}

// Float fields
int FOCFILE::hold(float& f, int seg, int offset, char type, int length) {

	float value;

	if (type != FIELDTYPE_FLOAT) {
		die("hold type not Float: %c\n", type);
	}

	if(Segment[seg]->read_bytes((UCHAR*)&value, offset, sizeof(float))) {
		f = value;
		return 1;
	}
	else {
		return 0;
	}

}

// SMDATE fields
int FOCFILE::hold(SMDATE &smd, int seg, int offset, char type, int length) {

	long value;

	if (type != FIELDTYPE_SMDATE) {
		die("hold type not SMDATE: %c\n", type);
	}

	if(Segment[seg]->read_bytes((UCHAR*)&value, offset, sizeof(long))) {
		smd.set_julian(value, SMDATE_FOCUS);
		return 1;
	}
	else {
		return 0;
	}

}

// Multiple fields. *Has* to be character output.
int FOCFILE::hold(char* s,
			int a_seg, int a_offset, char a_type, int a_length,
			int b_seg, int b_offset, char b_type, int b_length) {

	int	length = b_offset - a_offset + b_length;

	if (a_seg != b_seg) {
		die("hold a_seg %d must equal b_seg %d\n",
			a_seg, b_seg);
	}

	if (Segment[a_seg]->read_bytes((UCHAR*)s, a_offset, length)) {
		s[length] = 0;
		return 1;
	}
	else {
		return 0;
	}
}

// Multiple fields. 
int FOCFILE::hold(UCHAR* b,
			int a_seg, int a_offset, char a_type, int a_length,
			int b_seg, int b_offset, char b_type, int b_length) {

	int	length = b_offset - a_offset + b_length;

	if (a_seg != b_seg) {
		die("hold a_seg %d must equal b_seg %d\n",
			a_seg, b_seg);
	}

	return Segment[a_seg]->read_bytes((UCHAR*)b, a_offset, length);
}

// Repositions the cursor in the segment to the first logical record.
// Each child segment is appropriately positioned.
//
// Callable as:
//	reposition(void)		Reposition root segment
//	reposition(SEGMENT_MACRO)	Reposition segment
//	reposition(FIELD_MACRO)		Reposition field's segment
void FOCFILE::reposition(int seg, int offset, char type, int length) {

	if (seg == 0) {
		Root_segment->reposition();
	}
	else if (seg > 0 && seg <= Num_segments) {
		Segment[seg]->reposition();
	}
	else {
		die("reposition called for non-existant segment %i\n", seg);
	}
}

// Moves the cursor of the segment to the next logical record
// Returns 1 on success, 0 on failure
//
// Callable as:
//	next(void)			Next root segment record
//	next(SEGMENT_MACRO)		Next record in segment
//	next(FIELD_MACRO)		Next record in field's segment
int FOCFILE::next(int seg, int offset, char type, int length) {


	if (seg == 0 ) {
		return Root_segment->next();
	}
	else if (seg > 0 && seg <= Num_segments) {
		return Segment[seg]->next();
	}
	else {
		die("next called for non-existant segment %i\n", seg);
	}
}

// Does a next(), and then next()'s any unique children automatically.
// This makes it appear as if the unique children segments are merely
// extensions of the parent segment.
//
// Callable with void, SEGMENT_MACRO, and FIELD_MACRO arguments.
//
// Returns 1 on success, 0 on failure
int FOCFILE::next_with_uniques(int seg, int offset, char type, int length) {

	if (next(seg, offset, type, length)) {

		Segment[seg]->next_unique_children();
		return 1;
	}

	return 0;
}


// Prepares an index for usage. Indices take up space in memory,
// so I'll make the user tell me which indices he wants to use.
void FOCFILE::initialize_index(int idx, char type, int seg) {

	Index[idx]->initialize(type, seg);
}

// Check for the existence of a record with the indexed field
// equal to key
//
// returns 1 if it exists, 0 if it doesn't
int FOCFILE::find(int idx, char type, int seg, char* key) {
	return find(idx, type, seg, (void*) key);
}
int FOCFILE::find(int idx, char type, int seg, long& key) {
	return find(idx, type, seg, (void*) &key);
}
int FOCFILE::find(int idx, char type, int seg, double& key) {
	return find(idx, type, seg, (void*) &key);
}
int FOCFILE::find(int idx, char type, int seg, float& key) {
	return find(idx, type, seg, (void*) &key);
}
int FOCFILE::find(int idx, char type, int seg, SMDATE& key) {
	long date = key.julian();
	return find(idx, type, seg, (void*) &date);
}

int FOCFILE::find(int idx, char type, int seg, void* key) {

	FOCPTR junk;

	// Make sure the index is initialized
	if ( ! index_in_use(idx)) {
		initialize_index(idx, type, seg);
	}

	return Index[idx]->find(key, &junk);
}

// Same as find(), but moves the cursor (pointer to current record)
// to the first record it finds.
//
// returns 1 if it exists, 0 if it doesn't
int FOCFILE::match_index(int idx, char type, int seg, void* key) {

	debug("FILE::match_index called for index %d type %d seg %d "
		"key %c%c%c%c%c\n", idx, type, seg, ((char*)key)[0],
			((char*)key)[1],
			((char*)key)[2],
			((char*)key)[3],
			((char*)key)[4]);

	FOCPTR position;

	if (Index[idx]->find(key, &position)) {
		debug("FILE::match_index moving seg %d to page %d word %d\n",
			seg, position.page, position.word);
		//Segment[seg]->cursor_set(position, beginning);
		Segment[seg]->cursor_set(position, record);
		Segment[seg]->set_children_cursor_pos(beginning);
		return 1;
	}
	else {
		return 0;
	}
}

// Looks for the next record that matches a field. Does not use the index,
// and unlike FOCUS, you can match on any field. However, you can only
// match on one field at a time.
//
// This match() starts from the current record. If you want to start from
// the top of the segment, you have to issue a reposition() first. It
// moves the cursor.
//
// Returns one if its exists, 0 if it doesn't
int FOCFILE::match(int seg, int offset, char type, int length, char* key) {
	return match(seg, offset, type, length, (void*) key);
}
int FOCFILE::match(int seg, int offset, char type, int length, long& key) {
	return match(seg, offset, type, length, (void*) &key);
}
int FOCFILE::match(int seg, int offset, char type, int length, double& key) {
	return match(seg, offset, type, length, (void*) &key);
}
int FOCFILE::match(int seg, int offset, char type, int length, float& key) {
	return match(seg, offset, type, length, (void*) &key);
}
int FOCFILE::match(int seg, int offset, char type, int length, SMDATE& key) {
	long date = key.julian();
	return match(seg, offset, type, length, (void*) &date);
}


int FOCFILE::match(int seg, int offset, char type, int length, void* key) {

	int	ret_val;
	int	found = 0;
	UCHAR	*field_value;

	debug("FILE::match allocating \n");

	// Allocate memory to retrieve the value of the comparison-field.
	field_value = (UCHAR*) xmalloc("match", sizeof(UCHAR) * length);

	// Loop until we either find a matching record, or run out of data
	while (!found) {	
		debug("FILE::match looping\n");
		// Advance the record position.
		if (!(ret_val = next(seg, offset, type, length))) {
			debug("FILE::match no next inside loop\n");
			break;
		}

		read_bytes(field_value, seg, offset, type, length);

		if (memcmp(key, (void*) field_value, length) == 0) {
			found = 1;
		}

	}

	debug("FILE::match out of loop with find = %d\n", found);
	free(field_value);
	return found;
}

// Does a match(), then next()'s any unique children
int FOCFILE::match_with_uniques(int seg, int offset, char type, int length,
			void* key) {

	if (match(seg, offset, type, length, key)) {

		Segment[seg]->next_unique_children();
		return 1;
	}

	return 0;
}


// Build a JOIN between parent (p_*) and child (c_*)
// Returns an integer which is a unique JOIN ID. That ID can be used
// later to clear the JOIN.
int FOCFILE::join(int p_seg, int p_offset, char p_type, int p_length, // FIELD
			FOCFILE* c_foc,
			int c_idx, char c_type, int c_seg) {	// INDEX

	int		id;
	FOCJOIN*	new_join;
	FOCJOIN*	last_join;

	// Make sure that the child index is initialized!
	if ( ! c_foc->index_in_use(c_idx)) {
		c_foc->initialize_index(c_idx, c_type, c_seg);
	}

	// Find the next available ID number
	if (Join_list == NULL) {
		id = 1;

		debug("FILE::join setting\n");
		new_join = new FOCJOIN(Segment[p_seg], id, p_offset,
				p_length, c_foc, c_idx, c_type, c_seg);
		Join_list = new_join;
	}
	else {
		last_join = Join_list->last_foc(Join_list);
		id = last_join->id();

		debug("FILE::join appending\n");
		new_join = new FOCJOIN(Segment[p_seg], id, p_offset,
				p_length, c_foc, c_idx, c_type, c_seg);
		last_join->append_foc_list(new_join);
	}
	debug("FILE::join Got join id %d\n", id);


	debug("FILE::join calling segment %d\n", p_seg);
	Segment[p_seg]->join_segment_as_head(new_join);

	debug("FILE::join returning join id %d\n", id);
	return id;
}


// Join a segment as a child to a join
void FOCFILE::join_segment_as_child(FOCJOIN* join, int segnum) {

	debug("FILE::join_segment_as_child seg %d as a child to join %d\n",
		segnum, join->id() );
	Segment[segnum]->join_segment_as_child(join);

}

int FOCFILE::index_in_use(int index_number) {

	if (index_number <= 0 || index_number > Num_indices) {
		die("index_in_use: bad index number %d\n", index_number);
	}

	return Index[index_number]->index_in_use();
}

int FOCFILE::FDT_index_type(UCHAR *idx_fdt_entry) {

	return mkshort(&idx_fdt_entry[18]);
}

// Count the records in a segment. It supports a filter function.
int FOCFILE::reccount(int seg, int (*filter)(FOCFILE*)) {

	if (seg == 0 ) {
		return Root_segment->reccount(filter, this);
	}
	else if (seg > 0 && seg <= Num_segments) {
		return Segment[seg]->reccount(filter, this);
	}
	else {
		die("reccount called for non-existant segment %i\n", seg);
	}
}

int FOCFILE::reccount(int (*filter)(FOCFILE*)) {
	return Root_segment->reccount(filter, this);
}

int FOCFILE::reccount(int seg, int offset, char type, int length,
			int (*filter)(FOCFILE*)) {

	return reccount(seg, filter);
}

void FOCFILE::segment_name(char* answer, int seg) {
	if (seg == 0 ) {
		strcpy(answer, Root_segment->Segment_name());
	}
	else if (seg > 0 && seg <= Num_segments) {
		strcpy(answer, Segment[seg]->Segment_name());
	}
	else {
		die("segment_name called for non-existant segment %i\n", seg);
	}
}

void FOCFILE::index_name(char* answer, int idx) {
	if (idx > 0 && idx <= Num_indices) {
		strcpy(answer, Index[idx]->Index_name());
	}
	else {
		die("index_name called for non-existant index %i\n", idx);
	}
}


// =============================================================
// CLASS: FOCSEG
// -------------------------------------------------------------
// Class to handle FOCUS segments
// =============================================================

FOCSEG::FOCSEG(int seg_num, FILE* fh, UCHAR* fdt_entry) {

	// Terminate the string in NUL. 
	segment_name[8] = 0;

	// Store my ID # in case I need it.
	my_id = seg_num;

	// Create my page-buffer object
	Page = new FOCPAGE(fh);

	first_page	= mkshort(&fdt_entry[0]);
	last_page 	= mkshort(&fdt_entry[2]);

	memcpy(segment_name, &fdt_entry[4], 8);
	debug("SEG::Constructed %s\n", segment_name);

	segment_length	= mkshort(&fdt_entry[12]);
	parent_number	= mkshort(&fdt_entry[14]);
	number_of_pages	= mkshort(&fdt_entry[16]);
	number_of_pointers = mkshort(&fdt_entry[18]);

	// The number of children can be determined from
	// the number of pointers and whether or not we
	// have a parent.
	number_of_children = number_of_pointers - 1;

	if (parent_number > 0) {
		number_of_children--;
	}
	//debug("SEG::SEG %s has %d children\n", segment_name,
	//	number_of_children);

	// Allocate space for the pointers to my children
	if (number_of_children > 0) {
		Child = (FOCSEG**) xmalloc("SEG Constructor",
				number_of_children * sizeof(FOCSEG*));
	}
	else {
		Child = NULL;
	}

	// Initialize the child pointers to NULL. ***IMPORTANT***
	// Add_child_pointer() depends on these pointers being
	// initialized to NULL.
	for (int i = 0; i < number_of_children; i++) {
		Child[i] = NULL;
	}
	Unique_children	= NULL;

	// No joins yet
	Join_list	= NULL;
	Parent_join	= NULL;

	cursor 		= new FOCPTR();
	chain_beginning = new FOCPTR();
	cursor_pos	= inaccessible;
	segtype		= unknown;
};

FOCSEG::~FOCSEG() {

	delete Page;
	free(Child);	// don't iterate through children!
//	free(Join_list);
	free(Unique_children);	// don't iterate through children!
	delete cursor;
	delete chain_beginning;
};


// Adds a child segment pointer to this segments table of
// children. Everytime this is called, we loop through
// the vector of pointers until we find the NULL pointer.
// The new pointer goes there.
void FOCSEG::Add_child_pointer(FOCSEG* new_child) {

	int i = 0;

	if (number_of_children == 0) {
		die("SEG Add_child_pointer should have 0 children\n");
	}

	// Find the next available child slot.
	while (Child[i] != NULL) {
		i++;

		// oops! too far!
		if (i == number_of_children) {
			die("SEG Add_child_pointer has too many children!\n");
		}
	}

	Child[i] = new_child;
	debug("SEG::Add_child_pointer seg %s = %d child %d\n",
		segment_name, my_id, i);
}

// For now we're only interested in unique segments
void FOCSEG::set_segtype(char *segment_type) {

	// Parse the text to get the segtype
	switch (segment_type[0]) {
		/* Sn, SHn, S0 */
		case 'S':	
			switch (segment_type[1]) {
				case 'H':
					segtype = SHn;
					debug("FOCSEG::set_segtype to SHn\n");
					break;
				case '0':
					segtype = S0;
					debug("FOCSEG::set_segtype to S0\n");
					break;
				default:
					segtype = Sn;
					debug("FOCSEG::set_segtype to Sn\n");
					break;
			}
			/* get number of key fields here */
			break;

		case 'b':
			segtype = b;
			debug("FOCSEG::set_segtype to b\n");
			break;

		case 'U':
			segtype = U;
			debug("FOCSEG::set_segtype to U\n");
			break;

		/* KU, KM, KL, KLU */
		case 'K':
			switch (segment_type[1]) {
				case 'U':
					segtype = KU;
					debug("FOCSEG::set_segtype to KU\n");
					break;
				case 'M':
					segtype = KM;
					debug("FOCSEG::set_segtype to KM\n");
					break;
				case 'L':
					switch (segment_type[2]) {
					case 0:
						segtype = KL;
						debug(
						"FOCSEG::set_segtype to KL\n");
						break;
					case 'U':
						segtype = KLU;
						debug(
						"FOCSEG::set_segtype to KLU\n");
						break;
					default:
						warn("SEG::set_segtype "
						"Bad segtype %s\n",
						segment_type);
						break;
					}
				default:
					warn(
					"SEG::set_segtype Bad segtype %s\n",
					segment_type);
					break;
			}
			break;

		/* DKU, DKM */
		case 'D':
			if (segment_type[1] != 'K') {
				warn("SEG::set_segtype Bad segtype %s\n",
				segment_type);
				break;
			}
			switch (segment_type[2]) {
				case 'U':
					segtype = DKU;
					debug("FOCSEG::set_segtype to DKU\n");
					break;
				case 'M':
					segtype = DKM;
					debug("FOCSEG::set_segtype to DKM\n");
					break;
				default:
					warn(
					"SEG::set_segtype Bad segtype %s\n",
					segment_type);
					break;
			}
			break;

		default:
			warn("SEG::set_segtype Bad segtype %s\n", segment_type);
			break;
	}
}


// Send to beginning of chain
void FOCSEG::reposition(void) {
	debug("SEG::reposition called for seg %s = %d\n",
		segment_name, my_id);

	// The root segment can never be inaccessible when it comes
	// to repositioning. The top record is stored in the page-pointer
	// of the root segment's first page!
	if (cursor_pos == inaccessible && parent_number != 0) {
		die("SEG::reposition seg %s = %d is inaccessible!\n",
			segment_name, my_id);
	}

	// If this is the root segment, then we can simply go to
	// the first record pointed to on our first page.
	if (parent_number == 0) {
		FOCPTR top;
		Page->Parse_page_pointer(first_page, &top);
		cursor_set(top, beginning);
	}
	// go to the beginning of the current chain
	else {
		cursor_rewind();
	}

	debug("SEG::reposition seg %s = %d to page %d word %d type %d\n",
		segment_name, my_id,
		cursor->page, cursor->word, cursor->type);

	set_children_cursor_pos(cursor_pos);
}

void FOCSEG::set_children_cursor_pos(CURSOR_POS position_type) {

	debug("SEG::set_children_cursor_pos entered seg %s = %d\n",
		segment_name, my_id);

	debug_cursor_pos("SEG::set_children_cursor_pos -> ", position_type);

	// Jump out of here quickly if we can.
	if (number_of_children == 0) {
		debug("SEG::set_children_cursor_pos seg %s = %d"
			" has no children\n", segment_name, my_id);
		return;
	}

	int i;
	// If we're at an end or inaccessible, we can't read any pointers!
	if (position_type == end || position_type == inaccessible) {
		for(i = 0; i < number_of_children; i++) {

			debug("SEG::set_children_cursor_pos (end) child %d -> "
				"end\n", i);

			Child[i]->cursor_set_pos(position_type);
		}
	}
	// But if we're at a record or at the beginning, we sure can.
	else {
		FOCPTR child_position;

		for(i = 0; i < number_of_children; i++) {
			Page->Parse_pointer_at_word(cursor->page,
				cursor->word + i, &child_position);

			debug("SEG::set_children_cursor_pos (!end) child %d -> "
				"page %d word %d type %d\n",
				i, child_position.page, child_position.word,
				child_position.type);

			Child[i]->cursor_set(child_position, position_type);
			// Make grandchildren inaccessible
			Child[i]->set_children_cursor_pos(inaccessible);
		}
	}
}

void FOCSEG::cursor_set(FOCPTR &position, CURSOR_POS suggested_pos) {

	debug("SEG::cursor_set entered seg %s = %d\n",
		segment_name, my_id);

	debug_cursor_pos("SEG::cursor_set suggestion ->", suggested_pos);

	*cursor = position;

	if (cursor->type == PTR_EOCHAIN || cursor->type == PTR_NORECORD ||
			cursor->page <= 0 || cursor->word <= 0) {
		debug("SEG::cursor_set found an end\n");
		cursor_set_pos(end);
	}
	else {
		debug("SEG::cursor_set setting pos as suggested\n");
		cursor_set_pos(suggested_pos);
	}
}

void FOCSEG::cursor_set_pos(CURSOR_POS position_type) {

	debug("SEG::cursor_set_pos entered seg %s = %d\n",
		segment_name, my_id);

	debug_cursor_pos("SEG::cursor_set_pos -> ", position_type);

	cursor_pos = position_type;

	if (position_type == beginning) {
		*chain_beginning = *cursor;
		debug("SEG::cursor_set_pos setting chain_beginning to "
			" page %d word %d type %d\n",
			chain_beginning->page, chain_beginning->word,
			chain_beginning->type);
	}
	else if (position_type == inaccessible) {
		chain_beginning->clear();
		debug("SEG::cursor_set_pos clearing chain_beginning\n");
	}
}

void FOCSEG::cursor_rewind(void) {

	debug("SEG::cursor_rewind entered seg %s = %d\n",
		segment_name, my_id);

	if (chain_beginning->is_clear()) {
		die("FOCSEG::cursor_rewind chain_beginning is clear!\n");
	}

	cursor_set(*chain_beginning, beginning);
}


int FOCSEG::next(void) {

	debug("SEG::next entered with seg %s = %d\n", segment_name, my_id);

	// Did programmer mess up?
	if (cursor_pos == inaccessible) {
		die("SEG::next(%s) is not accessible yet. The parent segment"
			" has not been accessed yet.\n", segment_name);
	}

	// Do an alternate next() if we are the child segment in a join
	if (Parent_join) {
		debug("SEG::next Running SEG::Parent_join\n");
		debug("SEG::next leaving with seg = %d\n", my_id);
		return Parent_join->next();
	}

	// Maybe we're at the beginning
	if (cursor_pos == beginning) { 

		debug("SEG::next moving from beginning to record\n");
		cursor_set_pos(record);
	}
	// Go to the next instance in the chain
	else if (cursor_pos == record) {
		debug("SEG::next I'm at a record\n");

		FOCPTR next_record;
		Page->Parse_pointer_at_word(cursor->page,
			cursor->word + number_of_children, &next_record);
		cursor_set(next_record, record);
	}

	// No more instances in this chain; return!
	if (cursor_pos == end) {
		debug("SEG::next I'm already at the end\n");
		set_children_cursor_pos(end);
		debug("SEG::next leaving with seg = %d\n", my_id);
		return 0;
	}
	else {
		set_children_cursor_pos(beginning);
	}

	debug("SEG::next checking for children focfiles\n");
	// next() the children FOCUS files (joins)
	if (Join_list != NULL) {
		debug("SEG::next priming the children focfiles\n");

		FOCJOIN	*join = Join_list;

		while (join != NULL) {

			join->new_key();
			debug("SEG::next join->next_seg\n");
			join = join->next_seg();
		}
	}
	debug("SEG::next leaving with seg = %d\n", my_id);
	return 1;
}


void FOCSEG::next_unique_children(void) {

	debug("SEG::next_unique_children seg %s = %d\n",
		segment_name, my_id);

	FOCSEG	**unique_child;

	for(unique_child = Unique_children;
		*unique_child != NULL;
		unique_child++) {

		debug("SEG::next_unique_children next()ing child.\n");
		(*unique_child)->next();
	}
}

// Make an array of the children that are unique. This makes
// next_unique_children much faster since we don't have to query
// each child during each next.
void FOCSEG::set_unique_children_array(void) {

	int num_unique_children = 0;

	// Count the unique children
	for(int i = 0; i < number_of_children; i++) {

		if (Child[i]->is_unique()) {
			num_unique_children++;
		}
	}

	Unique_children = (FOCSEG**) xmalloc("SEG::set_unique_children",
				sizeof(FOCSEG*) * num_unique_children + 1);

	// Set the unique children
	for(int i = 0; i < number_of_children; i++) {

		if (Child[i]->is_unique()) {
			Unique_children[i] = Child[i];
		}
	}

	Unique_children[num_unique_children] = NULL;
}

// Returns 1 if segment is Unique, 0 if not
int FOCSEG::is_unique(void) {

	if (segtype == U || segtype == KU || segtype == DKU ||
		segtype == KLU) {
		return 1;
	}
	return 0;
}


// At the segment level, we don't need to know the difference
// between a long, double, float, or char*. We just need to read
// bytes. FOCSEG has one read function: read_bytes. It's the higher
// class, FOCFILE, that turns those bytes into data types.
int FOCSEG::read_bytes(UCHAR *target, int offset, int length) {

	UCHAR*	byte_ptr;

	debug("SEG::read_bytes reading page %d word %d offset %d length %d\n",
		cursor->page, cursor->word, offset, length);

	if (cursor_pos == inaccessible) {
		die("SEG::read_bytes(%s) is not accessible yet."
			"The parent segment has not been accessed yet.\n",
				segment_name);
	}

	if (cursor_pos != record) {
		return 0;
	}

	byte_ptr = Page->Return_word_offset(cursor->page,
			cursor->word + number_of_pointers);

	memcpy(target, byte_ptr + offset, length);
	return 1;
}


void FOCSEG::join_segment_as_head(FOCJOIN* new_join) {

	FOCJOIN		*last_join;

	debug("SEG::join_segment_as_head\n");
	if (Join_list == NULL) {
		debug("SEG::join is NULL\n");
		Join_list = new_join;
	}
	else {
		debug("SEG::join is not NULL\n");
		last_join = Join_list->last_seg(Join_list);
		last_join->append_seg_list(new_join);
	}
	debug("SEG::join Segment %d got join %d\n", my_id, new_join->id());
}

void FOCSEG::join_segment_as_child(FOCJOIN* join) {

	debug("SEG::join_segment_as_child seg %d joined as child!\n", my_id);
	Parent_join = join;
}

// Counts the number of records in the segment, starting from
// the top of the segment. It optionally uses a filter() function,
// which should return an integer (usually 0 or 1) that will be added
// to the sum. Since reccount() just adds the result of filter(), filter()
// can return anything: -1, 0, 1, 2, 3, -5... any integer you need.
int FOCSEG::reccount(int (*filter)(FOCFILE*), FOCFILE *foc) {

	int records = 0;

	reposition();
	// no filter function
	if (filter == NULL) {
		while (next()) {
			records++;
		}
	}
	// use the filter function
	else {
		while(next()) {
			records += filter(foc);
		}
	}

	return records;
}



// =============================================================
// CLASS: FOCINDEX
// -------------------------------------------------------------
// Class to contain indices.
// =============================================================
FOCINDEX::FOCINDEX(int idx_num, FILE* fh, UCHAR* fdt_entry) {

	// Terminate the string in NUL. 
	field_name[12] = 0;

	// Store my ID # in case I need it.
	my_id = idx_num;

	foc_fh = fh;

	first_page	= mkshort(&fdt_entry[0]);
	last_page 	= mkshort(&fdt_entry[2]);

	memcpy(field_name, &fdt_entry[4], 12);

	number_of_pages	= mkshort(&fdt_entry[16]);
	index_type	= mkshort(&fdt_entry[18]);
	
	in_use		= 0;

	// No need for a cache until initialization
	Cache		= NULL;
	size_of_key	= 0;

	debug("INDEX::INDEX Index %s has type %i first_page %d "
		"last_page %d # pages %d\n",
		field_name, index_type, first_page, last_page,
		number_of_pages);

};

FOCINDEX::~FOCINDEX() {
	delete Cache;
};


int FOCINDEX::index_in_use(void) {

	return (int) in_use;
}


// =============================================================
// CLASS: FOCINDEX_BTREE
// -------------------------------------------------------------
// Class to handle Balanced-Tree indices.
// =============================================================
FOCINDEX_BTREE::FOCINDEX_BTREE(int idx_num, FILE* fh, UCHAR* fdt_entry) 
	: FOCINDEX(idx_num, fh, fdt_entry) {
	debug("BTREE::Constructor for index %d\n", idx_num);
	Root_node = NULL;
};

FOCINDEX_BTREE::~FOCINDEX_BTREE() {

	debug("BTREE::Destructor for index %d\n", my_id);
	delete Root_node;
};

void FOCINDEX_BTREE::initialize(char type, int seg) {
	debug("BTREE::initialize called: type %c seg %d\n", type, seg);
	in_use = 1;
	Root_node = new FOCINDEX_BTREE_NODE(type, first_page, foc_fh);
	Cache = new FOCINDEXCACHE(Root_node->key_size());
}

int FOCINDEX_BTREE::find(void *key, FOCPTR *result) {
	debug("BTREE::find called\n");
	// we might get lucky and know the answer alredy
	if (Cache->lookup(key, result)) {
		return 1;
	}

	// search successful
	if (Root_node->find(key, result)) {
		debug("BTREE::find found!\n");
		Cache->insert(key, result);
		return 1;
	}
	// search unsuccessful
	else {
		debug("BTREE::find not found!\n");
		return 0;
	}
}

// =============================================================
// CLASS: FOCINDEX_BTREE_NODE
// -------------------------------------------------------------
// Class for the nodes in a Btree
// =============================================================
FOCINDEX_BTREE_NODE::FOCINDEX_BTREE_NODE(char key_type, int node_page,
			FILE *fh) {

	debug("BTREE_NODE::Constructor key %c node_page %d\n",
			key_type, node_page);
	type_of_key = key_type;
	Page = new FOCPAGE(fh);
	read_node_page(node_page);

	if (!is_leaf) {
		debug("BTREE_NODE::This node not leaf. Making new node\n");
		Children_node_level =
			new FOCINDEX_BTREE_NODE(key_type, left_child(), fh);
	}
	else {
		debug("BTREE_NODE::This node is the leaf.\n");
		// Leaves have no children
		Children_node_level = NULL;
	}
};

FOCINDEX_BTREE_NODE::~FOCINDEX_BTREE_NODE() {
	debug("BTREE_NODE::Destructor\n");
	delete Page;
	delete Children_node_level;
}

void FOCINDEX_BTREE_NODE::read_node_page(int node_page) {
	debug("BTREE_NODE::read_node_page page %d\n", node_page);
	// Save some microseconds by checking info in memory
	if (node_page == node_page_in_memory) return;

	UCHAR	*b = Page->Return_byte_offset(node_page, 0);

	is_leaf			= *b;
	parent_node_page	= mkshort(&b[4]);
	right_sibling_node_page	= mkshort(&b[6]);
	size_of_key		= mkshort(&b[8]);
	size_of_record		= mkshort(&b[10]);
	first_free_byte		= mkshort(&b[16]);

	/* If there is only one index page, then it acts like a leaf-page,
	   even though it's marked as a non-leaf page. In the BTREE_NODE,
	   I have no way of reading number_of_pages... but we can figure
	   out the same thing by looking at the size of the record. If the
	   size of the record doesn't contain room for 8 bytes (ptr to
	   child node and ptr to data-page), then we know that it's a leaf
	   node */
	if (  !is_leaf && (size_of_record - size_of_key < 8)) {
		debug("BTREE_NODE::is_leaf is 0, but not enough pointers..."
				" setting is_leaf to 1\n");
		is_leaf = 1;
	}

	debug("BTREE_NODE::read_node_page %d is_leaf %d parent_node %d  "
		"right_sibling_page %d\n", node_page, is_leaf,
		parent_node_page, right_sibling_node_page);

	debug("BTREE_NODE::read_node_page %d size_of_key %d "
		"size_of_record %d first_free %d\n", node_page,
		size_of_key, size_of_record, first_free_byte);

	node_page_in_memory	= node_page;
}


// Looks at the current node-page, and returns the page of the first
// record. 
int FOCINDEX_BTREE_NODE::left_child(void) {
	debug("BTREE_NODE::left_child\n");
	// Leaves don't have children!
	if (is_leaf) die("BTREE_NODE leaf was asked for left_child!\n");

/*	int node = mkshort(Page->Return_byte_offset(node_page_in_memory,
					0x14 + size_of_key + 4));*/

	// Read short at (0x14 + size_of_record - 4) instead of
	// (0x14 + size_of_key + 4) because of the non-leaf nodes that
	// have data-pages as children. This formulat will work for both
	// types of non-leaf nodes. 
	int node = mkshort(Page->Return_byte_offset(node_page_in_memory,
					0x14 + size_of_record - 4));

	debug("BTREE_NODE::left_child on page %d is %d.\n",
		node_page_in_memory, node);
	return node;
}


// Search for the key 
int FOCINDEX_BTREE_NODE::find(void *key, FOCPTR *result, int node_page) {

	debug("BTREE_NODE::find page %d\n", node_page);
	// we're a sub-find()
	if (node_page > 0) {
		read_node_page(node_page);
	}

	// find the key in a leaf node
	if (is_leaf) {
		return find_in_leaf(key, result);
	}

	// or a non-leaf node	
	return find_in_non_leaf(key, result);
}

// look through a non-leaf node to find a sub-node
int FOCINDEX_BTREE_NODE::find_in_non_leaf(void *key, FOCPTR *result) {

	debug("BTREE_NODE::find_in_non_leaf\n");
	int	comparison;

	UCHAR *b;
	UCHAR *start_b	= Page->Return_byte_offset(node_page_in_memory, 0x14);
	UCHAR *end_b	= start_b + first_free_byte - size_of_record;

	debug("BTREE_NODE::find_in_non_leaf type_of_key %c\n", type_of_key);
	for(b = end_b; b >= start_b; b -= size_of_record) {

		// compare correctly
		if (type_of_key == FIELDTYPE_ALPHA) {

			debug("BTREE_NODE::find_in_non_leaf comparing key "
			"%c%c%c%c%c against on-disk %c%c%c%c%c\n",
			((char*)key)[0], ((char*)key)[1], ((char*)key)[2],
			((char*)key)[3], ((char*)key)[4],
			((char*)b)[0], ((char*)b)[1], ((char*)b)[2],
			((char*)b)[3], ((char*)b)[4]);

			comparison = memcmp(key, b, size_of_key);
		}
		else if (type_of_key == FIELDTYPE_INTEGER ||
			 type_of_key == FIELDTYPE_SMDATE) {
			comparison = intcmp((long*)key, (long*)b);
		}
		else if (type_of_key == FIELDTYPE_DOUBLE) {
			comparison = doublecmp((double*)key, (double*)b);
		}
		else if (type_of_key == FIELDTYPE_FLOAT) {
			comparison = floatcmp((float*)key, (float*)b);
		}
		else {
			die("FOCINDEX_BTREE_NODE::find_in_non_leaf "
				"has wrong type %c\n", type_of_key);
		}

		// The key lies in the child
		if (comparison > 0) {
			debug("BTREE_NODE::find_in_non_leaf likes this page\n");
			return Children_node_level->find(key,
				result, mkshort(b + size_of_key + 4));
		}
		// Hit the nail on the head!
		else if (comparison == 0) {
			debug("BTREE_NODE::find_in_non_leaf likes this key\n");
			result->set_location(b + size_of_key);
			return 1;
		}

		// otherwise keep on going
	}

	// The first record in a non-leaf node should be less
	// than all values (NULL). If somehow we made it out of the
	// for() loop without being >= to NULL, then something really
	// strange happened. Let's die if that happened.
	die("INDEX_BTREE_NODE couldn't compare value correctly.\n");
	return 0;	// just here to make compilers happy
}


// check the leaf for the key. if it's not there, there's nowhere else
// to look, so give up.
int FOCINDEX_BTREE_NODE::find_in_leaf(void *key, FOCPTR *result) {

	debug("BTREE_NODE::find_in_leaf\n");
	UCHAR *b;
	UCHAR *start_b	= Page->Return_byte_offset(node_page_in_memory, 0x14);
	UCHAR *end_b	= start_b + first_free_byte;

	for(b = start_b; b < end_b; b += size_of_record) {

		if (memcmp(key, b, size_of_key) == 0) {

			// found it!
			result->set_location(b + size_of_key);
			debug("INDEX::find Found! %c%c%c%c%c%c%c%c "
				"Page %d Word %d\n", b[0], b[1], b[2], b[3],
					b[4],  b[5],  b[6],  b[7],  
					result->page, result->word);
			return 1;
		}
		else {
			debug("INDEX::find Didn't like %c%c%c%c%c%c%c%c\n",
					b[0], b[1], b[2], b[3], b[4],
					b[5], b[6], b[7]);
		}

	}

	// If we make it here, we didn't find the key
	debug("INDEX::find Couldn't find key.\n");
	return 0;
}


// =============================================================
// CLASS: FOCINDEXCACHE
// -------------------------------------------------------------
// Class to maintaine a cache of previous find()'s. This can
// speed up a search dramatically.
// =============================================================
FOCINDEXCACHE::FOCINDEXCACHE(int keysize) {
	size_of_key = keysize;

	cached_position = new FOCPTR();
	cached_key = (UCHAR*) xmalloc("FOCINDEXCACHE constructor", keysize);

};

FOCINDEXCACHE::~FOCINDEXCACHE() {

	delete cached_position;
	free(cached_key);
};

// Returns 1 on success (and moves record), 0 on failure
int FOCINDEXCACHE::lookup(void *key, FOCPTR *position) {

	debug("INDEXCACHE::lookup entered, size_of_key = %d\n",
		size_of_key);
	if (memcmp(key, cached_key, size_of_key) == 0) {
		debug("INDEXCACHE::lookup found in cache!\n");
		*position = *cached_position;
		return 1;
	}
	debug("INDEXCACHE::lookup not found.\n");

	return 0;
}

void FOCINDEXCACHE::insert(void *key, FOCPTR *position) {

	debug("INDEXCACHE::insert entered.\n");
	memcpy(cached_key, key, size_of_key);
	*cached_position = *position;
}



// =============================================================
// CLASS: FOCJOIN
// -------------------------------------------------------------
// Class to handle JOINs between two FOCFILEs
// =============================================================
FOCJOIN::FOCJOIN(FOCSEG* p_seg, int id, int p_offset, int p_length,
		FOCFILE* c_foc, int c_idx, char c_type, int c_seg) {

	debug("JOIN::JOIN creating join %d\n", id);
	// Squirrel away the data
	my_id		= id;
	parent_seg	= p_seg;		// p_seg is FOCSEG*
	parent_offset	= p_offset;
	parent_length	= p_length;

	child_foc	= c_foc;
	child_idx	= c_idx;
	child_type	= c_type;
	child_seg	= c_seg;

	debug("JOIN::JOIN calling foc->join_segment_as_child for seg %d\n", c_seg);
	// Tell child FOC about the join
	c_foc->join_segment_as_child(this, c_seg);

	// Make space for the key
	current_key = (UCHAR*) xmalloc("JOIN: new key",
					sizeof(UCHAR) * p_length);

	first_key_read	= 0;

	// Linked lists
	next_foc_node	= NULL;
	next_seg_node	= NULL;
}

FOCJOIN::~FOCJOIN() {

	free(current_key);
	delete next_foc_node;
}

// Returns the last FOCJOIN in the focus list
FOCJOIN* FOCJOIN::last_foc(FOCJOIN* head) {

	FOCJOIN	*current_join = head;

	while (current_join->next_foc_node != NULL) {
		current_join = current_join->next_foc_node;
	}

	return current_join;
}

// Returns the last FOCJOIN in the segment list
FOCJOIN* FOCJOIN::last_seg(FOCJOIN* head) {

	FOCJOIN	*current_join = head;

	while (current_join->next_seg_node != NULL) {
		current_join = current_join->next_seg_node;
	}

	return current_join;
}

// Appends a JOIN structure after this node, making sure to do
// and insert. Appends to the focus list.
void FOCJOIN::append_foc_list(FOCJOIN *new_node) {

	new_node->next_foc_node = next_foc_node;
	next_foc_node = new_node;
}

// Appends a JOIN structure after this node, making sure to do
// and insert. Appends to segment list
void FOCJOIN::append_seg_list(FOCJOIN *new_node) {

	new_node->next_seg_node = next_seg_node;
	next_seg_node = new_node;
}

void FOCJOIN::new_key(void) {

	first_key_read = 0;

	parent_seg->read_bytes(current_key, parent_offset, parent_length);

	debug("JOIN::new_key Read new_key %c%c%c%c%c\n",
			(char)(current_key[0]),
			(char)(current_key[1]),
			(char)(current_key[2]),
			(char)(current_key[3]),
			(char)(current_key[4]));
}



int FOCJOIN::next(void) {

	debug("JOIN::next entered with first_key_read = %d\n",
		first_key_read);
	/* Is this the first time we are accessing the child? */
	if ( ! first_key_read) {
		first_key_read = 1;
		debug("JOIN::next Reading first key...\n");
		return child_foc->match_index(child_idx, child_type, child_seg,
			current_key);
	}
	else {
		debug("JOIN::next First_key_read...NOP\n");
		// until one-to-many joins are supported, keep
		// the user from doing more than one next() in a child
		// FOCFILE.
		die("JOIN: Can't call next() again! One-to-many joins aren't"
			" supported yet!\n");
	}
	/* for now */
	return 0;
}

// =============================================================
// CLASS: FOCPAGE
// -------------------------------------------------------------
// Class to handle pages, the I/O structure. This is where we
// actually read the data from the file. We don't actually
// allocate the 4K of memory for the page buffer until a read
// is requested.
// =============================================================
FOCPAGE::FOCPAGE(FILE* fh) {

	foc_fh			= fh;
	Page_buffer		= NULL;
	Page_number_in_buffer	= 0;
	Page_pointer		= NULL;
};

FOCPAGE::~FOCPAGE() {

	free(Page_buffer);
	delete Page_pointer;
}

// Returns a pointer to a word in a page (1-indexed).
UCHAR* FOCPAGE::Return_word_offset(int page, int word) {

	// Minimal bounds-checking
	if (word < 1 || word > 1000) {
		die("PAGE Return_word_offset has bad word %d\n", word);
	}

	Read_page(page);
	return Page_buffer + (word - 1) * 4;
}

// Returns a pointer to a byte in a page (0-indexed).
UCHAR* FOCPAGE::Return_byte_offset(int page, int byte) {

	// Minimal bounds-checking
	if (byte < 0 || byte > 3999) {
		die("PAGE: Return_byte_offset bad byte %d\n", byte);
	}

	Read_page(page);
	return Page_buffer + byte;
}

void FOCPAGE::Parse_page_pointer(int page, FOCPTR *result) {

	Read_page(page);

	*result = *Page_pointer;

/*	pointer_type	= Ptr_pointer_type;
	page_index	= Ptr_page_index;
	page_number	= Ptr_page_number;*/
}

void FOCPAGE::Parse_pointer_at_word(int page, int word, FOCPTR *result) {

	UCHAR	*b;

	b = Return_word_offset(page, word);

	debug("PAGE::parse_pointer_at word calling PTR->parse_pointer\n");
	result->parse_pointer(b);

}



// Simply reads a page from the FOC file into the buffer
// Dies on an error
void FOCPAGE::Read_page(int page) {

	// Minimal bounds checking.
	if (page <= 0) {
		die("PAGE: Read_page bad page %d\n", page);
	}

	// If the page is already in the buffer, we lucked out.
	if (page == Page_number_in_buffer) {
		debug("PAGE::Lucky! page %d was already in buffer!\n", page);
		return;
	}
#ifdef DEBUG
	else debug("PAGE::Unlucky! page %d was in the buffer, not %d\n",
		Page_number_in_buffer, page);
#endif /* DEBUG */

	// Do we have a page buffer?
	if (!Page_buffer) {
		// Create the page buffer
		Page_buffer = (UCHAR*) xmalloc("PAGE", 4000);
		Page_pointer = new FOCPTR();
		debug("=========Ack! allocating 4K in memory for page %d"
			" ============\n", page);
	}

	// Position the read-pointer
	if(fseek(foc_fh, (page - 1) * 4096, SEEK_SET) < 0) {
		die("PAGE: fseek returned less-than-zero\n");
	}

	// This debug message is here so that I can verify that
	// my caches do indeed reduce the number of disk reads.
	// Each disk read is expensive.
	debug("*********Ack! reading 4K from disk for page %d************\n",
		page);

	int bytes_read;
	// Read one page of information.
	bytes_read = fread(Page_buffer, 1, 4000, foc_fh);
	if (bytes_read == 4000) {
		Page_number_in_buffer = page;

		// Parse the control info for this page.
		Parse_control();
	}
	else {
		die("PAGE fread returned %d bytes from page %d "
			"instead of 4000\n", bytes_read, page);
	}

}

void FOCPAGE::Parse_control(void) {

//	UCHAR	Focus_pointer[4];

	debug("PAGE::Parse_control\n");
	// Focus_pointer, Date, and Time are really
	// pointers, so don't dereference them!
//	memcpy(Focus_pointer,		&Page_buffer[CTRLOFF],    4);
	memcpy(Date,			&Page_buffer[CTRLOFF+17], 3);
	memcpy(Time,			&Page_buffer[CTRLOFF+20], 4);
	memcpy(&Transaction_number,	&Page_buffer[CTRLOFF+24], 4);

	Next_page		= mkshort(&Page_buffer[CTRLOFF+ 4]);
	Segment_number		= mkshort(&Page_buffer[CTRLOFF+ 6]);
	Last_word		= mkshort(&Page_buffer[CTRLOFF+ 8]);
	Page_number		= mkshort(&Page_buffer[CTRLOFF+10]);
	First_deleted		= mkshort(&Page_buffer[CTRLOFF+12]);
	Free_space		= mkshort(&Page_buffer[CTRLOFF+14]);
	Encryption_flag		= Page_buffer[CTRLOFF+16];

	//Page_pointer->parse_pointer(Focus_pointer);
	debug("PAGE::Parse_control parsing Page pointer PTR->parse_pointer\n");
	Page_pointer->parse_pointer(&Page_buffer[CTRLOFF]);
}




// =============================================================
// CLASS: FOCPTR
// -------------------------------------------------------------
// Class to handle FOCUS pointers. 4 bytes hold 3 variables.
// =============================================================
FOCPTR::FOCPTR() {

	debug("PTR::Constructor (void)\n");
	clear();
};

FOCPTR::FOCPTR(UCHAR *b) {

	debug("PTR::Constructor (UCHAR*) calling parse_pointer\n");
	parse_pointer(b);
};

/* Here's where the magic occurs. We pull out 3 values from a 4-byte
array.
	p = page
	w = word
	t = type

			Byte 0   Byte 1   Byte 2   Byte 3
			-------- -------- -------- --------
	FOCUS 6:	pppppppp pppppppp ttttttww wwwwwwww
	FOCUS 7:  ???	pppppppp pppppppp ppttttww wwwwwwww

	In FOCUS 6, pages can be stored in 16-bit ints. (short)
	In FOCUS 7, pages must be stored in 32-bit ints. (long)
*/
void FOCPTR::parse_pointer(UCHAR* b) {

	UCHAR*  right = b + 2;

	page	= mkshort(b);
	word	= mkshort(right);
	type	= mkshort(right);

	// Move the last 6 bits down to the beginning.
	// ttttttwwwwwwwwww -> 0000000000tttttt
	type >>= 10;

	// Turn off the first 6 bits. 0000001111111111 = 0x3ff;
	//                            ttttttwwwwwwwwww
	word &= 0x3ff;

	debug("PTR::parse_pointer %02x%02x%02x%02x"
		" -> page %d word %d type %d\n",
		b[0], b[1], b[2], b[3], page, word, type);
}

// Set page and word from short int's.
void FOCPTR::set_location(UCHAR* b) {

	page = mkshort(b);
	word = mkshort(b + 2);
	type = 0;
	debug("PTR::set_location %02x%02x%02x%02x"
		" -> page %d word %d type %d\n",
		b[0], b[1], b[2], b[3], page, word, type);
}

void FOCPTR::clear(void) {
	page = 0;
	word = 0;
	type = 0;
}

int FOCPTR::is_clear(void) {
	return (page == 0 && word == 0 && type == 0);
}

// =============================================================
// Extra functions
// =============================================================

// Take the pointer, treat it as a short int, and return an int
int mkshort(UCHAR* ptr) {

	short i = *(short*)ptr;
	return (int)i;

	// the alternative: ptr[0] + ptr[1] * 256
}

// Malloc or die
void* xmalloc(char *label, int bytes) {

	void*	memory;

	if ((memory = (void*) malloc(bytes)) == NULL) {
		die("Can't allocate %d bytes for %s\n", bytes, label);
	}

#ifdef DEBUG
	total_memory_allocated += bytes;
	debug("%%%%%%%% Xmalloc has allocated %d so far.\n",
		total_memory_allocated);
#endif /* DEBUG */

	return memory;
}

// Comparison functions for alpha (not null-terminated strings), longs,
// doubles, and floats. Return -1, 0, or 1:
//
// -1	: a < b
//  0	: a == b
//  1	: a > b
int intcmp(long* a, long* b) {

	if (*a < *b) {
		return -1;
	}
	else if (*a > *b) {
		return 1;
	}

	return 0;
}

int doublecmp(double* a, double* b) {

	if (*a < *b) {
		return -1;
	}
	else if (*a > *b) {
		return 1;
	}

	return 0;
}

int floatcmp(float* a, float* b) {

	if (*a < *b) {
		return -1;
	}
	else if (*a > *b) {
		return 1;
	}

	return 0;
}

// Some libraries don't have strdup. It's a combo malloc and strcpy.
#ifndef HAS_STRDUP
static char* strdup(const char *s) {

	char *new;

	new = (char*) xmalloc(strlen(s) + 1);
	strcpy(new, s);

	return new;
}
#endif /* ! HAS_STRDUP */


#ifdef DEBUG
void debug_cursor_pos(char *s, CURSOR_POS position_type) {
	if (position_type == beginning) {
		debug("%s beginning\n", s);
	}
	else if (position_type == record) {
		debug("%s record\n", s);
	}
	else if (position_type == end) {
		debug("%s end\n", s);
	}
	else if (position_type == inaccessible) {
		debug("%s inaccessible\n", s);
	}
	else {
		debug("%s unknown!\n");
	}
}
#endif /* DEBUG */

/* vi magic 
vi:set ts=8:
vi:set sw=8:
*/
