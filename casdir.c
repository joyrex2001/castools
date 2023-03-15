/**************************************************************************/
/*                                                                        */
/* file:         casdir.c                                                 */
/* version:      1.31 (April 11, 2016)                                    */
/*                                                                        */
/* description:  This tool displays the contents of a .cas file. The .cas */
/*               format is the standard format for MSX to emulate tape    */
/*               recorders and can be used with most emulators.           */
/*                                                                        */
/*                                                                        */
/*  This program is free software; you can redistribute it and/or modify  */
/*  it under the terms of the GNU General Public License as published by  */
/*  the Free Software Foundation; either version 2, or (at your option)   */
/*  any later version. See COPYING for more details.                      */
/*                                                                        */
/*                                                                        */
/* Copyright 2001-2016 Vincent van Dam (vincentd@erg.verweg.com)          */
/*                                                                        */
/**************************************************************************/

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <memory.h>

char HEADER[8] = { 0x1F,0xA6,0xDE,0xBA,0xCC,0x13,0x7D,0x74 };
char ASCII[10] = { 0xEA,0xEA,0xEA,0xEA,0xEA,0xEA,0xEA,0xEA,0xEA,0xEA };
char BIN[10]   = { 0xD0,0xD0,0xD0,0xD0,0xD0,0xD0,0xD0,0xD0,0xD0,0xD0 };
char BASIC[10] = { 0xD3,0xD3,0xD3,0xD3,0xD3,0xD3,0xD3,0xD3,0xD3,0xD3 };

enum next {
  NEXT_NONE,
  NEXT_ASCII,
  NEXT_BINARY,
  NEXT_DATA
};

int main(int argc, char* argv[])
{
  FILE *ifile;
  union {
    uint8_t data[10];
    struct binary_header_t {
      uint16_t start, stop, exec;
    } binary_header;
  } buffer;
  char filename[6];
  long position;
  int  next = NEXT_NONE;

  if (argc != 2) {
    
    printf("usage: %s <ifile>\n",argv[0]);
    exit(0);
  }
  
  if ( (ifile = fopen(argv[1],"rb")) == NULL) {

    fprintf(stderr,"%s: failed opening %s\n",argv[0],argv[1]);
    exit(1);
  }

  position=0;
  while (fread(&buffer,1,8,ifile)==8) {
    
    position += 8;

    if (!memcmp(&buffer,HEADER,8)) {
      
      switch (next) {

	case NEXT_NONE:
	default:
          if (fread(&buffer,1,10,ifile)==10) {
	    if (!memcmp(&buffer,ASCII,10)) {
	      
	      fread(filename,1,6,ifile); next=NEXT_ASCII;
	      printf("%.6s  ascii\n",filename);
	      position += 16;
	    } 
      
	    else if (!memcmp(&buffer,BIN,10)) {
	      
	      fread(filename,1,6,ifile); next=NEXT_BINARY;
	      position += 16;
	    }

	    else if (!memcmp(&buffer,BASIC,10)) {
	      
	      fread(filename,1,6,ifile); next=NEXT_DATA;
	      printf("%.6s  basic\n", filename);
	      position += 16;
	    }
      
	    else {

	      printf("------  custom  %.6x\n",(int)position-8);
	      fseek(ifile, -2, SEEK_CUR);
	      position += 8;
	    }
	  }
	  break;

	case NEXT_ASCII:
	  while (fread(&buffer,1,8,ifile) == 8 &&
	         memchr(&buffer, 0x1a, 8) == NULL)
	    position += 8;
	  position += 8;

	  next = NEXT_NONE;
	  break;

	case NEXT_BINARY:
	  if (fread(&buffer,1,8,ifile)==8) {
	    if (!buffer.binary_header.exec)
	       buffer.binary_header.exec=buffer.binary_header.start;

	    printf("%.6s  binary  %.4x,%.4x,%.4x\n",filename,
	    		buffer.binary_header.start,
	    		buffer.binary_header.stop,
	    		buffer.binary_header.exec);
	    position += 8;
	    next=NEXT_NONE;
	  }
	  break;

	case NEXT_DATA:
	  next=NEXT_NONE;
	  break;
      }
    }
  }
    
  fclose(ifile);
 
  return 0;
}

