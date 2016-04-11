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
#include <stdlib.h>
#include <memory.h>

char HEADER[8] = { 0x1F,0xA6,0xDE,0xBA,0xCC,0x13,0x7D,0x74 };
char ASCII[10] = { 0xEA,0xEA,0xEA,0xEA,0xEA,0xEA,0xEA,0xEA,0xEA,0xEA };
char BIN[10]   = { 0xD0,0xD0,0xD0,0xD0,0xD0,0xD0,0xD0,0xD0,0xD0,0xD0 };
char BASIC[10] = { 0xD3,0xD3,0xD3,0xD3,0xD3,0xD3,0xD3,0xD3,0xD3,0xD3 };

#define NEXT_NONE   0
#define NEXT_BINARY 1
#define NEXT_DATA   2

int main(int argc, char* argv[])
{
  FILE *ifile;
  char buffer[10];
  char filename[6];
  int  start;
  int  stop;
  int  exec;
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
  while (fread(buffer,1,8,ifile)==8) {
    
    if (!memcmp(buffer,HEADER,8)) {
      
      if (fread(buffer,1,10,ifile)==10) {
	
        if (next==NEXT_BINARY) {
	
	  fseek(ifile,position+8,SEEK_SET);
	  start = fgetc(ifile)+fgetc(ifile)*256;
	  stop  = fgetc(ifile)+fgetc(ifile)*256;
	  exec  = fgetc(ifile)+fgetc(ifile)*256;
	  if (!exec) exec=start;
	  
	  printf("%.6s  binary  %.4x,%.4x,%.4x\n",filename,start,stop,exec);
	  next=NEXT_NONE;
	}
	  
	else if (next==NEXT_DATA) next=NEXT_NONE;
	
	else if (!memcmp(buffer,ASCII,10)) {
	  
	  fread(filename,1,6,ifile);
	  printf("%.6s  ascii\n",filename);
	  
	  while (fgetc(ifile)!=0x1a && !feof(ifile));
	  position=ftell(ifile);
	} 
      
	else if (!memcmp(buffer,BIN,10)) {
	  
	  fread(filename,1,6,ifile); next=NEXT_BINARY;
	}

	else if (!memcmp(buffer,BASIC,10)) {
	  
	  fread(filename,1,6,ifile); next=NEXT_DATA;
	  printf("%.6s  basic\n", filename);
	}
      
	else  printf("------  custom  %.6x\n",(int)position);
    
      }

    }
	
    fseek(ifile, ++position, SEEK_SET);
      
  }
    
  fclose(ifile);
 
  return 0;
}

