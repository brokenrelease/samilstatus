/*
COPYWRIGHT 2015 Francis Olejnik
    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.


SAMILSTATUS

This program retrieves & displays data from Samil Power models of inverters over a serial port.

Usage:  [-v] -n name -d device -s serialnumber
					
	-v is optional for verbose output whilst debugging
	-n is the inverter name prefix (required: handy if your connecting to more than one inverter)
	-d is the serial port device name that the inverter is connected to (required)
	-s is the inverters serial number (required for inverter login)
	
	Example: -v -n Samil1 -d /dev/ttyUSB0 -s S33114L133\n	

BY:			Francis Olejnik
DATE:		31-January-2015
Version: 	201501311110
*/


#include <stdio.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <limits.h>
#include <time.h> 
#include <stdint.h>
#include <string.h>
#include <getopt.h>
#include <stdlib.h>

//Function prototypes
void print_formatted_hex_array(char *buffer, int arraysize, int rowlength, int highlightstart, int highlightcount);
uint16_t checksum(const char *data_p, int nBytes);
void millisleep(int milliseconds);
void parse_print_data_point(char *buffer, int datapoint_bufferoffset, int datapoint_size, char *inverter_name_description, \
							char *point_description, char *engineering_units, int is_decimal, float decimal_multiplier, int verboseflag);




//MAIN - This is where the magic starts
int main(int argc, char **argv)
{

//Options Variables
extern char *optarg;
extern int optind;
int c, err = 0;
int verboseflag=0, nameprefixflag=0, deviceportflag=0, serialnumberflag=0;
char *deviceport, *serialnumber, *nameprefix;
static char usage[] = 	"Usage:  %s [-v] -n name -d device -s serialnumber\n\n"
						"-v is optional for verbose output whilst debugging\n"
						"-n is the inverter name prefix (required: handy if your connecting to more than one inverter)\n"
						"-d is the serial port device name that the inverter is connected to (required)\n"
						"-s is the inverters serial number (required for inverter login)\n\n"
						"Example: %s -v -n Samil1 -d /dev/ttyUSB0 -s S33114L133\n";					


while ((c = getopt(argc, argv, "vn:d:s:")) != -1)
	switch (c) {
	case 'v':
		verboseflag = 1;
		break;
	case 'n':
		nameprefixflag = 1;
		nameprefix = optarg;
		break;
	case 'd':
		deviceportflag=1;
		deviceport = optarg;
		break;
	case 's':
		serialnumberflag = 1;
		serialnumber = optarg;
		break;
	case '?':
		err = 1;
		break;
	}

//nameprefix is a mandatory arg
if (nameprefixflag == 0) {
	fprintf(stderr, "%s missing -n option\n", argv[0]);
	fprintf(stderr, usage, argv[0]);
	exit(1);
//deviceport is a mandatory arg
} else if (deviceportflag == 0) {
	fprintf(stderr, "%s missing -d option\n", argv[0]);
	fprintf(stderr, usage, argv[0]);
	exit(1);
//serialnumber is a mandatory arg
} else if (serialnumberflag == 0) {
	fprintf(stderr, "%s missing -s option\n", argv[0]);
	fprintf(stderr, usage, argv[0]);
	exit(1);
//invalid option
} else if (err) {
	fprintf(stderr, usage, argv[0]);
	exit(1);
}


//Print out the options
if (verboseflag){
	printf("--------------Input arguments--------------\n");
	printf("verbose      = %d\n", verboseflag);
	printf("device       = \"%s\"\n", deviceport);
	printf("serialnumber = \"%s\"\n", serialnumber);
	printf("-------------------------------------------\n\n");
}


//Open the serialport
struct termios options;

int rt_fd;

rt_fd = open(deviceport, O_RDWR | O_NOCTTY | O_NONBLOCK);
	if (rt_fd == -1) {
	// Exit if the specified serial port cannot be found
	fprintf(stderr, "unable to open \"%s\"\n", deviceport);
    return(1);
	}
	
tcgetattr(rt_fd, &options);
bzero(&options, sizeof(options));

options.c_cflag = B9600 | CS8 | CLOCAL | CREAD;

tcsetattr(rt_fd, TCSANOW, &options);


//Write to the serialport

int writecount;

char inv_querystring[] = {0x55, 0xAA, 0x00, 0x00, 0x00, 0x33, 0x01, 0x02, 0x00, 0x01, 0x35};
char inv_initstring[] =  {0x55, 0xAA, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF};
char inv_loginstring[] = {0x55, 0xAA, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x0A};


//Write the initialization string to the inverter
writecount = write(rt_fd, inv_initstring, sizeof(inv_initstring));

//wait 100 milliseconds for a reply
millisleep(100);

//Read inverters reply from the Serialport.
//We dont care what the reply is at this stage, we just want to flush the receive buffer.
int readcount;
unsigned char rx_buffer[255];
readcount = read(rt_fd, rx_buffer, sizeof(rx_buffer));


//LOGIN
unsigned char checksumbytes[2]; //2 bytes for the checksum part of the message
uint16_t checksumtally=0; //Tally checksome as we go from loginstring & serialnumber string
checksumtally = checksum(inv_loginstring, sizeof(inv_loginstring));
checksumtally = checksumtally + checksum(serialnumber, strlen(serialnumber));

checksumbytes[0] = checksumtally>>8;
checksumbytes[1] = checksumtally;

writecount = write(rt_fd, inv_loginstring, sizeof(inv_loginstring));
writecount = write(rt_fd, serialnumber, strlen(serialnumber));
writecount = write(rt_fd, checksumbytes, sizeof(checksumbytes));
millisleep(100);

readcount = read(rt_fd, rx_buffer, sizeof(rx_buffer));


//Query
writecount = write(rt_fd, inv_querystring, sizeof(inv_querystring));

if (verboseflag){
	printf("------------------TX DATA------------------\n");
	printf("INIT, LOGIN & QUERY data sent to inverter\n", verboseflag);
	printf("Checksum value is %.4X hex (%d) decimal\n", checksumtally, checksumtally);
	printf("Login Message Checksum High Byte = %.2X\n", checksumbytes[0]);
	printf("Login Message Checksum Low Byte  = %.2X\n", checksumbytes[1]);
	printf("-------------------------------------------\n\n");
}


millisleep(100);
readcount = read(rt_fd, rx_buffer, sizeof(rx_buffer));

if (verboseflag){
	//print the received byte count message & data
	printf("------------------RX DATA------------------\n");
	printf("%d Bytes received from inverter\n", readcount);
	print_formatted_hex_array(rx_buffer, 51, 16, 0, 0);
	printf("-------------------------------------------\n\n");
}





//Close the serialport
close(rt_fd);

//DECODE OUTPUT

if (verboseflag){
	printf("--------FORMAT & DECODE DATA POINTS--------\n");
}

//Do date & timestamp first
char outstr[10];
char format_date[] = "%Y%m%d";
time_t t;
struct tm *tmp;
t = time(NULL);
tmp = localtime(&t);

strftime(outstr, sizeof(outstr), format_date, tmp);

printf("%s_DateStamp=%s\n", nameprefix, outstr);

char format_time[] = "%H:%M";

strftime(outstr, sizeof(outstr), format_time, tmp);

printf("%s_TimeStamp=%s\n", nameprefix, outstr);


//Test data, Comment out for live system, this is used for debugging at night when theres no sun to turn on the inverter!!
/*readcount = 51;
unsigned char test_data[] = {	0x55, 0xAA, 0x00, 0x33, 0x00, 0x00, 0x01, 0x82, 0x28, 0x01, 0xF8, 0x0A, 0x2D, 0x00, 0x24, 0x00, \
								0x00, 0x3A, 0xA1, 0x01, 0x03, 0x02, 0xFD, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, \
								0x00, 0x00, 0x2A, 0x09, 0x93, 0x13, 0x8B, 0x03, 0xF1, 0x00, 0x01, 0x3F, 0x3B, 0x00, 0x00, 0x00, \ 
								0x00, 0x07, 0xE2};
memcpy(rx_buffer, test_data, sizeof(test_data));*/


//A valid output response from the inverter has 51 bytes so lets check
if (readcount != 51) {
	//If the response isnt 51 bytes long then exit
	printf("%s_InverterOnline=0\n", nameprefix);
	return (1);
	}


printf("%s_InverterOnline=1\n", nameprefix);

uint16_t parseVar; //2 byte container
float engval; //engineering value container
const OFFSET = 8; //count off 9 items in the array 0-8
int msgptr = 0;


/*
Example contents of rx data buffer for a valid inverter status response

(00) 55 AA 00 33 00 00 01 82 28 01 F8 0A 2D 00 24 00
(16) 00 3A A1 01 03 02 FD 00 00 00 00 00 00 00 00 00
(32) 00 00 2A 09 93 13 8B 03 F1 00 01 3F 3B 00 00 00
(48) 00 07 E2

BYTES  0 to  8
BYTES  9 &  10 Inverter Temp,         0.1 deg C per Unit
BYTES 11 &  12 Panel1 Volts,          100mV per Unit
BYTES 13 &  14 Panel1 Current,        100mA per Unit
BYTES 15 to 20
BYTES 21 &  22 Todays Energy,         10 Watt Hours per Unit
BYTES 23 to 32
BYTES 33 &  34 Grid Current,          100mA per Unit
BYTES 35 &  36 Grid Volts,            100mV per Unit   
BYTES 37 &  38 Grid Frequency,        0.01 Hertz per Unit
BYTES 39 &  40 Instantaneous Power,   1 Watt per Unit
BYTES 41 to 44 Lifetime Total Energy, 0.1 Kilowatt Hours per Unit
BYTES 44 to 50
*/

parse_print_data_point(rx_buffer,  9, 2, nameprefix, "Inverter_Temperature", "C", 1, 0.1, verboseflag);
parse_print_data_point(rx_buffer, 11, 2, nameprefix, "Panel1_Volts", "V", 1, 0.1, verboseflag);
parse_print_data_point(rx_buffer, 13, 2, nameprefix, "Panel1_Current", "A", 1, 0.1, verboseflag);
parse_print_data_point(rx_buffer, 21, 2, nameprefix, "Todays_Energy", "Wh", 1, 10, verboseflag); 
parse_print_data_point(rx_buffer, 33, 2, nameprefix, "Grid_Current", "A", 1, 0.1, verboseflag);
parse_print_data_point(rx_buffer, 35, 2, nameprefix, "Grid_Volts", "V", 1, 0.1, verboseflag);
parse_print_data_point(rx_buffer, 37, 2, nameprefix, "Grid_Frequency", "Hz", 1, 0.01, verboseflag);
parse_print_data_point(rx_buffer, 39, 2, nameprefix, "Instantaneous_Power", "W", 1, 1, verboseflag);
parse_print_data_point(rx_buffer, 41, 4, nameprefix, "Total_Energy", "kWh", 1, 0.1, verboseflag);

if (verboseflag){
	printf("-------------------------------------------\n");
	}
}



//Functions
void print_formatted_hex_array(char *buffer, int arraysize, int rowlength, int highlightstart, int highlightcount){
#define ANSI_REVERSE_VIDEO_ON  "\033[7m"
#define ANSI_REVERSE_VIDEO_OFF "\033[27m"
int rowCounter =0 , columnCounter = 0, bufferindex = 0;
while (rowCounter < (arraysize*1.0/(rowlength))){ 
 //The multiplication of arraysize by 1.0 casts arraysize to a float so the division works properly
 //Id like to re jig this function to do this purely with integer maths
 printf("(%.4d) ", rowCounter*rowlength);
 columnCounter = 0;
 rowCounter++;
 while (columnCounter < (rowlength)){
  if (bufferindex <= arraysize){
   if ((bufferindex >= highlightstart) && (bufferindex < (highlightstart + highlightcount))){
	printf("\x1b[07m%.2X\x1b[27m ", buffer[bufferindex]);
   } else {
    printf("%.2X ", buffer[bufferindex]);
   }	
  }
  bufferindex++;
  columnCounter++;
  }
 printf("\n");
 }
}


//quick & dirty checksum calculator
uint16_t checksum(const char *data_p, int nBytes)
{
    uint16_t sum = 0;
    while (nBytes-- > 0)
    {
        sum += *data_p++;
    }
    return (sum);
}


void millisleep(int milliseconds) {
struct timespec time;
time.tv_sec = milliseconds / 1000;
time.tv_nsec = (milliseconds % 1000) * (1000000);
nanosleep(&time,NULL);
}


void parse_print_data_point(char *buffer, int datapoint_bufferoffset, int datapoint_size, char *inverter_name_description, \
							char *point_description, char *engineering_units, int is_decimal, float decimal_multiplier, int verboseflag){

int bytecounter = 0;
uint32_t parsevar = 0, byteposMultiplier = 1;
float engineering_value;

while (bytecounter < datapoint_size) {
 byteposMultiplier = 1<<(((datapoint_size-1)-bytecounter)*8);
 parsevar = parsevar + (buffer[datapoint_bufferoffset+bytecounter] * byteposMultiplier);
 bytecounter++;
 } 

engineering_value = (float)parsevar * decimal_multiplier;
printf("%s_%s_%s=%0.2f\n", inverter_name_description, point_description, engineering_units, engineering_value);

if (verboseflag){
	//print out extra data about the point if verbose flag is set
	print_formatted_hex_array(buffer, 51, 16, datapoint_bufferoffset, datapoint_size);
	printf("\n");
	}
}




