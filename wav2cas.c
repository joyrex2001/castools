/**************************************************************************/
/*                                                                        */
/* file:         wav2cas.c                                                */
/* version:      1.31 (April 11, 2016)                                    */
/*                                                                        */
/* description:  This tool exports the contents of a .wav file to a .cas  */
/*               file. The .cas format is the standard format for MSX to  */
/*               emulate tape recorders and can be used with most         */
/*               emulators.                                               */
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
#include <string.h>
#include <memory.h>

#ifndef bool
#define true   1
#define false  0
#define bool   int
#endif

#define THRESHOLD_SILENCE   100
#define THRESHOLD_HEADER    25

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

/* default arguments */
int   threshold = 5;     /* amplitude threshold  */
bool  envelope  = 2;     /* envelope correction  */
bool  normalize = false; /* amplitude normalize  */
bool  phase     = true;  /* phase shift */
float window    = 1.5;   /* window factor */

/* header definitions of a wav file */
typedef struct
{
  char     RiffID[4];
  uint32_t RiffSize;
  char     WaveID[4];
  char     FmtID[4];
  uint32_t FmtSize;
  uint16_t wFormatTag;
  uint16_t nChannels;
  uint32_t nSamplesPerSec;
  uint32_t nAvgBytesPerSec;
  uint16_t nBlockAlign;
  uint16_t wBitsPerSample;
} WAVE_HEADER;

typedef struct
{
  char     DataID[4];
  uint32_t nDataBytes;
} WAVE_BLOCK;



/* Read wav file for tape image */
int tapeRead(char* szFileName, int8_t** pBuffer, int32_t *size)
{
  FILE* wav_file;
  WAVE_HEADER header;
  WAVE_BLOCK  block;

  int  adder;
  int32_t i,j,pos;
  int8_t data;
  bool found;

  if ((wav_file=fopen(szFileName,"rb"))==NULL) return -1;

  fread(&header,sizeof(header),1,wav_file);

  /* Make header compatible with PPC micro */
  #if (BIGENDIAN)
  header.RiffSize       = BIGENDIANLONG(header.RiffSize);
  header.FmtSize        = BIGENDIANLONG(header.FmtSize);
  header.nSamplesPerSec = BIGENDIANLONG(header.nSamplesPerSec);
  header.nAvgBytesPerSec= BIGENDIANLONG(header.nAvgBytesPerSec);
  header.wFormatTag     = BIGENDIANSHORT(header.wFormatTag);
  header.nChannels      = BIGENDIANSHORT(header.nChannels);
  header.nBlockAlign    = BIGENDIANSHORT(header.nBlockAlign);
  header.wBitsPerSample = BIGENDIANSHORT(header.wBitsPerSample);
  #endif

  /* Determine how many bytes to skip in reading file for 8-bit mono */
  adder=header.nChannels*(header.wBitsPerSample/8);

  /* Search for data tag */
  found = false;
  pos = ftell(wav_file);
  while(fread(&block,sizeof(block),1,wav_file))
    if (!strncmp(block.DataID,"data",4)) {
      *size=BIGENDIANLONG(block.nDataBytes)/adder ;
      *pBuffer=(int8_t*)malloc(*size*sizeof(int8_t));
      found = true;
      break;
    } else {
      fseek(wav_file,pos++,SEEK_SET);
    }

  /* Basic error handling */
  if (!found) {
    fprintf(stderr,"Incorrect wav header!\n");
    return -1;
  }

  if (*pBuffer==NULL) {
    fprintf(stderr,"Not enough memory!\n");
    return -1;
  }

  /* Show wav info */
  printf("Reading %s (%d Hz, %d-bits, %s)...\n",
	 szFileName,
	 (int)header.nSamplesPerSec,
	 (int)header.wBitsPerSample,
	 header.nChannels==1 ? "mono" : "stereo" );

  /* Read file */
  for (i=0;i<(*size);i++) {

    for (j=1; j<adder; j++) fread(&data,sizeof(int8_t),1,wav_file);
    fread(&data,sizeof(int8_t),1,wav_file);
    if (header.wBitsPerSample==8) data^=128;
    if (phase) data=-data;
    (*pBuffer)[i]=data;
  }

  fclose(wav_file);
  return header.nSamplesPerSec;
}



/* correct envelope and denoise signal */
void correctEnvelope(int8_t **buffer,int32_t size)
{
  int32_t i;
  for (i=1;i<size-1;i++)

    (*buffer)[i] = ( 0.5*(*buffer)[i-1] +
		     1.0*(*buffer)[i]   +
		     2.0*(*buffer)[i+1]   ) / 3.5;
}



/* make signal as loud as possible */
void normalizeAmplitude(int8_t **buffer,int32_t size)
{
  int32_t i;
  int  maximum=0;
  for (i=0;i<size;i++)
    if (abs((*buffer)[i])>maximum) maximum=abs((*buffer)[i]);
  for (i=0;i<size;i++) (*buffer)[i]*=127/(float)maximum;
}



/* detect silence */
bool isSilence(int8_t *buffer,int32_t index,int32_t size)
{
  int32_t silent=0;

  while (index<size && silent<THRESHOLD_SILENCE) {

    if ((buffer[index] >= threshold ||
	 buffer[index] <= -threshold )) return false;

    silent++; index++;
  }

  return true;
}



/* skip silent parts */
void skipSilence(int8_t *buffer, int32_t *index, int32_t size)
{
  while(*index<size &&
	(buffer[*index] <= threshold &&
	 buffer[*index] >= -threshold )) (*index)++;
}



/* get the number of bytes of one pulse */
int32_t getPulseWidth(int8_t *buffer, int32_t *index, int32_t size)
{
  int min = 1000;
  int max =-1000;
  int pt  = max;

  int prev = *index > 0 ? buffer[(*index)-1] : 0;

  int32_t width = 0;
  for(;*index<size;width++) {

    /* ascending */
    if (buffer[*index]>prev) {

      if (prev==min) {

	if (pt-min>=threshold) {

	  while(width>1) {

	    if (buffer[*index]>=pt-(pt-min)/2) break;
	    width--; (*index)--;
	  }

	  return width;
	}

	min=1000;
      }

      if (buffer[*index]>max) max=buffer[*index];
    }


    /* descending */
    if (buffer[*index]<prev) {

      if (prev==max) {

	if (max>pt) pt=max;
	max=-1000;
      }

      if (buffer[*index]<min) min=buffer[*index];
    }

    prev=buffer[(*index)++];
  }

  return width;
}



/* detect headers */
bool isHeader(int8_t *buffer, int32_t index, int32_t size)
{

  int32_t width;
  int32_t pulses  = 0;
  int32_t biggest = 0;

  /* skip first pulse for phase independance */
  getPulseWidth(buffer,&index,size);

  while (index<size && pulses<THRESHOLD_HEADER ) {

    width = getPulseWidth(buffer,&index,size);
    if (!biggest) biggest=width;
    if (width>(float)biggest*window) return false;
    if (width>biggest) biggest = width;
    pulses++;
  }

  if (pulses>=THRESHOLD_HEADER) return true;

  return false;
}



/* skip header and return average with of a short pulse */
float skipHeader(int8_t *buffer, int32_t *index, int32_t size)
{

  int32_t  width;
  int32_t  count   = 0;
  float average = 0;

  /* skip first pulse for phase independance */
  getPulseWidth(buffer,index,size);

  while (*index<size) {

    width=getPulseWidth(buffer,index,size);

    if (average && width>(float)average*window ) {

	*index-=width;
	return average;
    }

    /* average=(count*average+width)/++count; */
    count++; average=((count-1)*average+width)/count;
  }

  return average;
}



/* read a byte from wave data */
int readByte(int8_t *buffer, int32_t *index, int32_t size, float average)
{
  int  bit;
  int32_t width;
  int  value = 0;
  int  i;

  /* start bit (int32_t pulse) */
  width=getPulseWidth(buffer,index,size);
  if (isSilence(buffer,*index,size) ||
      width<average*window) return -1;

  /* data bits (lsb first) */
  for (bit=0;bit<8;bit++) {

    width=getPulseWidth(buffer,index,size);
    if (isSilence(buffer,*index,size)) return -1;

    if (width<average*window) {

      value+=(1<<bit);
      getPulseWidth(buffer,index,size); /* skip 2nd short pulse */
      if (isSilence(buffer,*index,size)) return -1;
    }
  }

  /* two stop bits (four short pulses) */
  for (i=0;i<3;i++) {

    getPulseWidth(buffer,index,size);
    if (isSilence(buffer,*index,size)) return -1;
  }
  getPulseWidth(buffer,index,size);

  return value;
}



/* show a brief description */
void showUsage(char *progname)
{
  printf("usage: %s [-np] [-t threshold] [-w window] [-e envelope] <ifile> <ofile>\n"
	 " -n   normalize amplitude level\n"
	 " -p   phase shift signal\n"
	 " -w   window factor (default:%.1f)\n"
	 " -e   level of envelope correction (default:%d)\n"
	 " -t   threshold factor (default:%d)\n"
	 ,progname,window,envelope,threshold);
}



int main(int argc, char* argv[])
{
  FILE *output;
  int8_t *buffer;
  int32_t frequency,size,index,written;
  float average;
  int   data,i,j;
  bool  header;

  char  *ifile = NULL;
  char  *ofile = NULL;

  /* parse command line options */
  for (i=1; i<argc; i++) {

    if (argv[i][0]=='-') {

      for(j=1;j && argv[i][j]!='\0';j++)

	switch(argv[i][j]) {

	case 'n': normalize=true; break;
	case 'p': phase=false; break;
	case 'w': window=atof(argv[++i]);    j=-1; break;
	case 't': threshold=atoi(argv[++i]); j=-1; break;
	case 'e': envelope=atoi(argv[++i]);  j=-1; break;

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

  /* read the sample data and store it in buffer */
  frequency=tapeRead(ifile,&buffer,&size);
  if (frequency<0) {

    fprintf(stderr,"%s: failed reading %s\n",argv[0],ifile);
    exit(1);
  }

  /* open/create the output data file */
  if ((output=fopen(ofile,"wb"))==NULL) {

    fprintf(stderr,"%s: failed writing %s\n",argv[0],ofile);
    exit(1);
  }

  /* work on signal first */
  if (normalize) normalizeAmplitude(&buffer,size);
  for(i=0;i<envelope;i++) correctEnvelope(&buffer,size);

  /* let's do it */
  printf("Decoding audio data...\n");

  /* sample probably starts with some silence before the data, skip it */
  written=index=0;
  skipSilence(buffer,&index,size);

  header=false;
  /* loop through all audio data and extract the contents */
  for (;index<size;index++) {

    /* detect silent parts and skip them */
    if (isSilence(buffer,index,size)) {

      printf("[%.1f] skipping silence\n",(double)index/frequency);
      skipSilence(buffer,&index,size);
    }

    /* detect header and proces the data block followed */
    if (isHeader(buffer,index,size)) {

      printf("[%.1f] header detected\n",(double)index/frequency);
      average=skipHeader(buffer,&index,size);

      /* write .cas header if none already written */
      if (!header) {

	/* .cas headers always start at fixed positions */
	for (;written&7;written++) putc(0x00,output);

	/* write a .cas header */
	putc(0x1f,output); putc(0xa6,output);
	putc(0xde,output); putc(0xba,output);
	putc(0xcc,output); putc(0x13,output);
	putc(0x7d,output); putc(0x74,output);
	written+=8;
	header=true;
      }

      printf("[%.1f] data block\n",(double)index/frequency);

      while (!isSilence(buffer,index,size) && index<size) {
	data=readByte(buffer,&index,size,average);
	if (data>=0) { putc(data,output); written++; header=false; }
	else break;
      }

    } else {

      /* data found without a header, skip it */
      printf("[%.1f] skipping headerless data\n",(double)index/frequency);
      while(!isSilence(buffer,index,size) && index<size ) index++;
    }

  }

  fclose(output);
  free(buffer);

  printf("All done...\n");
  return 0;
}
