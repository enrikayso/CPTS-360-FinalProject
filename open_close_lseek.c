int my_truncate(MINODE* mip)
{
	char ibuf[BLKSIZE] = { 0 };
	char ibuf2[BLKSIZE] = { 0 };
	//release mips inode data blocks
	//a file may have 12 directm 256 indirect and 256*256 double indirect (use a for loop^2)
	for (int i = 0; i < 12; i++)
	{
		if (mip->INODE.i_block[i] > 0)
			bdalloc(mip->dev, mip->INODE.i_block[i]);
		else
			break;
	}

	if (mip->INODE.i_block[12] != 0)
	{
		printf("We have indirect blocks\n");
		//get the indirect blocks
		//memset((char *)ibuf,0,BLKSIZE);
		get_block(mip->dev, mip->INODE.i_block[11], (char*)ibuf);

		for (int i = 0; i < 255; i++)
		{
			if (ibuf[i] > 0)
				bdalloc(mip->dev, ibuf[i]);
			else
				break;
		}

		bdalloc(mip->dev, mip->INODE.i_block[12]);
	}
	if (mip->INODE.i_block[13] != 0)
	{
		printf("We have double blocks\n");
		memset((char*)ibuf, 0, BLKSIZE);
		get_block(mip->dev, mip->INODE.i_block[13], (char*)ibuf);
		for (int i = 0; i < 256; i++)
		{
			if (ibuf[i] > 0)
			{
				memset((char*)ibuf2, 0, BLKSIZE);
				get_block(mip->dev, ibuf[i], (char*)ibuf2);
				for (int j = 0; j < 256; j++)
				{
					if (ibuf2[j] > 0)
						bdalloc(mip->dev, ibuf2[j]);
					else
						break;
				}
				bdalloc(mip->dev, ibuf[i]);
			}
		}
		bdalloc(mip->dev, mip->INODE.i_block[13]);
	}

	//2 update time
	//3 set size to 0 and mark dirty
	mip->INODE.i_size = 0;
	mip->INODE.i_atime = mip->INODE.i_mtime = time(0L);
	mip->dirty = 1;
	return 0;
}

int my_open()
{
	printf("========================================\n");
	printf("                              OPEN START\n");
	int i = 0;
	if (strlen(pathname) == 0)
	{
		printf("open error: path is empty\n");
		return -1;;
	}
	char filename[128];
	int mode = 5;

	char tempmode[128];
	sscanf(pathname, "%s %s", filename, tempmode);

	//check sscanf results
	if (strlen(filename) == 0 || strlen(tempmode) == 0)
	{
		printf("open error: bad sscanf\n");
		return -1;;
	}

	mode = atoi(tempmode);
	printf("filename = %s, mode = %d\n", filename, mode);
	//check open flags
	if (mode < 0 || mode > 4)
	{
		printf("open error: bad mode\n");
		return -1;;
	}

	//determine relative or absolute
	if (pathname[0] == '/')
		dev = root->dev;
	else
		dev = running->cwd->dev;

	printf("dev = %d", dev);
	int ino = getino(filename);
	if (ino == 0)
	{
		if (mode == 4)
		{
			strcpy(pathname, filename);
			creat_file();
			ino = getino(filename);
			if (ino == 0)
			{
				printf("open error: bad creat or getino\n");
				return -1;;
			}
		}
		else
		{
			printf("open error: filename does not exist\n");
			return -1;;
		}
	}
	printf(" ino = %d\n", ino);

	MINODE* mip = iget(dev, ino);
	if (!S_ISREG(mip->INODE.i_mode))
	{
		printf("open error: filename is a directory or link\n");
		iput(mip);
		return -1;;
	}
	for (i = 0; i < NFD; i++)
	{
		if (running->fd[i])
		{
			if (running->fd[i]->mptr->ino == ino && running->fd[i]->refCount != 0)
			{
				if ((mode == 0 && running->fd[i]->mode != 0) || mode != 0)
				{
					printf("open error: file already open or otherwise unable to open\n");
					iput(mip);
					return -1;;
				}
			}
		}
	}

	//make a new OFT
	OFT* oftp = (OFT*)malloc(sizeof(OFT));
	oftp->mode = mode;
	oftp->refCount = 1;
	oftp->mptr = mip;

	switch (mode)
	{
	case 0://R: Offset 0
		oftp->offset = 0;
		break;
	case 1://W: truncate
		my_truncate(mip);
		oftp->offset = 0;
		break;
	case 2://RW: do not truncate
		oftp->offset = 0;
	case 3://append
		oftp->offset = mip->INODE.i_size;
		break;
	default:
		printf("open error: invalid mode \n");
		return -1;
	}
	printf("we look for lowest free\n");
	//look for lowest free entry in FD
	i = 0;
	while (i < NFD)
	{
		if (!running->fd[i])
		{
			printf("inserted oftp in fd[%d]\n", i);
			running->fd[i] = oftp;
			break;
		}
		i++;
	}

	switch (mode)
	{
	case 0:
		mip->INODE.i_atime = time(0L);
		break;
	default:
		mip->INODE.i_atime = mip->INODE.i_mtime = time(0L);
	}

	mip->dirty = 1;
	printf("open success\n");
	return i;
}

int my_close_file(int fd)
{
	printf("========================================\n");
	printf("                             CLOSE START\n");
	if (fd > NFD || !running->fd[fd])
	{
		printf("close error: fd out of range\n");
		return -1;
	}
	OFT* oftp = running->fd[fd];
	running->fd[fd] = 0;
	oftp->refCount--;

	if (oftp->refCount > 0)
	{
		printf("close success\n");
		return 0;
	}

	//if we are the last user we want to dispose of the node.
	MINODE* mip = oftp->mptr;
	iput(mip);
	printf("close success\n");
	return 0;
}

int my_lseek(int fd, int position)
{
	printf("========================================\n");
	printf("                             LSEEK START\n");
	if (fd > NFD || !running->fd[fd])
	{
		printf("lseek error: fd out of range\n");
		return -1;
	}

	if (position >= 0 && position < running->fd[fd]->mptr->INODE.i_size)
	{
		int originalPosition = running->fd[fd]->offset;
		running->fd[fd]->offset = position;
		printf("lseek success\n");
		return originalPosition;
	}

	printf("lseek error: fd out of range\n");
	return -1;
}

int my_pfd()
{
	printf("========================================\n");
	printf("                               PFD START\n");
	char temp[30];
	printf(" fd 	 mode 	 offset 	 INODE\n");
	printf("----	 ----	 ------		-------\n");
	for (int i = 0; i < NFD; i++)
	{
		if (running->fd[i])
		{
			switch (running->fd[i]->mode)
			{
			case 0://R:
				strcpy(temp, "READ");
				break;
			case 1://W:
				strcpy(temp, "WRITE");
				break;
			case 2://RW:
				strcpy(temp, "R/W");
				break;
			case 3://append
				strcpy(temp, "APPEND");
				break;
			default:
				printf("PFD error: invalid mode \n");
				return -1;
			}
			printf(" %d      %s        %d       	[%d, %d]\n", i, temp, running->fd[i]->offset, running->fd[i]->mptr->dev, running->fd[i]->mptr->ino);
		}
	}
}

int my_dup(int fd)
{
	if (!running->fd[fd])
	{
		printf("dup error : fd is empty\n");
		return -1;
	}
	else
	{
		for (int i = 0; i < NFD; i++)
		{
			if (!running->fd[i])
			{
				running->fd[fd]->refCount++;
				running->fd[i] == running->fd[fd];
				return i;
			}
		}
	}
}