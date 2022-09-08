int enter_name(MINODE* pip, int myino, char* myname)
{
	char buf[BLKSIZE];
	bzero(buf, BLKSIZE); //optional clear from texbook.
	int i;
	for (i = 0; i < 12; i++)
	{
		if (pip->INODE.i_block[i] == 0)
		{
			printf("we jumped to step 5\n");
			break;
		}

		//     (1). get parent's data block into a buf[];
		get_block(dev, pip->INODE.i_block[i], buf);
		DIR* dp = (DIR*)buf;
		char* cp = buf;

		// step to LAST entry in block:
		int blk = pip->INODE.i_block[i];
		printf("step to LAST entry in data block %d\n", blk);
		while (cp + dp->rec_len < buf + BLKSIZE)
		{
			printf("stepped past %s\n", dp->name);
			cp += dp->rec_len;
			dp = (DIR*)cp;
		}

		//Let remain = LAST entry's rec_len - its IDEAL_LENGTH;
		int ideal_len = 4 * ((8 + dp->name_len + 3) / 4);
		const int remain = dp->rec_len - ideal_len;
		if (remain >= 4 * ((8 + strlen(myname) + 3) / 4))
		{
			//trim previous
			dp->rec_len = ideal_len;
			//enter new entry
			cp += dp->rec_len;
			dp = (DIR*)cp;
			dp->inode = myino;
			dp->rec_len = BLKSIZE - ((u32)(uintptr_t)cp - (u32)(uintptr_t)buf);
			dp->name_len = strlen(myname);
			dp->file_type = EXT2_FT_DIR;
			strcpy(dp->name, myname);
			put_block(dev, blk, buf);
			if (running->cwd->dev != dev)
			{
				printf("the cwd has changed");
			}
			
			return 0;
		}
	}

	int bno = balloc(dev);
	if (bno <= 0)
	{
		printf("enter child error: block error\n");
		return -1;
	}
	printf("finally, dev is %d\n",dev);
	pip->INODE.i_block[i] = bno;
	pip->INODE.i_size += BLKSIZE;
	pip->dirty = 1;
	get_block(dev, bno, buf);

	DIR* dp = (DIR*)buf;
	dp->inode = myino;
	dp->file_type = EXT2_FT_DIR;
	dp->rec_len = BLKSIZE;
	dp->name_len = strlen(myname);
	strcpy(dp->name, myname);
	put_block(dev, bno, buf);
	return 0;
}

//from both exercise guide and textbook
int mymkdir(MINODE* pip, char* name)
{
	int ino = ialloc(dev);
	int bno = balloc(dev);
	printf("> ino = %d, bmo = %d\n", ino, bno);
	MINODE* mip = iget(dev, ino);
	INODE* ip = &mip->INODE;

	//(4).2 Create INODE (textbook)
	ip->i_mode = 0x41ED;
	ip->i_uid = running->uid;
	ip->i_gid = running->gid;
	ip->i_size = BLKSIZE;
	ip->i_links_count = 2; //FOR . AND ..
	ip->i_atime = ip->i_ctime = ip->i_mtime = time(0L);
	ip->i_blocks = 2;
	ip->i_block[0] = bno;
	for (int x = 1; x < 15; x++)
		ip->i_block[x] = 0;
	mip->dirty = 1;
	iput(mip);

	//(4).3 Create Data (textbook)
	char buf[BLKSIZE];
	bzero(buf, BLKSIZE); //optional clear from texbook.
	//get_block(dev,bno,buf);

	DIR* dp = (DIR*)buf;

	//make . entry
	dp->inode = ino;
	dp->rec_len = 12;
	dp->name_len = 1;
	dp->name[0] = '.';
	//make .. entry
	dp = (DIR*)((char*)dp + 12);
	dp->inode = pip->ino;
	dp->rec_len = BLKSIZE - 12;
	dp->name_len = 2;
	dp->name[0] = dp->name[1] = '.';
	put_block(pip->dev, bno, buf);
	

	if (enter_name(pip, ino, name) != 0)
	{
		printf("mkdir error: failed to enter child\n");
		return -1;
	}
}

// from exercise guide
int make_dir()
{
	printf("========================================\n");
	printf("                           MKDIR   START\n");
	if (strlen(pathname) == 0)
	{
		printf("mkdir error: path is empty\n");
		return -1;
	}

	char dirpathcopy[64]; char basepathcopy[64];
	strcpy(dirpathcopy, pathname);
	strcpy(basepathcopy, pathname);
	char* parent = dirname(dirpathcopy);
	char* child = basename(basepathcopy);

	//determine relative or absolute
	if (pathname[0] == '/')
		dev = root->dev;
	else
		dev = running->cwd->dev;

	printf(" looking for parent %s\n", parent);
	//get parent ino

	int pino = getino(parent);
	MINODE* pmip = iget(dev, pino);
	if (!S_ISDIR(pmip->INODE.i_mode) && strcmp(parent, ".") != 0)
	{
		printf("mkdir error: parent path is not a directory\n");
		iput(pmip);
		return -1;
	}
	printf(" looking for child %s\n", child);
	if (search(pmip, child) != 0)
	{
		printf("mkdir error: directory name already exists \n");
		iput(pmip);
		return -1;
	}

	mymkdir(pmip, child);

	pmip->INODE.i_links_count++;
	pmip->dirty = 1;
	pmip->INODE.i_atime = time(0L);

	iput(pmip);
}

int my_creat(MINODE* pip, char* name)
{
	printf("we enter\n");
	int ino = ialloc(dev);
	int bno = balloc(dev);
	printf("> ino = %d, bmo = %d\n", ino, bno);
	MINODE* mip = iget(dev, ino);
	INODE* ip = &mip->INODE;

	//(4).2 Create INODE (textbook)
	ip->i_mode = 0x81A4;
	ip->i_uid = running->uid;
	ip->i_gid = running->gid;
	ip->i_size = 0;
	ip->i_links_count = 1; //FOR . AND ..
	ip->i_atime = ip->i_ctime = ip->i_mtime = time(0L);
	ip->i_blocks = 0;
	for (int x = 0; x < 15; x++)
		ip->i_block[x] = 0;
	mip->dirty = 1;
	iput(mip);
	printf("here, dev is %d\n",dev);
	if (enter_name(pip, ino, name) != 0)
	{
		printf("creat error: failed to enter child\n");
		return -1;
	}
	return 0;
	
}

int creat_file()
{
	printf("========================================\n");
	printf("                           CREAT   START\n");
	if (strlen(pathname) == 0)
	{
		printf("creat error: path is empty\n");
		return -1;
	}
	char dirpathcopy[64]; char basepathcopy[64];
	strcpy(dirpathcopy, pathname);
	strcpy(basepathcopy, pathname);
	char* parent = dirname(dirpathcopy);
	char* child = basename(basepathcopy);
	printf("at the start, dev is %d\n",dev);
	//determine relative or absolute
	if (pathname[0] == '/')
		dev = root->dev;
	else
		dev = running->cwd->dev;

	//get parent ino
	int pino = getino(parent);
	MINODE* pmip = iget(dev, pino);
	if (!S_ISDIR(pmip->INODE.i_mode) && strcmp(parent, ".") != 0)
	{
		printf("creat error: parent path is not a directory\n");
		iput(pmip);
		return -1;
	}
	if (search(pmip, child) != 0)
	{
		printf("creat error: file name already exists \n");
		iput(pmip);
		return -1;
	}

	my_creat(pmip, child);
	pmip->dirty = 1;
	pmip->INODE.i_atime = time(0L);

	iput(pmip);
}