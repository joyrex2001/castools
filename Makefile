.PHONY: all install clean cpu cas2wav wav2cas casdir

ifneq ($(WINDIR),)
cas2wav_e   = cas2wav.exe
cpuprogram  = cpu.exe
wav2cas_e   = wav2cas.exe
casdir_e    = casdir.exe
else
cas2wav_e   = cas2wav
cpuprogram  = cpu
wav2cas_e   = wav2cas
casdir_e    = casdir
endif

CC = gcc
CPU = $(shell ./${cpuprogram})
CFLAGS = -O2 -Wall -fomit-frame-pointer
CLIBS = -lm

all: clean cpu cas2wav wav2cas casdir

cpu: cpu.c
	$(CC) cpu.c -o $(cpuprogram)

cas2wav: cas2wav.c
	$(CC) $(CFLAGS) -DBIGENDIAN=${CPU} $^ -o $(cas2wav_e) $(CLIBS)

wav2cas: wav2cas.c
	$(CC) $(CFLAGS) -DBIGENDIAN=${CPU} $^ -o $(wav2cas_e) $(CLIBS)

casdir: casdir.c		
	$(CC) $(CFLAGS) -DBIGENDIAN=${CPU} $^ -o $(casdir_e) $(CLIBS)

install: all
	cp $(cas2wav_e) $(wav2cas_e) $(casdir_e) /usr/local/bin

uninstall:
	rm -f /usr/local/bin/$(cas2wav_e) /usr/local/bin/$(wav2cas_e) /usr/local/bin/$(casdir_e)

clean:
	rm -f $(cas2wav_e)
	rm -f $(wav2cas_e)
	rm -f $(casdir_e)		
	rm -f $(cpuprogram)	
