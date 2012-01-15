// testcar.cpp
// -----------
// An example of how to use focfile.cpp to read a FOCUS file. In this
// example, we read the ubiquitous CAR.FOC file.
//
// Gilbert Ramirez <gram@alumni.rice.edu>

#include <stdio.h>
#include <stdlib.h>
#include "focfile.h"
#include "car.h"

#define die(format, args...) \
	fprintf(stderr, "testcar: " format, ## args); \
	exit(-1);

char*		country;

int main(void) {

	FOCFILE*	foc;
	FILE*		fh;

	char*		car;
	char*		model;
	char*		bodytype;
	long		seats;
	double		dealer_cost;
	char*		warranty;
	char*		standard;

	if (!(fh = fopen("car.foc", "rb"))) {
		die("Can't open car.foc\n");
	}

	// Associate a FOCFILE object with the open filehandle.
	foc = new FOCFILE(FOCFILE_CAR, fh);

	// Allocate space for the strings.
	country		= foc->string_alloc(FOCFLD_CAR_COUNTRY);
	car			= foc->string_alloc(FOCFLD_CAR_CAR);
	model		= foc->string_alloc(FOCFLD_CAR_MODEL);
	bodytype	= foc->string_alloc(FOCFLD_CAR_BODYTYPE);
	warranty	= foc->string_alloc(FOCFLD_CAR_WARRANTY);
	standard	= foc->string_alloc(FOCFLD_CAR_STANDARD);

	foc->reposition(FOCSEG_CAR_ORIGIN);

	while(foc->next(FOCSEG_CAR_ORIGIN)) {

		foc->hold(country, FOCFLD_CAR_COUNTRY);
		printf("Country (%s)\n", country);

		while(foc->next(FOCSEG_CAR_COMP)) {

			foc->hold(car, FOCFLD_CAR_CAR);
			printf("   Car (%s)\n", car);

			while(foc->next(FOCSEG_CAR_CARREC)) {

				foc->hold(model, FOCFLD_CAR_MODEL);
				printf("      Model (%s)\n", model);

				while(foc->next(FOCSEG_CAR_BODY)) {

					foc->hold(bodytype, FOCFLD_CAR_BODYTYPE);
					foc->hold(seats, FOCFLD_CAR_SEATS);
					foc->hold(dealer_cost, FOCFLD_CAR_DEALER_COST);
					printf("         Body (%s) Seats "
						FOCFMT_CAR_SEATS " Dcost "
						FOCFMT_CAR_DEALER_COST "\n",
						bodytype, seats, dealer_cost);
				}
			}

			while(foc->next(FOCSEG_CAR_WARANT)) {

				foc->hold(warranty, FOCFLD_CAR_WARRANTY);
				printf("    Warranty (%s)\n", warranty);
			}


			while(foc->next(FOCSEG_CAR_EQUIP)) {

				foc->hold(standard, FOCFLD_CAR_STANDARD);
				printf("    Standard (%s)\n", standard);
			}
		}
	}

	printf("The root segment as %d records\n", foc->reccount());

	delete foc;
	fclose(fh);

	return 0;
}

