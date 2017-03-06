// Check if CPU if Big Endian or Litle Endian

#include <stdio.h>
#include <stdint.h>

#ifndef int32_t
#define int32_t long
#endif

int main( int argc, char **argv ) {

		char	bytearray[4] = {0x01,0x02,0x03,0x04};

		long	*b = (int32_t*) bytearray;



		if	(*b == 0x01020304) {
				printf ("1\n");

		}
		else
		{
				printf("0\n");

		}
		return	0;

}
