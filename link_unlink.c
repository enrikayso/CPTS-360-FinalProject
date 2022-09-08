int my_link()
{
	printf("========================================\n");
	printf("                              LINK START\n");
	if (strlen(pathname) == 0)
	{
		printf("link error: path is empty\n");
		return -1;
	}

	char old_file[128];
	char new_file[128];
	sscanf(pathname, "%s %s", old_file, new_file);

	if (strlen(old_file) == 0 || strlen(new_file) == 0)
	{
		printf("link error: missing args\n");
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
		printf("link error: old path is a directory\n");
		iput(omip);
		return-1;
	}
	int old_dev = dev;
	if (new_file[0] == '/')
		dev = root->dev;
	else
		dev = running->cwd->dev;

	printf(" looking for new_file %s\n", new_file);
	// new_file must not exist yet:
	const int nino = getino(new_file);
	if (nino != 0)
	{
		printf("link error: new path already exists\n");
		iput(omip);
		return -1;
	}
	//(3). create new_file with the same inode number of old_file:
	char new_file_copy[64];
	strcpy(new_file_copy, new_file);
	char* parent = dirname(new_file); char* child = basename(new_file_copy);

	const int pino = getino(parent);
	if (dev != old_dev)
	{
		printf("link error: devs don't match\n");
		iput(omip);
		return -1;
	}

	MINODE* pmip = iget(dev, pino);
	// creat entry in new parent DIR with same inode number of old_file

	if (enter_name(pmip, omip->ino, child) != 0)
	{
		printf("mkdir error: failed to enter child\n");
		iput(omip);
		iput(pmip);
		return -1;
	}

	omip->INODE.i_links_count++;
	omip->dirty = 1; // for write back by iput(omip)
	iput(omip);
	iput(pmip);
	return 0;
}

int my_unlink(char* filename) {
	dev = running->cwd->dev;
	//get file_name's minode
	int ino = getino(filename);
	if (ino == 0) {
		printf("Error: File does not exist.\n");
		return 0;
	}

	MINODE* mip = iget(dev, ino);

	//check its a REG or symbolic LNK file; Cannot be a DIR
	if (S_ISDIR(mip->INODE.i_mode)) {
		printf("Error: Is a DIR");
		return -1;
	}

	if(running->uid != 0){
	  printf("Error, user does not have access to this\n");
	  return 0;
	}

	//remove name entry from parent DIR's data block;
	char* parent = dirname(filename);
	char* child = basename(filename);
	int old_dev = dev;
	dev = mip->dev;
	const int pino = getino(parent);
	if (dev != old_dev)
		dev = old_dev;

	MINODE* pmip = iget(dev, pino);
	rm_child(pmip, child);
	pmip->dirty = 1;
	iput(pmip);

	//decrement INODES link_count by 1
	mip->INODE.i_links_count--;

	//Step 4 in text book
	if (mip->INODE.i_links_count > 0) {
		//remove file name
		mip->dirty = 1;
	}
	else { //links_count = 0: remove filename
	  //deallocate all data blocks in INODE;
		for (int i = 0; i < 12; i++) {
			if (mip->INODE.i_block[i] == 0) {
				continue;
			}
			bdalloc(mip->dev, mip->INODE.i_block[i]);
		}
		//deallocate INODE;
		idalloc(mip->dev, mip->ino);
	}
	iput(mip); //release mip
	return 0;
}
