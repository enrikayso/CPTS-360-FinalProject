int symlink_file()
{
	printf("========================================\n");
	printf("                           SYMLINK START\n");
	if (strlen(pathname) == 0)
	{
		printf("link error: path is empty\n");
		return -1;;
	}
	int old_dev;
	int new_dev;
	char old_file[128] = { 0 };
	char new_file[128] = { 0 };
	sscanf(pathname, "%s %s", old_file, new_file);

	if (strlen(old_file) == 0 || strlen(new_file) == 0)
	{
		printf("symlink error: missing args\n");
		return -1;
	}

	//determine relative or absolute
	if (old_file[0] == '/')
		dev = root->dev;
	else
		dev = running->cwd->dev;

	printf(" looking for old_file %s\n", old_file);

	const int oino = getino(old_file);

	MINODE* omip = iget(dev, oino);

	// verify old_file exists and is not a DIR;
	if (S_ISDIR(omip->INODE.i_mode))
	{
		printf("symlink error: old path is a directory\n");
		iput(omip);
		return -1;;
	}

	if (new_file[0] == '/')
		dev = root->dev;
	else
		dev = running->cwd->dev;

	// new_file must not exist yet:
	int nino = getino(new_file);
	if (nino != 0)
	{
		printf("symlink error: new path already exists\n");
		iput(omip);
		return-1;;
	}
	strcpy(pathname, new_file);
	creat_file();
	nino = getino(new_file);
	MINODE* nmip = iget(dev, nino);
	nmip->INODE.i_size = strlen(old_file);

	//set type to symlink
	nmip->INODE.i_mode = nmip->INODE.i_mode & 0X0FFFF;

	nmip->INODE.i_mode = nmip->INODE.i_mode | 0xA000;

	//cast into i_block
	char* cp = (char*)(nmip->INODE.i_block);
	strcpy(cp, old_file);

	nmip->dirty = 1;
	iput(nmip);
	return 0;
}

int my_readlink(char* file, char* buffer)
{
	/************* Algorithm of readlink (file, buffer) *************/
	/*
	(1). get file’s INODE in memory; verify it’s a LNK file
	(2). copy target filename from INODE.i_block[ ] into buffer;
	(3). return file size;
	*/

	if (strlen(file) == 0)
	{
		printf("readlink error: path is empty\n");
		return -1;;
	}

	//determine relative or absolute
	if (file[0] == '/')
		dev = root->dev;
	else
		dev = running->cwd->dev;

	const int ino = getino(file);
	if (ino == 0)
	{
		printf("readlink error: path not found\n");
		return -1;
	}

	MINODE* mip = iget(dev, ino);

	if ((mip->INODE.i_mode & 0xA000) != 0xA000)
	{
		printf("readlink error: path is not a symlink\n");
		iput(mip);
		return -1;;
	}

	memset(&buffer[0], 0, sizeof(buffer));
	strcpy(buffer, (char*)(mip->INODE.i_block));
	iput(mip);
	return strlen(buffer);
}