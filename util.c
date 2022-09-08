/*********** util.c file ****************/
// from textbook

#include <sys/types.h>
#include <unistd.h>
MINODE* mialloc()
{
	int i;
	for (i = 0; i < NMINODE; i++)
	{
		MINODE* mp = &minode[i];
		if (mp->refCount == 0)
		{
			mp->refCount = 1;
			return mp;
		}
	}
	printf("FS panic: out of minodes\n");
	return 0;
}

// from textbook
int midalloc(MINODE* mip)
{
	mip->refCount = 0;
}

// from textbook
int get_block(int dev, int blk, char* buf)
{
	lseek(dev, (long)blk * BLKSIZE, 0);
	int n = read(dev, buf, BLKSIZE);
}

// from textbook
int put_block(int dev, int blk, char* buf)
{
	lseek(dev, (long)blk * BLKSIZE, 0);
	int n = write(dev, buf, BLKSIZE);
}

// from textbook
int tokenize(char* pathname)
{
	memset(&name[0], 0, sizeof(nname));
	char* s;
	strcpy(gline, pathname);
	nname = 0;
	s = strtok(gline, "/");
	while (s) {
		name[nname++] = s;
		s = strtok(0, "/");
	}
}
int get_iblock_value()
{
	char buffer[BLKSIZE];
	get_block(dev, 2, buffer);
	gp = (GD*)buffer;
	return gp->bg_inode_table;//inodes begin block number
}

// // from textbook
MINODE* iget(int dev, int ino)
{
	char buf[BLKSIZE];
	// search in-memory minodes first
	for (int i = 0; i < NMINODE; i++)
	{
		MINODE* mip = &minode[i];
		if (mip->refCount && (mip->dev == dev) && (mip->ino == ino))
		{
			mip->refCount++;
			return mip;
		}
	}
	// needed INODE=(dev,ino) not in memory
	MINODE* mip = mialloc(); // allocate a FREE minode
	mip->dev = dev;
	mip->ino = ino; // assign to (dev, ino)
	const int block = (ino - 1) / 8 + iblock; // disk block containing this inode
	const int offset = (ino - 1) % 8; // which inode in this block
	get_block(dev, block, buf);
	INODE* ip = (INODE*)buf + offset;
	mip->INODE = *ip; // copy inode to minode.INODE
	// initialize minode
	mip->refCount = 1;
	mip->mounted = 0;
	mip->dirty = 0;
	mip->mptr = 0;
	return mip;
}

// from textbook
void iput(MINODE* mip)
{
	char buf[BLKSIZE];
	if (mip == 0) return;
	mip->refCount--; // dec refCount by 1
	if (mip->refCount > 0) return; // still has user
	if (mip->dirty == 0) return; // no need to write back
	// write INODE back to disk
	int block = (mip->ino - 1) / 8 + iblock;
	int offset = (mip->ino - 1) % 8;
	// get block containing this inode
	get_block(mip->dev, block, buf);
	INODE* ip = (INODE*)buf + offset; // ip points at INODE
	*ip = mip->INODE; // copy INODE to inode in block
	put_block(mip->dev, block, buf); // write back to disk
	midalloc(mip); // mip->refCount = 0;
}

// from textbook
int search(MINODE* mip, char* name)
{
	char temp[256], sbuf[BLKSIZE];
	for (int i = 0; i < 12; i++)
	{ // search DIR direct blocks only
		if (mip->INODE.i_block[i] == 0)
			return 0;
		get_block(mip->dev, mip->INODE.i_block[i], sbuf);
		DIR* dp = (DIR*)sbuf;
		char* cp = sbuf;
		while (cp < sbuf + BLKSIZE)
		{
			strncpy(temp, dp->name, dp->name_len);
			temp[dp->name_len] = 0;
			if (strcmp(name, temp) == 0)
				return dp->inode;

			cp += dp->rec_len;
			dp = (DIR*)cp;
		}
	}
	return 0;
}

// dev is a global
int getino(char* pathname)
{
	int i, ino, blk, disp;
	INODE* ip;
	MINODE* mip;

	//printf("getino: pathname=%s\n", pathname);
	if (strcmp(pathname, "/") == 0)
		return 2;

	// start mip = root or cwd
	if (pathname[0] == '/')
		mip = iget(dev, 2);
	else
		mip = iget(running->cwd->dev, running->cwd->ino);

	char gpath[64] = { 0 };

	strcpy(gpath, pathname);    // global gpath[128]
	tokenize(gpath);  // tokens are in global gpath[128]

	for (i = 0; i < nname; i++) {
		//printf("========================================\n");
		//printf("getino: i=%d name[%d]=%s\n", i, i, name[i]);
		  //if we have an upward traversal
		if (strcmp(name[i], "..") == 0)
		{
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
						printf("upward traversal\n");
						dev = mip->dev;
						break;
					}
				}
			}
		}
		ino = search(mip, name[i]);

		if (ino == 0)
		{
			iput(mip);
			printf("name %s does not exist\n", name[i]);
			return 0;
		}
		iput(mip);
		mip = iget(dev, ino);    // get next mip
		if (mip->mounted == 1)
		{
			iput(mip);
			printf("downward traversal\n");
			//follow the mount table pointer to locate the mount table entry
			dev = mip->mptr->dev;
			//iput(mip);
			mip = iget(dev, 2);
			ino = mip->ino;
		}
	}
	iput(mip);          // this is the last mip, its ino can NOT be zero
	return ino;         // this is the ino of last component
}

int findmyname(MINODE* parent_minode, u32 myino, char* my_name)
{
	char buf[BLKSIZE];
	for (int i = 0; i < EXT2_NDIR_BLOCKS; i++)
	{
		if (parent_minode->INODE.i_block[i] == 0)
			return 0;

		get_block(parent_minode->dev, parent_minode->INODE.i_block[i], buf);
		DIR* dp = (DIR*)buf;
		char* cp = buf;

		while (cp < buf + BLKSIZE) {
			if (dp->inode == myino)
			{
				strcpy(my_name, dp->name);
				return 1;
			}
			cp += dp->rec_len;
			dp = (DIR*)cp;
		}
		printf("\n");
	}
	return 0;
}

// myino = ino of . return ino of ..
int findino(MINODE* mip, u32* myino)
{
	if (!S_ISDIR(mip->INODE.i_mode) || mip == NULL)
	{
		return -1;
	}
	char buf[BLKSIZE];
	if (mip->INODE.i_block[0] == 0)
		return 0;

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
				//printf("upward traversal\n");
				dev = mip->dev;
				break;
			}
		}
	}
	get_block(mip->dev, mip->INODE.i_block[0], buf);
	DIR* dp = (DIR*)buf;
	char* cp = buf;
	*myino = dp->inode;
	cp += dp->rec_len;
	dp = (DIR*)cp;
	return dp->inode;
}

// developed from Chapter 11.3.1
int tst_bit(char* buf, int bit)
{
	const int i = bit / 8;
	const int j = bit % 8;
	if (buf[i] & (1 << j))
		return 1;
	return 0;
}

// developed from Chapter 11.3.1
int set_bit(char* buf, int bit)
{
	return buf[bit / 8] |= (1 << (bit % 8));
}

// developed from Chapter 11.3.1
int clr_bit(char* buf, int bit)
{
	return buf[bit / 8] &= ~(1 << (bit % 8));
}

// Provided by KC in mkdir guide.
int decFreeInodes(int dev)
{
	char buf[BLKSIZE];
	get_block(dev, 1, buf);
	sp = (SUPER*)buf;
	sp->s_free_inodes_count--;
	put_block(dev, 1, buf);
	get_block(dev, 2, buf);
	gp = (GD*)buf;
	gp->bg_free_inodes_count--;
	put_block(dev, 2, buf);
}

// exercise
int decFreeBlocks(int dev)
{
	char buf[BLKSIZE];
	get_block(dev, 1, buf);
	sp = (SUPER*)buf;
	sp->s_free_blocks_count--;
	put_block(dev, 1, buf);
	get_block(dev, 2, buf);
	gp = (GD*)buf;
	gp->bg_free_blocks_count--;
	put_block(dev, 2, buf);
}

// extrapolated from dec by harry
int incFreeInodes(int dev)
{
	char buf[BLKSIZE];
	get_block(dev, 1, buf);
	sp = (SUPER*)buf;
	sp->s_free_inodes_count++;
	put_block(dev, 1, buf);
	get_block(dev, 2, buf);
	gp = (GD*)buf;
	gp->bg_free_inodes_count++;
	put_block(dev, 2, buf);
}

// extrapolated from dec by harry
int incFreeBlocks(int dev)
{
	char buf[BLKSIZE];
	get_block(dev, 1, buf);
	sp = (SUPER*)buf;
	sp->s_free_blocks_count++;
	put_block(dev, 1, buf);
	get_block(dev, 2, buf);
	gp = (GD*)buf;
	gp->bg_free_blocks_count++;
	put_block(dev, 2, buf);
}

// Provided by KC in mkdir guide.
int ialloc(int dev)  // allocate an inode number from inode_bitmap
{
	char buf[BLKSIZE];

	// read inode_bitmap block
	get_block(dev, imap, buf);

	for (int i = 0; i < ninode; i++) {
		if (tst_bit(buf, i) == 0) {
			set_bit(buf, i);
			put_block(dev, imap, buf);
			printf("allocated ino = %d\n", i + 1); // bits count from 0; ino from 1
			decFreeInodes(dev);
			return i + 1;
		}
	}
	return 0;
}

// Provided by KC in rmdir guide.
int idalloc(int dev, int ino)
{
	int  i;
	char buf[BLKSIZE];

	if (ino > ninode)
	{
		printf("inumber %d out of range\n", ino);
		return -1;
	}

	get_block(dev, imap, buf);
	clr_bit(buf, ino - 1);
	put_block(dev, imap, buf);
	incFreeInodes(dev);
}

// exercise
int balloc(int dev)
{
	char buf[BLKSIZE];

	// read bnode_bitmap block
	get_block(dev, bmap, buf);

	for (int i = 0; i < nblocks; i++) {
		if (tst_bit(buf, i) == 0) {
			set_bit(buf, i);
			put_block(dev, bmap, buf);
			printf("allocated bit = %d\n", i + 1);
			decFreeBlocks(dev);
			return i + 1;
		}
	}
	return 0;
}

// exercise
int bdalloc(int dev, int bit)
{
	char buf[BLKSIZE];
	if (bit <= 0 || bit > BLKSIZE)
	{
		printf("bit %d out of range\n", bit);
		return -1;
	}

	get_block(dev, bmap, buf);
	clr_bit(buf, bit - 1);
	put_block(dev, bmap, buf);
	incFreeBlocks(dev);
}