int myread(int fd, char* buf, int nbytes) {
	int count = 0;
	int blk; //logicalblock, converthelper

	char readbuf[BLKSIZE];
	int buf2[256];//indirect buffer
	int buf3[256];//double indirect buffer
	int dbuf[256];//double indirect converter

	//set oftp to correct fd
	OFT* oftp = running->fd[fd];
	MINODE* mip = oftp->mptr;

	bzero(readbuf, BLKSIZE);

	char* cq = buf;

	//get amount of bytes to read using file size minus offset
	int avil = mip->INODE.i_size - oftp->offset;

	while (nbytes && avil) {
		//mailman logical block num and the startbyte from offset
		int lbk = oftp->offset / BLKSIZE;
		int startByte = oftp->offset % BLKSIZE;
		//printf("offset = %d, BLKSIZE = 1024",oftp->offset);

		//direct blocks
		if (lbk < 12) {
			blk = mip->INODE.i_block[lbk];//map LOGICAL lbk to PHYSICAL blk
		}
		//indirect blocks
		else if (lbk >= 12 && lbk < 256 + 12) {
			get_block(mip->dev, mip->INODE.i_block[12], (char*)buf2);
			int* ip = (int*)buf2 + lbk - 12;
			blk = *ip; //get blk num
		}
		//double indirect blocks
		else {
			get_block(mip->dev, mip->INODE.i_block[13], (char*)buf3);
			lbk -= (12 + 256);
			int dblk = buf3[lbk / 256];
			get_block(mip->dev, dblk, (char*)dbuf);
			blk = dbuf[lbk % 256];
		}

		//get the data block into readbuf[BLKSIZE]
		get_block(mip->dev, blk, readbuf);
		//copy from startByte to buf[], at most remain bytes in this block
		char* cp = readbuf + startByte;
		int remain = BLKSIZE - startByte;//number of bytes remain in readbuff[]

		int max = remain;
		if (max > nbytes) {
			max = nbytes;
		}
		else if (max > avil) {
			max = avil;
		}

		memcpy(cq, cp, max);
		avil -= max;
		nbytes -= max;
		remain -= max;
		count += max;
		oftp->offset += max;
	}
	//if one data block is not enough, loop back to OUTER while for more...

  //printf("myread: read %d char from file descriptor %d\n", count, fd);
	return count;
}

int read_file()
{
	int fd, nbytes;
	char buf[BLKSIZE];

	//ask for a fd and nbytes to read
	printf("Enter a fd and nbytes\n");
	printf("fd = ");
	scanf("%d", &fd);

	while ((getchar()) != '\n');

	printf("nbytes = ");
	scanf("%d", &nbytes);

	while ((getchar()) != '\n');

	//verify that fd is indeed opened for RD or RW
	//check to see if open
	if (!running->fd[fd]) {
		printf("Error: not open\n");
		return -1;
	}
	//check to see if not RD or RW
	if (running->fd[fd]->mode != 0 && running->fd[fd]->mode != 2) {
		printf("Error: invalid mode\n");
		return -1;
	}
	int count = myread(fd, buf, nbytes);
	printf("myread: read %d char from file descriptor %d\n", count, fd);
	buf[count] = 0;//as a null terminated string
	puts(buf);//better than the printf
	printf("\n");
}

int my_cat(char* pathname) {
	char mybuf[1024];
	int n, i;
	int size = 1024;
	strcat(pathname, " 0");
	const int fd = my_open();

	if (fd < 0)
	{
		printf("Error: open failed, check if target file exists or is already open\n");
		return -1;
	}
	//check for pathname
	if (!pathname) {
		printf("Error: No file name.\n");
		return -1;
	}

	while ((n = myread(fd, mybuf, BLKSIZE)) > 0) {
		mybuf[n] = 0;//as a null terminated string
		puts(mybuf);//better than the printf
		memset((char*)mybuf, 0, BLKSIZE);
	}
	printf("\n");
	my_close_file(fd);

	return 0;
}