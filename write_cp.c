int my_write(int fd, char buf[], int nbytes)
{
	MINODE** mip = &running->fd[fd]->mptr;
	int blk = 0;
	int nbytes_init = nbytes;
	char wbuf[BLKSIZE];
	char* cq = buf;
	printf("the buffer is %s\n", buf);
	while (nbytes > 0)
	{
		// compute LOGICAL BLOCK (lbk) and the startByte in that lbk:

		int lbk = running->fd[fd]->offset / BLKSIZE;
		int startByte = running->fd[fd]->offset % BLKSIZE;

		printf("getting block...\n");
		if (lbk < 12) {                         // direct block
			printf("small block...\n");
			if ((*mip)->INODE.i_block[lbk] == 0) {   // if no data block yet
				(*mip)->INODE.i_block[lbk] = balloc((*mip)->dev);// MUST ALLOCATE a block
			}
			blk = (*mip)->INODE.i_block[lbk];      // blk should be a disk block now
		}
		else if (lbk >= 12 && lbk < 256 + 12)
		{ // INDIRECT blocks
			printf("indirect block...\n");
			int ibuf[256] = { 0 };
			if ((*mip)->INODE.i_block[12] == 0)
			{
				//allocate a block for it;
				(*mip)->INODE.i_block[12] = balloc((*mip)->dev);
				//zero out the block on disk !!!!
				//put_block(mip->dev,mip->INODE.i_block[12],ibuf);
			}
			memset((char*)ibuf, 0, BLKSIZE);
			//get i_block[12] into an int ibuf[256];
			get_block((*mip)->dev, (*mip)->INODE.i_block[12], (char*)ibuf);
			blk = ibuf[lbk - 12];
			if (blk == 0)
			{
				//allocate a disk block;
				ibuf[lbk - 12] = balloc((*mip)->dev);
				//record it in i_block[12];
				blk = ibuf[lbk - 12];
				put_block((*mip)->dev, (*mip)->INODE.i_block[12], (char*)ibuf);
			}
		}
		else
		{
			printf("double indirect block...\n");
			// double indirect blocks POSITION 13*/
			int ibuf[256] = { 0 };
			int ibuf2[256] = { 0 };
			if ((*mip)->INODE.i_block[13] == 0)
			{
				//allocate a block for it;
				(*mip)->INODE.i_block[13] = balloc((*mip)->dev);
				//zero out the block on disk !!!!
			}
			memset((char*)ibuf, 0, BLKSIZE);
			get_block((*mip)->dev, (*mip)->INODE.i_block[13], (char*)ibuf);
			//MAILMAN CALCULATION
			int lbk_double = (lbk - (256 + 12)) / 256;
			int lbk_double_indirect = (lbk - (256 + 12)) % 256;

			if (ibuf[lbk_double] == 0)
			{
				ibuf[lbk_double] = balloc((*mip)->dev);
			}
			memset((char*)ibuf2, 0, BLKSIZE);
			get_block((*mip)->dev, ibuf[lbk_double], (char*)ibuf2);
			blk = ibuf2[lbk_double_indirect];
			if (blk == 0)
			{
				//allocate a disk block;
				blk = balloc((*mip)->dev);
				ibuf[lbk_double_indirect] = blk;
				put_block((*mip)->dev, ibuf[lbk_double], (char*)ibuf2);
			}
			put_block((*mip)->dev, (*mip)->INODE.i_block[13], (char*)ibuf);
		}

		/* all cases come to here : write to the data block */

		get_block((*mip)->dev, blk, wbuf);   // read disk block into wbuf[ ]
		char* cp = wbuf + startByte;      // cp points at startByte in wbuf[]
		int remain = BLKSIZE - startByte;     // number of BYTEs remain in this block
		printf("writing to block... %d\n", blk);
		//if we have enough left in our block to write everything,
		if (remain >= nbytes)
		{
			strncpy(cp, cq, nbytes);
			running->fd[fd]->offset += nbytes;
			if (running->fd[fd]->offset > (*mip)->INODE.i_size)
			{
				(*mip)->INODE.i_size += nbytes;
			}
			nbytes = 0;
			//could just use break here?
		}
		else
		{
			//we can only write remain bytes
			strncpy(cp, cq, remain);
			*cq += remain;
			running->fd[fd]->offset += remain;
			nbytes -= remain;
			if (running->fd[fd]->offset > (*mip)->INODE.i_size)
			{
				(*mip)->INODE.i_size += remain;
			}
		}

		printf("returning block...\n");
		put_block((*mip)->dev, blk, (char*)wbuf);   // write wbuf[ ] to disk

		// loop back to outer while to write more .... until nbytes are written
	}

	(*mip)->dirty = 1;       // mark mip dirty for iput()
	printf("wrote %d char into file descriptor fd=%d\n", nbytes_init, fd);
	return nbytes;
}

int write_file()
{
	printf("========================================\n");
	printf("                        WRITE FILE START\n");
	if (strlen(pathname) == 0)
	{
		printf("write file error: path is empty\n");
		return -1;;
	}
	char buf[128] = { 0 };
	char fdstring[3] = { 0 };
	int fd;
	sscanf(pathname, "%s \"%[^\"]\"", fdstring, buf);

	//expect an fd and a text string to write
	//check sscanf results
	if (strlen(fdstring) == 0 || strlen(buf) == 0)
	{
		printf("write file error: bad sscanf\n");
		return -1;;
	}

	fd = atoi(fdstring);
	printf("fd = %d, textlen = %ld\n", fd, strlen(buf));
	printf("verify fd is open...\n");
	//verify fd is opened for 1-3 modes
	if (!running->fd[fd] || running->fd[fd]->mode == 0)
	{
		printf("write file error: fd isn't open or is in R\n");
		return -1;
	}
	//copy the text into a buffer and get its length
	int nbytes = strlen(buf);
	//return mywrite
	printf("starting write...\n");
	return(my_write(fd, buf, nbytes));
}

int cp(char* pathname) {//char *src, char *dest
	printf("Inside of cp\n");
	char* token[128], * s;
	char original[128], temp[128];
	int n = 0;
	strcpy(original, pathname);
	strcpy(temp, pathname);
	s = strtok(temp, " ");
	while (s) {
		token[n++] = s;
		s = strtok(NULL, " ");
	}
	printf("Token[0] = %s and Token[1] = %s\n", token[0], token[1]);
	strcpy(pathname, token[0]);
	strcat(pathname, " 0");
	printf("****pathname = %s *****\n", pathname);
	int fd = my_open();

	strcpy(pathname, token[1]);
	strcat(pathname, " 1");
	printf("****pathname for GD = %s *****\n", pathname);
	int gd = my_open();

	printf("\nsrc = %s\n", token[0]);
	printf("\ndest = %s\n", token[1]);

	printf("fd = %d\n", fd);
	printf("gd = %d\n", gd);

	char buf[BLKSIZE];

	while ((n = myread(fd, buf, BLKSIZE))) {
		printf("n = %d\n", n);
		my_write(gd, buf, n);
	}
}

int mv(char* src, char* dest) {
	MINODE* mip;
	dev = running->cwd->dev;
	int ino = getino(src);
	//verify src exists
	if (ino == 0) {
		printf("Source file does not exist\n");
		return -1;
	}

	mip = iget(dev, ino);

	//check whether src is on the same dev as src
	if (mip->dev == dev) {
		printf("same dev as src\n");
		my_link();
		unlink(src);
	}
	else {
		printf("Not same dev as src\n");
		//cp(src, dest);
		unlink(src);
	}

	printf("File moved successfully!\n");
	return 0;
}