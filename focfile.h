/*
    focfile.h
    ---------
    C++ class to read Intel-based FOCUS files (PC/FOCUS 6.01)
    FOCUS and PC/FOCUS are trademarks of Information Builders, Inc. (IBI)

    *********************************************************
    This library is in no way related to or supported by IBI.
    IBI's FOCUS is a proprietary database whose format may
    change at any time. If you have any problems, suggestions,
    or comments about the FocFile C++ library, contact
    the author, not Information Builders!
    *********************************************************

    Copyright (C) 1997  Gilbert Ramirez <gram@alumni.rice.edu>
    $Id: focfile.h,v 1.11 1997/04/03 05:14:11 gram Exp $

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


#ifndef FOCFILE_H
#define FOCFILE_H

#include <stdio.h>

#ifndef SMDATE_H
#include "smdate.h"
#endif /* SMDATE_H */

// This is the one class available to the programmer.
class FOCFILE;

// These are used internally.
class FOCSEG;
class FOCINDEX;
class FOCINDEX_BTREE;
class FOCINDEX_BTREE_NODE;
//class FOCINDEX_HASH;	// not available yet
class FOCINDEXCACHE;
class FOCJOIN;
class FOCPAGE;
class FOCPTR;

typedef unsigned char UCHAR;	// unsigned character (byte!)

// This gives the programmer one class to deal with. It controls one FOCUS
// file, and will move the cursor in any children FOCUS files that are
// joined to it.
//
// Static cross-references are not supported yet.
//
// Pass the constructor an fopen()'ed file-handle (FILE*).
// After destruction, you have to explicitly fclose() the file-handle
// that you opened. 
class FOCFILE {

public:
	FOCFILE(char *mfd_string, FILE *fh);
	FOCFILE(FILE *fh);
	~FOCFILE();

	// Non-index functions
	void	reposition(int seg=0, int offset=0,
				char type='?', int length=0);
	int	next(int seg=0, int offset=0, char type='?', int length=0);
	int	next_with_uniques(int seg=0, int offset=0, char type='?',
			int length=0);
private:
	int	match(int seg, int offset, char type, int length, void* key);
public:
	int	match(int seg, int offset, char type, int length, char* key);
	int	match(int seg, int offset, char type, int length, long& key);
	int	match(int seg, int offset, char type, int length, double& key);
	int	match(int seg, int offset, char type, int length, float& key);
	int	match(int seg, int offset, char type, int length, SMDATE& key);

	int	match_with_uniques(int seg, int offset, char type,
			int length, void* key);

	// Index-related functions
private:
	int	find(int idx, char type, int seg, void* key);
public:
	int	find(int idx, char type, int seg, char* key);
	int	find(int idx, char type, int seg, long& key);
	int	find(int idx, char type, int seg, double& key);
	int	find(int idx, char type, int seg, float& key);
	int	find(int idx, char type, int seg, SMDATE& key);

	// Join a field in the Parent segment to a field in a Child FOCFILE
	int	join(int p_seg, int p_offset, char p_type, int p_length,
			FOCFILE* c_foc,
			int c_idx, char c_type, int c_seg);
	int	join_clear(int join_number=0);

	// Allocate space in the heap for alphanumeric fields
	// which will be stored in C-style (0-terminated) strings
	char* string_alloc(int seg, int offset=0, char type=0, int length=0);
	char* string_alloc(
		int a_seg, int a_offset, char a_type, int a_length,
		int b_seg, int b_offset, char b_type, int b_length);


	// Read fields and store them in user-space.
	int hold(char *s,   	int seg, int offset, char type, int length);
	int hold(long &l,   	int seg, int offset, char type, int length);
	int hold(double &d, 	int seg, int offset, char type, int length);
	int hold(float &f,  	int seg, int offset, char type, int length);
	int hold(SMDATE &smd,	int seg, int offset, char type, int length);

	int hold(char *s,
			int a_seg, int a_offset, char a_type, int a_length,
			int b_seg, int b_offset, char b_type, int b_length);

	int hold(UCHAR *b,
			int a_seg, int a_offset, char a_type, int a_length,
			int b_seg, int b_offset, char b_type, int b_length);

	// Goodies
	int reccount(int (*filter)(FOCFILE*));
	int reccount(int seg=0, int (*filter)(FOCFILE*)=NULL);
	int reccount(int seg, int offset, char type, int length,
			int (*filter)(FOCFILE*)=NULL);

	/* ---------------------------------------------------------- */

	// Miscellaneous, used mostly by other methods/classes
	int read_bytes(UCHAR* b, int seg, int offset, char type, int length);
	void join_segment_as_child(FOCJOIN* join, int seg);
	void clear_joined_segment(int seg);
	void initialize_index(int idx, char type, int seg);
	int  index_in_use(int idx);
	int  match_index(int idx, char type, int seg, void* key);

	int number_seg(void) { return Num_segments; };
	int number_idx(void) { return Num_indices; };
	void segment_name(char* answer, int seg);
	void index_name(char* answer, int idx);


private:
	void Parse_fdt(FILE *fh);
	void Parse_fdt_seg(FILE *fh, UCHAR *buffer);
	void Parse_fdt_idx(FILE *fh, UCHAR *buffer);
	void Parse_mfd(char *mfd_string);
	int FDT_index_type(UCHAR *idx_fdt_entry);
private:
	int		Num_segments;
	int		Num_indices;
	FOCSEG		*Root_segment; // For convenience

	// Arrays of structures
	FOCSEG		**Segment;
	FOCINDEX	**Index;

	// JOINs
	FOCJOIN		*Join_list;	// Linked list of children joins
};


// Used in next()-ing a segment
// ----------------------------
// beginning	:	At beginning of segment, before all records
// record	:	Currently on a record
// end		:	End of segment, after all records
// inaccessible	:	Not 'record', and not 'primed'... nothing!
enum CURSOR_POS { beginning, record, end, inaccessible };

enum SEGTYPE { Sn, SHn, S0, b, U, KU, KM, DKU, DKM, KL, KLU, unknown };

// Most of the actual work happens at the segment level. Each segment
// in the FOCUS file (more or less a relational table) requires an instance
// of this FOCSEG class. This class does the work of reading fields
// and advancing the current-record pointer.
class FOCSEG {

public:
	FOCSEG(int seg_num, FILE* fh, UCHAR* fdt_entry);
	~FOCSEG();

	void	Add_child_pointer(FOCSEG* new_child);
	void	set_segtype(char *segment_type);
	int	Get_parent(void) { return parent_number; };

	int	next(void);
	void	reposition(void);
	void	next_unique_children(void);
	int	is_unique(void);
	void	set_unique_children_array(void);

	int	read_bytes(UCHAR *target, int offset, int length);

	void	cursor_set(FOCPTR &position, CURSOR_POS suggested_pos);
	void	cursor_set_pos(CURSOR_POS position_type);
	void	set_children_cursor_pos(CURSOR_POS position_type);

	void	join_segment_as_head(FOCJOIN* new_join);
	void	join_segment_as_child(FOCJOIN* join);

	int	reccount(int (*filter)(FOCFILE*), FOCFILE *foc);
	char*	Segment_name(void) { return segment_name; };

private:
	void	cursor_rewind(void);

private:
	int	my_id;
	FOCPAGE	*Page;
	FOCSEG	**Child;
	FOCSEG	**Unique_children;

	int	first_page;
	int	last_page;
	char	segment_name[9];
	int	segment_length;
	int	parent_number;
	int	number_of_pages;
	int	number_of_pointers;

	// current-record pointer (the cursor)
	// These three fields are very related and really should be
	// moved out into some sort of FOCCURSOR class. However, for
	// speed reasons I'll keep them as separat fields in
	// FOCSEG.
	FOCPTR		*cursor;
	CURSOR_POS	cursor_pos;		// Position in segment
	FOCPTR		*chain_beginning;

	FOCJOIN		*Join_list;		// Children FOCUS files
	FOCJOIN		*Parent_join;		// One parent join
	int		number_of_children;	// Children segments

	SEGTYPE		segtype;
};



// Manages FOCUS index information. Indices are different on each platform
// of FOCUS, and a single platform may have different types of indices.
// The FOCINDEX class is a base class for the different types of indices.
// So far I only know of two types of FOCUS indices, hash and btree.
class FOCINDEX {

public:
	FOCINDEX(int idx_num, FILE* fh, UCHAR* fdt_entry);
	~FOCINDEX();

	int		index_in_use(void);
	virtual void	initialize(char type, int seg) = 0;
	virtual int	find(void *key, FOCPTR *position) = 0;
	char*		Index_name(void) { return field_name; };

protected:
	int	my_id;
	char	my_type;
	FILE	*foc_fh;

	int	first_page;
	int	last_page;
	char	field_name[13];
	int	number_of_pages;
	int	index_type;

	char		in_use;	// Has this index been initialized?
	FOCINDEXCACHE	*Cache;
	int		size_of_key;
};



// PC/FOCUS 6.01 uses Btree indices. The "Balanced Tree" index structure
// was developed in 1970 by R. Bayer and E. McCreight, according to
// Al Stevens in "C Database Development", 2nd ed., published by MIS Press
// in 1991, Portland Oregon.
// For more information about the FOCUS Btrees, read the comments in
// the appropriate section in focfile.cpp
class FOCINDEX_BTREE : public FOCINDEX {

public:
	FOCINDEX_BTREE(int idx_num, FILE* fh, UCHAR* fdt_entry);
	~FOCINDEX_BTREE();

	void	initialize(char type, int seg);
	int	find(void *key, FOCPTR *position);

private:
	FOCINDEX_BTREE_NODE	*Root_node;

};

/* Instead of allocating a structure per node, I'll
   allocate a class per node-level. The nodes are 4K long... too big
   to allocate memory for each one!
   Each node is a FOCUS page, so I'll call nodes 'node_pages'. */
class FOCINDEX_BTREE_NODE {

public:
	FOCINDEX_BTREE_NODE(char key_type, int node_page, FILE *fh);
	~FOCINDEX_BTREE_NODE();

	int find(void *key, FOCPTR *result, int node_page=0);
	int find_in_non_leaf(void *key, FOCPTR *result);
	int find_in_leaf(void *key, FOCPTR *result);
	int key_size(void) { return size_of_key; };
	
private:
	void	read_node_page(int node_page);
	int	left_child(void);

private:
	UCHAR	is_leaf;
	int	parent_node_page;
	int	right_sibling_node_page;
	int	size_of_key;
	int	size_of_record;
	int	first_free_byte;
	char	type_of_key;

	FOCPAGE	*Page;
	int	node_page_in_memory;

	FOCINDEX_BTREE_NODE	*Children_node_level;

};

/* Some day ... hashes are good, but you need to know the hashing function!
class FOCINDEX_HASH : public FOCINDEX {

}; */

// When using an index, I've noticed that in many reports the same key is
// searched for within a short period of time. Therefore, a small cache
// will speed things up considerably. This may be a waste in some data
// extraction programs, so I'll keep the cache very small.
class FOCINDEXCACHE {

public:
	FOCINDEXCACHE(int keysize);
	~FOCINDEXCACHE();

	int	lookup(void *key, FOCPTR *position);
	void	insert(void *key, FOCPTR *position);

// Right now we only have a one-record cache. It works well for me!
private:
	FOCPTR	*cached_position;
	UCHAR	*cached_key;
	int	size_of_key;
};


// This holds information about a JOIN between two FOCFILE objects
// It assumes a linked-list of FOCJOIN structures. There probably
// won't be more than 10 JOINs in effect for a given FOCFILE, so
// a linked-list won't slow us down.
class FOCJOIN {

public:
	FOCJOIN(FOCSEG* p_seg, int id, int p_offset, int p_length,
		FOCFILE* c_foc, int c_idx, char c_type, int c_seg);
	~FOCJOIN();

	FOCJOIN*	last_foc(FOCJOIN *head);
	FOCJOIN*	last_seg(FOCJOIN *head);

	FOCJOIN*	next_foc(void) { return next_foc_node; };
	FOCJOIN*	next_seg(void) { return next_seg_node; };

	int	id(void) { return my_id; };
	void	append_foc_list(FOCJOIN* new_node);
	void	append_seg_list(FOCJOIN* new_node);
	void	new_key(void);
	int	next(void);

private:
	// There are two linked lists that we reside on. The FOCFILE
	// linked list keeps track of all joins in which the FOCFILE
	// is the parent. The segment keeps track of all joins in which
	// the segment is the parent.
	FOCJOIN*	next_foc_node;
	FOCJOIN*	next_seg_node;

	// Data
	int		my_id;
	UCHAR*		current_key;
	UCHAR*		old_key;	// used to compare against new key

	FOCSEG*		parent_seg;
	int		parent_offset;
	int		parent_length;

	FOCFILE*	child_foc;
	int		child_idx;
	char		child_type;
	int		child_seg;
	// Flags
	int		first_key_read; // Has the first child rec been read?
};

// The FOCPAGE class read pages from the FOC file. Each page is 4096 bytes,
// but the buffer is only 4000 bytes, since the last 96 bytes are unused.
// All the higher clases (FOCFILE, FOCSEG, and FOCINDEX)
// make heavy use of FOCPAGE.
class FOCPAGE {

public:
	FOCPAGE(FILE* fh);
	~FOCPAGE();

	UCHAR*	Return_word_offset(int page, int word);
	UCHAR*	Return_byte_offset(int page, int byte);

	void	Parse_page_pointer(int page, FOCPTR *result);
	void	Parse_pointer_at_word(int page, int word, FOCPTR *result);

private:
	void	Read_page(int page);	// Loads page into buffer
	void	Parse_control(void);	// Parses control info in buffer


private:
	UCHAR	*Page_buffer;		// buffer to store page data
	int	Page_number_in_buffer;	// page currently in buffer
	FILE*	foc_fh;

	// Page information for page in buffer
	int	Next_page;
	int	Segment_number;
	int	Last_word;
	int	Page_number;
	int	First_deleted;
	int	Free_space;
	UCHAR	Encryption_flag; 
	UCHAR	Date[3];	// I don't know the format of Date
	UCHAR	Time[4];	// or Time...
	long	Transaction_number;

	// The members of the page's focus pointer
	FOCPTR	*Page_pointer;
};



// Handles the magical FOCUS pointers: 4 bytes which hold 3 variables.
class FOCPTR {

public:
	FOCPTR();		// O=old, N=new
	FOCPTR(UCHAR *b);		

	void	parse_pointer(UCHAR *b);
	void	set_location(UCHAR *b);
	inline void	clear(void);
	inline int	is_clear(void);

	// Yes, the following should be private variables with public
	// access methods. But I use these variables so often that I'd
	// rather not have the overhead of a function-call. I just
	// have to trust myself not to change the values of these
	// class-members from outside the class!
public:
	int	type;	// Type of FOCUS pointer (1-11)
	int	page;	// Page in FOCUS file (1-65536, or 1-262144)
	int	word;	// Word on page (1-4000)

};


#endif /* FOCFILE_H */

/* magic settings for vi editors
vi:set ts=8:
vi:set sw=8:
*/
