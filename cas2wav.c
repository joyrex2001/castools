/**************************************************************************/
/*                                                                        */
/* file:         cas2wav.c                                                */
/* version:      1.31 (April 11, 2016)                                    */
/*                                                                        */
/* description:  This tool exports the contents of a .cas file to a .wav  */
/*               file. The .cas format is the standard format for MSX to  */
/*               emulate tape recorders. The wav can be copied onto a     */
/*               tape to be read by a real MSX.                           */
/*                                                                        */
/*                                                                        */
/*  This program is free software; you can redistribute it and/or modify  */
/*  it under the terms of the GNU General Public License as published by  */
/*  the Free Software Foundation; either version 2, or (at your option)   */
/*  any later version. See COPYING for more details.                      */
/*                                                                        */
/*                                                                        */
/* Copyright 2001-2016 Vincent van Dam (vincentd@erg.verweg.com)          */
/* MultiCPU Copyright 2007 Ramones     (ramones@kurarizeku.net)           */
/*                                                                        */
/**************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <memory.h>
#include <math.h>

#ifndef bool
#define true   1
#define false  0
#define bool   int
#endif

/* CPU type defines */
#if (BIGENDIAN)
#define BIGENDIANSHORT(value)  ( ((value & 0x00FF) << 8) | \
                                 ((value & 0xFF00) >>8 ) )
#define BIGENDIANINT(value)    ( ((value & 0x000000FF) << 24) | \
                                 ((value & 0x0000FF00) << 8)  | \
                                 ((value & 0x00FF0000) >> 8)  | \
                                 ((value & 0xFF000000) >> 24) )
#define	BIGENDIANLONG(value)   BIGENDIANINT(value) // I suppose Long=int
#else
#define BIGENDIANSHORT(value) value
#define BIGENDIANINT(value)   value
#define	BIGENDIANLONG(value)  value
#endif

/* number of ouput bytes for silent parts */
#define SHORT_SILENCE     OUTPUT_FREQUENCY    /* 1 second  */
#define LONG_SILENCE      OUTPUT_FREQUENCY*2  /* 2 seconds */

/* frequency for pulses */
#define LONG_PULSE        1200
#define SHORT_PULSE       2400

/* number of uint16_t pulses for headers */
#define LONG_HEADER       16000
#define SHORT_HEADER      4000

/* output settings */
#define OUTPUT_FREQUENCY  43200

/* default output baudrate */
int BAUDRATE = 1200;

/* headers definitions */
char HEADER[8] = { 0x1F,0xA6,0xDE,0xBA,0xCC,0x13,0x7D,0x74 };
char ASCII[10] = { 0xEA,0xEA,0xEA,0xEA,0xEA,0xEA,0xEA,0xEA,0xEA,0xEA };
char BIN[10]   = { 0xD0,0xD0,0xD0,0xD0,0xD0,0xD0,0xD0,0xD0,0xD0,0xD0 };
char BASIC[10] = { 0xD3,0xD3,0xD3,0xD3,0xD3,0xD3,0xD3,0xD3,0xD3,0xD3 };

/* definitions for .wav file */
#define PCM_WAVE_FORMAT   1
#define MONO              1
#define STEREO            2

typedef struct
{
  char      RiffID[4];
  uint32_t  RiffSize;
  char      WaveID[4];
  char      FmtID[4];
  uint32_t  FmtSize;
  uint16_t  wFormatTag;
  uint16_t  nChannels;
  uint32_t  nSamplesPerSec;
  uint32_t  nAvgBytesPerSec;
  uint16_t  nBlockAlign;
  uint16_t  wBitsPerSample;
  char      DataID[4];
  uint32_t  nDataBytes;
} WAVE_HEADER;

WAVE_HEADER waveheader =
{
  { "RIFF" },
  BIGENDIANLONG(0),
  { "WAVE" },
  { "fmt " },
  BIGENDIANLONG(16),
  BIGENDIANSHORT(PCM_WAVE_FORMAT),
  BIGENDIANSHORT(MONO),
  BIGENDIANLONG(OUTPUT_FREQUENCY),
  BIGENDIANLONG(OUTPUT_FREQUENCY),
  BIGENDIANSHORT(1),
  BIGENDIANSHORT(8),
  { "data" },
  BIGENDIANLONG(0)
};



/* write a pulse */
void writePulse(FILE *output,uint32_t f)
{
  uint32_t n;
  double length = OUTPUT_FREQUENCY/(BAUDRATE*(f/1200));
  double scale  = 2.0*M_PI/(double)length;

  for (n=0;n<(uint32_t)length;n++)
    putc( (char)(sin((double)n*scale)*127)^128, output);
}



/* write a header signal */
void writeHeader(FILE *output,uint32_t s)
{
  int  i;
  for (i=0;i<s*(BAUDRATE/1200);i++) writePulse(output,SHORT_PULSE);
}



/* write silence */
void writeSilence(FILE *output,uint32_t s)
{
  int n;
  for (n=0;n<s;n++) putc(128,output);
}



/* write a byte */
void writeByte(FILE *output,int byte)
{
  int  i;

  /* one start bit */
  writePulse(output,LONG_PULSE);

  /* eight data bits */
  for (i=0;i<8;i++) {
    if (byte&1) {
      writePulse(output,SHORT_PULSE);
      writePulse(output,SHORT_PULSE);
    } else writePulse(output,LONG_PULSE);
    byte = byte >> 1;
  }

  /* two stop bits */
  for (i=0;i<4;i++) writePulse(output,SHORT_PULSE);
}



/* write data until a header is detected */
void writeData(FILE* input,FILE* output,uint32_t *position,bool* eof)
{
  int  read;
  int  i;
  char buffer[8];

  *eof=false;
  while ((read=fread(buffer,1,8,input))==8) {

    if (!memcmp(buffer,HEADER,8)) return;

    writeByte(output,buffer[0]);
    if (buffer[0]==0x1a) *eof=true;
    fseek(input,++*position,SEEK_SET);
  }

  for (i=0;i<read;i++) writeByte(output,buffer[i]);

  if (buffer[0]==0x1a) *eof=true;
  *position+=read;

  return;
}


/* show a brief description */
void showUsage(char *progname)
{
  printf("usage: %s [-2] [-s seconds] <ifile> <ofile>\n"
         " -2   use 2400 baud as output baudrate\n"
         " -s   define gap time (in seconds) between blocks (default 2)\n"
	 ,progname);
}



int main(int argc, char* argv[])
{
  FILE *output,*input;
  uint32_t size,position;
  int  i,j;
  int  stime = -1;
  bool eof;
  char buffer[10];

  char *ifile = NULL;
  char *ofile = NULL;

  /* parse command line options */
  for (i=1; i<argc; i++) {

    if (argv[i][0]=='-') {

      for(j=1;j && argv[i][j]!='\0';j++)

        switch(argv[i][j]) {

        case '2': BAUDRATE=2400; break;
        case 's': stime=atof(argv[++i]); j=-1; break;

        default:
          fprintf(stderr,"%s: invalid option\n",argv[0]);
          exit(1);
        }

      continue;
    }

    if (ifile==NULL) { ifile=argv[i]; continue; }
    if (ofile==NULL) { ofile=argv[i]; continue; }

    fprintf(stderr,"%s: invalid option\n",argv[0]);
    exit(1);
  }

  if (ifile==NULL || ofile==NULL) { showUsage(argv[0]); exit(1); }

  /* open input/output files */
  if ((input=fopen(ifile,"rb"))==NULL) {
    fprintf(stderr,"%s: failed opening %s\n",argv[0],argv[1]);
    exit(1);
  }

  if ((output=fopen(ofile,"wb"))==NULL) {
    fprintf(stderr,"%s: failed writing %s\n",argv[0],argv[2]);
    exit(1);
  }

  /* write initial .wav header */
  fwrite(&waveheader,sizeof(waveheader),1,output);

  position=0;
  /* search for a header in the .cas file */
  while (fread(buffer,1,8,input)==8) {

    if (!memcmp(buffer,HEADER,8)) {

      /* it probably works fine if a long header is used for every */
      /* header but since the msx bios makes a distinction between */
      /* them, we do also (hence a lot of code).                   */

      position+=8;
      if (fread(buffer,1,10,input) == 10) {

	if (!memcmp(buffer,ASCII,10)) {

	  fseek(input,position,SEEK_SET);
	  writeSilence(output,stime>0?OUTPUT_FREQUENCY*stime:LONG_SILENCE);
	  writeHeader(output,LONG_HEADER);
	  writeData(input,output,&position,&eof);

	  do {

	    position+=8; fseek(input,position,SEEK_SET);
	    writeSilence(output,SHORT_SILENCE);
	    writeHeader(output,SHORT_HEADER);
	    writeData(input,output,&position,&eof);

	  } while (!eof && !feof(input));

	}
	else if (!memcmp(buffer,BIN,10) || !memcmp(buffer,BASIC,10)) {

	  fseek(input,position,SEEK_SET);
	  writeSilence(output,stime>0?OUTPUT_FREQUENCY*stime:LONG_SILENCE);
	  writeHeader(output,LONG_HEADER);
	  writeData(input,output,&position,&eof);
	  writeSilence(output,SHORT_SILENCE);
	  writeHeader(output,SHORT_HEADER);
	  position+=8; fseek(input,position,SEEK_SET);
	  writeData(input,output,&position,&eof);

	} else {

	  printf("unknown file type: using long header\n");
	  fseek(input,position,SEEK_SET);
	  writeSilence(output,LONG_SILENCE);
	  writeHeader(output,LONG_HEADER);
	  writeData(input,output,&position,&eof);
	}

      }
      else {

	printf("unknown file type: using long header\n");
	fseek(input,position,SEEK_SET);
	writeSilence(output,stime>0?OUTPUT_FREQUENCY*stime:LONG_SILENCE);
	writeHeader(output,LONG_HEADER);
	writeData(input,output,&position,&eof);
      }

    } else {

      /* should not occur */
      fprintf(stderr,"skipping unhandled data\n");
      position++;
    }

    fseek(input,position,SEEK_SET);
  }

  /* write final .wav header */
  size = ftell(output)-sizeof(waveheader);
  waveheader.nDataBytes = BIGENDIANLONG(size);
  waveheader.RiffSize = BIGENDIANLONG(size);
  fseek(output,0,SEEK_SET);
  fwrite(&waveheader,sizeof(waveheader),1,output);

  fclose(output);
  fclose(input);

  return 0;
}
