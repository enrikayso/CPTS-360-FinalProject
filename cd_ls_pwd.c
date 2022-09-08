/************* cd_ls_pwd.c file **************/
char* t1 = "xwrxwrxwr-------";
char* t2 = "----------------";

//new chdir helper
int my_chdir(char* pathname)
{
	printf("========================================\n");
	printf("                           CHDIR   START\n");
	printf("cd to %s\n", pathname);

	// Determine DEV
	if (pathname[0] == '/')
		dev = root->dev;
	else
		dev = running->cwd->dev;

	// Get the MINODE
	int ino = getino(pathname);
	MINODE* mip = iget(dev, ino);

	if (S_ISDIR(mip->INODE.i_mode))
	{
		iput(running->cwd);
		running->cwd = mip;
	}
	else
	{
		printf("cd error: target is not a directory\n");
		return -1;
	}
	printf("cd success: cd to [%d %d] OK\n", mip->dev, mip->ino);
	return 0;
}

int ls_file(MINODE* mip, char* name)
{
	INODE* ip = &(mip->INODE);
	u16 mode = ip->i_mode;
	if ((mode & 0xF000) == 0x8000) // if (S_ISREG())
		printf("%c", '-');
	if ((mode & 0xF000) == 0x4000) // if (S_ISDIR())
		printf("%c", 'd');
	if ((mode & 0xF000) == 0xA000) // if (S_ISLNK())
		printf("%c", 'l');
	for (int i = 8; i >= 0; i--)
	{
		if (mode & (1 << i)) // print r|w|x
			printf("%c", t1[i]);
		else
			printf("%c", t2[i]); // or print
	}
	printf("%4d %4d %4d %8d ", ip->i_links_count, ip->i_gid, ip->i_uid, ip->i_size);
	// print time
	time_t mytime = mip->INODE.i_mtime;

	char ftime[64];
	strcpy(ftime, ctime(&mytime)); // print time in calendar form
	ftime[strlen(ftime) - 5] = 0; // kill \n at end
	printf("%s ", ftime);

	// print name
	printf("%s", basename(name)); // print file basename
	// print -> linkname if symbolic file
	if ((mode & 0xF000) == 0xA000)
	{
		// use readlink() to read linkname
		char linkname[1024];
		ssize_t len = my_readlink(name, linkname);
		if (len != -1)
		{
			linkname[len] = '\0';
			printf(" -> %s", linkname); // print linked name
		}
	}
	printf("\n");
	return 0;
}

int ls_dir(MINODE* mip)
{
	char buf[BLKSIZE];
	for (int i = 0; i < EXT2_NDIR_BLOCKS; i++)
	{
		if (mip->INODE.i_block[i] == 0)
			return 0;

		get_block(dev, mip->INODE.i_block[i], buf);
		DIR* dp = (DIR*)buf;
		char* cp = buf;

		while (cp < buf + BLKSIZE) {
			MINODE* lmip = iget(mip->dev, dp->inode);
			ls_file(lmip, dp->name);

			iput(lmip);
			cp += dp->rec_len;
			dp = (DIR*)cp;
		}
		printf("\n");
	}
	return 0;
}

int ls(char* pathname)
{
	printf("========================================\n");
	printf("                                LS START\n");
	printf("ls %s\n", pathname);
	dev = running->cwd->dev;
	MINODE* mip;

	//if we have a pathname
	if (strlen(pathname) != 0)
	{
		//if we are coming from the root
		if (pathname[0] == '/')
		{
			dev = root->dev;
		}
		int ino = getino(pathname);
		if (ino != 0)
			mip = iget(dev, ino);
		else
			return -1;

		if (S_ISDIR(mip->INODE.i_mode))
			ls_dir(mip);
		else
			ls_file(mip, pathname);
	}
	else
	{
		mip = iget(dev, running->cwd->ino);
		ls_dir(mip);
	}

	iput(mip);
	return 0;
}

int rpwd(MINODE* wd)
{
	if (wd == root)
	{
		return 0;
	}
	int my_ino;
	char my_name[128];
	dev = wd->dev;
	const int parent_ino = findino(wd, &my_ino);

	MINODE* pip = iget(dev, parent_ino);
	findmyname(pip, my_ino, my_name);
	rpwd(pip);
	iput(pip);
	printf("/%s", my_name);
	return 0;
}

int my_pwd()
{
	MINODE* wd = running->cwd;
	printf("========================================\n");
	printf("                              PWD  START\n");
	if (wd == root) {
		printf("/\n");
		return 0;
	}
	rpwd(running->cwd);
	printf("\n");
	return 0;
}