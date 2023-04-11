#ifndef FILE_H
#define FILE_H

#include "fs.h"
#include "proc.h"
#include "types.h"

#define PIPESIZE (512)
#define FILEPOOLSIZE (NPROC * FD_BUFFER_SIZE)

// in-memory copy of an inode,it can be used to quickly locate file entities on disk
struct inode {
	uint dev; // Device number
	uint inum; // Inode number
	int ref; // Reference count
	int valid; // inode has been read from disk?
	short type; // copy of disk inode
	short nlink;
	uint size;
	uint addrs[NDIRECT + 1];
	// LAB4: You may need to add link count here
};

// Defines a file in memory that provides information about the current use of the file and the corresponding inode location
struct file {
	enum { FD_NONE = 0, FD_INODE, FD_STDIO } type;
	int ref; // reference count
	char readable;
	char writable;
	struct inode *ip; // FD_INODE
	uint off;
};

//A few specific fd
enum {
	STDIN = 0,
	STDOUT = 1,
	STDERR = 2,
};
struct Stat {
	uint64 dev; // 文件所在磁盘驱动号，该实现写死为 0 即可。
	uint64 ino; // inode 文件所在 inode 编号
	uint32 mode; // 文件类型
	uint32 nlink; // 硬链接数量，初始为1
	uint64 pad[7]; // 无需考虑，为了兼容性设计
};

extern struct file filepool[FILEPOOLSIZE];

void fileclose(struct file *);
struct file *filealloc();
int fileopen(char *, uint64);
uint64 inodewrite(struct file *, uint64, uint64);
uint64 inoderead(struct file *, uint64, uint64);
struct file *stdio_init(int);
int show_all_files();
int link(char *path, struct inode *ip);
int unlink(char *path);
int fstat(struct file* file, struct Stat *st);
#define MAX_PATH (200)
#endif // FILE_H