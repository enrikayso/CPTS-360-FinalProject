int rm_child(MINODE* parent, char* childName)
{
	int found = 0;
	char buf[BLKSIZE], buf2[BLKSIZE], temp[256];

	memset(buf2, 0, BLKSIZE);

	printf("We want to remove %s\n", childName);

	for (int i = 0; i < 12; i++) {
		printf("i is currently %d\n", i);
		if (parent->INODE.i_block[i]) {
			get_block(parent->dev, parent->INODE.i_block[i], buf);

			char* cp = buf;
			char* cp2 = buf;
			DIR* dp = (DIR*)buf;
			DIR* dp2 = (DIR*)buf;
			DIR* prevdp = (DIR*)buf;

			while (cp < buf + BLKSIZE && found != 1) {
				memset(temp, 0, 256);
				strncpy(temp, dp->name, dp->name_len);
				//printf("Temp is: %s\n", temp);
				if (strcmp(childName, temp) == 0) {
					if (dp->rec_len == BLKSIZE && cp == buf) { //child only entry in block
						bdalloc(parent->dev, parent->INODE.i_block[i]);
						parent->INODE.i_block[i] = 0;
						parent->INODE.i_blocks--;
						found = 1;
					}
					else { //child is either last entry or in middle
						while (buf + BLKSIZE > dp2->rec_len + cp2) { //get to last element in block
							prevdp = dp2;
							cp2 += dp2->rec_len;
							dp2 = (DIR*)cp2;
						}

						if (dp == dp2) { //child is last entry
							prevdp->rec_len += dp->rec_len;
							found = 1;
						}
						else { //child in middle of block
							int size = ((buf + BLKSIZE) - (cp + dp->rec_len));
							dp2->rec_len += dp->rec_len;
							memmove(cp, (cp + dp->rec_len), size);
							prevdp = (DIR*)cp;
							memset(temp, 0, 256);
							strncpy(temp, prevdp->name, prevdp->name_len);
							found = 1;
						}
					}
				}
				cp += dp->rec_len;
				dp = (DIR*)cp;
			}
			if (found == 1) {
				put_block(parent->dev, parent->INODE.i_block[i], buf);
				return 1;
			}
		}
	}
	printf("Error: child not found\n");
	return 0;
}

int my_rmdir(char* pathname) {
	int counter = 0;
	int is_dir = 0;
	int not_busy = 0;
	int is_empty = 0;

	char dir_path_copy[64], base_path_copy[64];
	strcpy(dir_path_copy, pathname);
	strcpy(base_path_copy, pathname);
	char name[128];
	//****Get in-memory INODE of pathname (Textbook)
	//#Get inumber of pathname
	dev = running->cwd->dev;
	const int ino = getino(pathname);
	//determine dev
	//Get minode pointer
	MINODE* mip = iget(dev, ino);

	//Check ownership (Help page)
	if (running->uid != 0) {
		printf("This user does not the authority to remove DIR\n");
		return -1;
	}
	//Verify INODE is a DIR
	if (S_ISDIR(mip->INODE.i_mode)) {
		is_dir = 1;
	}

	//minode is not BUSY
	if (mip->refCount == 1) {
		not_busy = 1;
	}

	//verify DIR is empty, traverse data blocks
	if (mip->INODE.i_links_count == 2) {
		//if we reached a device root but not our root root
		if (mip->ino == 2 && dev != root->dev)
		{
			//look through mount table
			for (int j = 0; j < NMTABLE; j++)
			{
				//if a match found
				if (mtable[j].dev == dev)
				{
					iput(mip);
					mip = mtable[j].mntDirPtr;
					dev = mip->dev;
					break;
				}
			}
		}
		char buf[BLKSIZE];
		get_block(mip->dev, mip->INODE.i_block[0], buf);
		DIR* dp = (DIR*)buf;
		char* cp = buf;

		while (cp < buf + BLKSIZE) {
			//See if there are additional entries
			if (counter > 2) {
			  //printf("Inside counter>2. Counter = %d\n", counter);
				is_empty = 0;
			}
			else if (counter == 2) {
			  //printf("Inside counter==2. Counter = %d\n", counter);
				is_empty = 1;
			}
			counter++;
			//printf("In the while loop, counter is: %d\n", counter);
			cp += dp->rec_len;
			dp = (DIR*)cp;
		}
	}

	if (counter == 2) {
		is_empty = 1;
	}
	//printf("isDir=%d, notBusy=%d, isEmpty=%d\n", is_dir, not_busy, is_empty);

	//(5 on the help page)
	if (!is_dir || !not_busy || !is_empty) {
		iput(mip);
		//printf("Counter is: %d\n", counter);
		printf("REJECT, not empty, not a dir, or busy\n");
		return -1;
	}
	//we should do stuff here?
	char* child = basename(base_path_copy); //directory to be removed
	//get parent ino and minode
	char* parent = dirname(dir_path_copy);
	int pino = getino(parent);
	MINODE* pmip = iget(mip->dev, pino);
	//remove name from parent directory
	rm_child(pmip, child);
	//Deallocate block and inode (from help page)
	for (int i = 0; i < 12; i++) {
		if (mip->INODE.i_block[i] == 0) {
			break;
		}
		bdalloc(mip->dev, mip->INODE.i_block[i]);//mip->dev
	}
	idalloc(mip->dev, mip->ino);
	iput(mip); //Clears mip->refCount = 0

	//(9)
	pmip->INODE.i_links_count--;
	pmip->INODE.i_atime = time(0L);
	pmip->INODE.i_mtime = time(0L);
	pmip->dirty = 1;
	iput(pmip);
	return 1;
}
