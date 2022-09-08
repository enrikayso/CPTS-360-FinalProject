#include "type.h"

// global variables
MINODE minode[NMINODE], * root;
MTABLE mtable[NMTABLE];
PROC   proc[NPROC], * running;

int ninode, nblocks, bmap, imap, iblock;
int dev;
char gline[25], * name[16], pathname[64];// tokenized component string strings
int nname; // number of component strings
char* rootdev = "mydisk"; // default root_device
#include "util.c"
#include "mkdir_creat.c"
#include "rmdir.c"
#include "link_unlink.c"
#include "symlink.c"
#include "cd_ls_pwd.c"
#include "mount_unmount.c"
#include "open_close_lseek.c"
#include "read_cat.c"
#include "write_cp.c"

const char* local_cmd[] = { "man","ls","cd","pwd","mkdir","creat", "rmdir", "symlink","link","unlink","open","close","pfd","read","write","cat","cp","mount","cs","umount","quit" };

// Function provided by textbook
int find_cmd(char* command)
{
	int i = 0;
	while (local_cmd[i])
	{
		if (!strcmp(command, local_cmd[i]))
			return i; // found command: return index i
		i++;
	}
	return -1; // not found: return -1}
}

int fs_init()
{
	int i, j;
	for (i = 0; i < NMINODE; i++) // initialize all minodes as FREE
		minode[i].refCount = 0;
	for (i = 0; i < NMTABLE; i++) // initialize mtable entries as FREE
		mtable[i].dev = 0;
	for (i = 0; i < NPROC; i++)
	{ // initialize PROCs
		proc[i].status = READY; // reday to run
		proc[i].pid = i; // pid = 0 to NPROC-1
		proc[i].uid = i; // P0 is a superuser process
		for (j = 0; j < NFD; j++)
			proc[i].fd[j] = 0; // all file descriptors are NULL
		proc[i].next = &proc[i + 1];
	}
	proc[NPROC - 1].next = &proc[0]; // circular list
	running = &proc[0]; // P0 runs first
}

int quit()
{
	int i;
	MINODE* mip;
	for (i = 0; i < NMINODE; i++) {
		mip = &minode[i];
		if (mip->refCount > 0)
			iput(mip);
	}
	exit(0);
}

int mount_root(char* rootdev) // mount root file system
{
	printf("========================================\n");
	printf("                             MOUNT START\n");
	int i;
	MTABLE* mp;
	SUPER* sp;
	GD* gp;
	char buf[BLKSIZE];

	dev = open(rootdev, O_RDWR);
	if (dev < 0) {
		printf("panic : can’t open root device\n");
		exit(1);
	}
	/* get super block of rootdev */
	get_block(dev, 1, buf);
	sp = (SUPER*)buf;
	/* check magic number */
	if (sp->s_magic != SUPER_MAGIC) {
		printf("super magic=%x : %s is not an EXT2 filesys\n",
			sp->s_magic, rootdev);
		exit(0);
	}
	// fill mount table mtable[0] with rootdev information
	mp = &mtable[0]; // use mtable[0]
	mp->dev = dev;
	// copy super block info into mtable[0]
	ninode = mp->ninodes = sp->s_inodes_count;
	nblocks = mp->nblocks = sp->s_blocks_count;
	strcpy(mp->devName, rootdev);
	strcpy(mp->mntName, "/");
	get_block(dev, 2, buf);
	gp = (GD*)buf;
	bmap = mp->bmap = gp->bg_block_bitmap;
	imap = mp->imap = gp->bg_inode_bitmap;
	iblock = mp->iblock = gp->bg_inode_table;
	printf("bmap=%d imap=%d iblock=%d\n", bmap, imap, iblock);
	// call iget(), which inc minode’s refCount
	root = iget(dev, 2); // get root inode
	mp->mntDirPtr = root; // double link
	root->mptr = mp;
	// set proc CWDs
	for (i = 0; i < NPROC; i++) // set proc’s CWD
		proc[i].cwd = iget(dev, 2); // each inc refCount by 1
	printf("mount : %s mounted on / \n", rootdev);
	return 0;
}
int man()
{
	printf("========================================\n");
	printf("                            COMMAND LIST\n");
	printf("> man     : print the manual\n");
	printf("> ls      : list the target directory    : args = target_path\n");
	printf("> cd      : change the current directory : args = target_directory\n");
	printf("> pwd     : print the working directory\n");
	printf("> mkdir   : make a new directory         : args = target_path\n");
	printf("> creat   : create a new file            : args = target_path\n");
	printf("> rmdir   : remove a directory           : args = target_path\n");
	printf("> symlink : create a symlink w/ 2 files  : args = oldfile newfile\n");
	printf("> link    : create a link w/ 2 files     : args = oldfile newfile\n");
	printf("> unlink  : delete a linkfile            : args = target_path\n");
	printf("> open    : open a file                  : args = target_path 0|1|2|3 (R|W|R/W|A)\n");
	printf("> close   : close a file                 : args = fd\n");
	printf("> pfd     : display active files w/ fd \n");
	printf("> read    : read a file                  : args = target_path\n");
	printf("> write   : write to a file              : args = fd \"[char lim of 117]\"\n");
	printf("> cat     : cat a file (print contents)  : args = target_path\n");
	printf("> cp      : copy a file to a empty file  : args = oldfile newfile\n");
	printf("> mount   : mount to an empty directory  : args = diskname newfolder\n");
	printf("> unmount : unmount a disk               : args = diskname\n");
	printf("> cs      : switch users\n");
	printf("> quit    : exit simulation\n");
	return 0;
}

//change the currently running process.
int cs()
{
	printf("The previous user has uid: %d\n", running->uid);
	running = running->next; //
	printf("The current user has uid: %d\n", running->uid);
}

char disk[64];
int main(int argc, char* argv[])
{
	if (argv[1])
		strcpy(disk, argv[1]);
	else
		strcpy(disk, "diskimage");
	printf("loading %s\n", disk);
	int(*fptr[])() = { man,ls,my_chdir,my_pwd,make_dir,creat_file, my_rmdir, symlink_file,my_link, my_unlink,my_open,my_close_file,my_pfd,read_file,write_file,my_cat,cp,mount,cs,umount,quit };
	char line[128], cmd[16];
	if (argc > 1)
		rootdev = argv[1];
	fs_init();
	mount_root(disk);
	while (1)
	{
		printf("P%d running: ", running->pid);
		printf("input command : [for help enter \"man\"] ");
		fgets(line, 128, stdin);
		line[strlen(line) - 1] = 0;

		if (line[0] == 0)
			continue;
		pathname[0] = 0;

		sscanf(line, "%s %[^\n]", cmd, pathname);
		printf("cmd=%s pathname=%s\n", cmd, pathname);

		const int index = find_cmd(cmd);
		if (index >= 0)
		{
			switch (index)
			{
			case 1:
				fptr[index](pathname);
				break;
			case 2:
				fptr[index](pathname);
				break;
			case 6://rmdir
				fptr[index](pathname);
				break;
			case 9: //unlink
				fptr[index](pathname);
				break;
			case 11: //close
				fptr[index](atoi(pathname));
				break;
			case 15://cat
				fptr[index](pathname);
				break;
			case 16:
				fptr[index](pathname);
				break;
			case 19:
				fptr[index](pathname);
				break;

			default:
				fptr[index]();
			}
		}
		memset(&pathname[0], 0, sizeof(pathname));
	}
}