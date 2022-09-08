int mount()
{
	printf("========================================\n");
	printf("                              MOUNT START\n");

	//if no parameter display current mounted systems
	if (strlen(pathname) == 0)
	{
		for (int i = 0; i < NMTABLE; i++)
		{
			if (mtable[i].dev == 0)
				return 0;
			printf("Mount Name: %s, Device Name %s\n", mtable[i].mntName, mtable[i].devName);
		}
		return 0;;
	}

	char filename[128] = { 0 };
	char mountfile[128] = { 0 };
	sscanf(pathname, "%s %s", filename, mountfile);
	//check sscanf results
	if (strlen(filename) == 0 || strlen(mountfile) == 0)
	{
		printf("mount error: bad sscanf\n");
		return -1;;
	}
	int i;
	for (i = 0; i < NMTABLE; i++)
	{
		if (mtable[i].dev == 0)
			break;
		if (strcmp(mtable[i].devName, filename) == 0)
		{
			printf("mount error: filesys already mounted\n");
			return -1;
		}
	}
	if (i + 1 == NMTABLE)
	{
		printf("mount error: max mntables reached\n");
		return -1;
	}
	MTABLE* mp;
	mp = &mtable[i];

	SUPER* sp;
	char buf[BLKSIZE];

	int ldev = open(filename, O_RDWR);
	if (ldev < 0) {
		printf("panic : can’t open root device\n");
		return -1;
	}

	/* get super block */
	get_block(ldev, 1, buf);
	sp = (SUPER*)buf;
	/* check magic number */
	if (sp->s_magic != SUPER_MAGIC) {
		printf("super magic=%x : %s is not an EXT2 filesys\n",
			sp->s_magic, rootdev);
		return -1;
	}

	//determine relative or absolute
	if (mountfile[0] == '/')
		dev = root->dev;
	else
		dev = running->cwd->dev;
	int ino = getino(mountfile);
	if (ino == 0)
	{
		printf("mount error : file does not exist\n");
		return -1;
	}
	MINODE* mip = iget(dev, ino);
	printf("the minode ino is %d\n", ino);
	if (!S_ISDIR(mip->INODE.i_mode))
	{
		printf("mount error : not a directory\n");
		iput(mip);
		return -1;
	}

	for (int j = 0; j < NPROC; j++)
	{
		printf("the proc ino is %d\n", proc[j].cwd->ino);
		if (proc[j].cwd->ino == ino)
		{
			printf("mount error : directory busy\n");
			iput(mip);
			return -1;
		}
	}

	mip->mptr = mp;
	strcpy(mp->devName, filename);
	strcpy(mp->mntName, mountfile);
	mp->dev = ldev;
	// copy super block info into mtable[0]
	mp->ninodes = sp->s_inodes_count;
	mp->nblocks = sp->s_blocks_count;

	get_block(dev, 2, buf);
	GD* gp = (GD*)buf;
	mp->bmap = gp->bg_block_bitmap;
	mp->imap = gp->bg_inode_bitmap;
	mp->iblock = gp->bg_inode_table;

	mp->mntDirPtr = mip;
	mip->mounted = 1;
	//printf(" THIS MIP IS %d\n",mip->mounted);
	printf("bmap=%d imap=%d iblock=%d\n", mp->bmap, mp->imap, mp->iblock);
	//iput(mip);
	printf("mount success\n");
	return 0;
}

int umount(char* filesys) {
	printf("========================================\n");
	printf("                              UMOUNT START\n");
	MTABLE* mp;
	MINODE* mip;
	int checkMount = 0;
	int i, index = 0;

	for (i = 0; i < NMTABLE; i++) {
		if (mtable[i].dev == 0) {
			break;
		}
		if (strcmp(mtable[i].devName, filesys) == 0) {
			mp = &mtable[i];
			checkMount = 1;
			index = i;
			printf("%s will be unmounted from %s\n", mtable[i].devName, mtable[i].mntName);
			break;
		}
	}

	if (checkMount == 0) {
		printf("%s is not mounted\n", filesys);
		return-1;
	}

	if (running->cwd->dev == mp->dev) {
		printf("Error: %s is still active, cannot unmount\n", filesys);
		return -1;
	}

	mip = mp->mntDirPtr;
	mp->dev = 0;
	mp->mntDirPtr->mounted = 0;
	iput(mip);
	printf("%s has been successfully unmounted\n", filesys);

	return 0;//success
}