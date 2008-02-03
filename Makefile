all: cbmfs

cbmfs: cbmfs.c
	cc -D_FILE_OFFSET_BITS=64 -o cbmfs -lfuse cbmfs.c

zip:
	rm -f cbmfs.zip
	zip -9 cbmfs.zip cbmfs.c Makefile
	
clean:
	rm -f cbmfs cbmfs.zip
