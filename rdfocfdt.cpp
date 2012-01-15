/*
    rdfocfdt
    --------
    Reads a FOCUS file Field Definition Table and prints
    the fields and indices in their FOCUS-created order.

    *********************************************************
    This library is in no way related to or supported by IBI.
    IBI's FOCUS is a proprietary database whose format may
    change at any time. If you have any problems, suggestions,
    or comments about the FocFile C++ library, contact
    the author, not Information Builders!
    *********************************************************

    Copyright (C) 1997  Gilbert Ramirez <gram@alumni.rice.edu>
    $Id: rdfocfdt.cpp,v 1.3 1997/03/02 22:50:48 gram Exp $

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
#include "focfile.h"

#define die(format, args...) \
	fprintf(stderr, "rdfocfdt: " format, ## args); \
	exit(-1)

void Process_file(char *name);

int main(int argc, char **argv) {

	if (argc < 1) {
		die("Must supply the name of a FOCUS file.\n");
		exit(-1);
	}

	for(int i = 1; i < argc; i++) {

		Process_file(argv[i]);
	}

	return 0;
}

void Process_file(char *name) {

	FILE	*file;
	FOCFILE	*foc;

	if (!(file = fopen(name, "rb"))) {
		die("can't open ddentry.foc\n");
	}

	foc = new FOCFILE(file);

	int num_segs = foc->number_seg();
	int num_idxs = foc->number_idx();

	char *entry = (char*) malloc(65);

	for(int i = 1; i <= num_segs; i++) {

		foc->segment_name(entry, i);
		printf("SEG %d %s\n", i, entry);
	}


	for(int i = 1; i <= num_idxs; i++) {

		foc->index_name(entry, i);
		printf("IDX %d %s\n", i, entry);
	}

	delete foc;
	fclose(file);
}
