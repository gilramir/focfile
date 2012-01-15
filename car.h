
#define FOCSEG_CAR_ORIGIN		1
#define FOCFLD_CAR_COUNTRY		1,0,'A',10
#define FOCFMT_CAR_COUNTRY		"%10s"

#define FOCSEG_CAR_COMP		2
#define FOCFLD_CAR_CAR		2,0,'A',16
#define FOCFMT_CAR_CAR		"%16s"

#define FOCSEG_CAR_CARREC		3
#define FOCFLD_CAR_MODEL		3,0,'A',24
#define FOCFMT_CAR_MODEL		"%24s"

#define FOCSEG_CAR_BODY		4
#define FOCFLD_CAR_BODYTYPE		4,0,'A',12
#define FOCFMT_CAR_BODYTYPE		"%12s"
#define FOCFLD_CAR_SEATS		4,12,'I',4
#define FOCFMT_CAR_SEATS		"%3ld"
#define FOCFLD_CAR_DEALER_COST		4,16,'D',8
#define FOCFMT_CAR_DEALER_COST		"%7.0lf"
#define FOCFLD_CAR_RETAIL_COST		4,24,'D',8
#define FOCFMT_CAR_RETAIL_COST		"%7.0lf"
#define FOCFLD_CAR_SALES		4,32,'I',4
#define FOCFMT_CAR_SALES		"%6ld"

#define FOCSEG_CAR_SPECS		5
#define FOCFLD_CAR_LENGTH		5,0,'D',8
#define FOCFMT_CAR_LENGTH		"%5.0lf"
#define FOCFLD_CAR_WIDTH		5,8,'D',8
#define FOCFMT_CAR_WIDTH		"%5.0lf"
#define FOCFLD_CAR_HEIGHT		5,16,'D',8
#define FOCFMT_CAR_HEIGHT		"%5.0lf"
#define FOCFLD_CAR_WEIGHT		5,24,'D',8
#define FOCFMT_CAR_WEIGHT		"%6.0lf"
#define FOCFLD_CAR_WHEELBASE		5,32,'D',8
#define FOCFMT_CAR_WHEELBASE		"%6.1lf"
#define FOCFLD_CAR_FUEL_CAP		5,40,'D',8
#define FOCFMT_CAR_FUEL_CAP		"%6.1lf"
#define FOCFLD_CAR_BHP		5,48,'D',8
#define FOCFMT_CAR_BHP		"%6.0lf"
#define FOCFLD_CAR_RPM		5,56,'I',4
#define FOCFMT_CAR_RPM		"%5ld"
#define FOCFLD_CAR_MPG		5,60,'D',8
#define FOCFMT_CAR_MPG		"%6.0lf"
#define FOCFLD_CAR_ACCEL		5,68,'D',8
#define FOCFMT_CAR_ACCEL		"%6.0lf"

#define FOCSEG_CAR_WARANT		6
#define FOCFLD_CAR_WARRANTY		6,0,'A',40
#define FOCFMT_CAR_WARRANTY		"%40s"

#define FOCSEG_CAR_EQUIP		7
#define FOCFLD_CAR_STANDARD		7,0,'A',40
#define FOCFMT_CAR_STANDARD		"%40s"

#define FOCFILE_CAR		"s1 tS1 s2 tS1 s3 tS1 s4 tS1 s5 tU s6 tS1 s7 tS1 "