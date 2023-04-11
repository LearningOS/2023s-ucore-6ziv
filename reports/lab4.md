# chapter6，7 练习

王哲威（[https://github.com/LearningOS/2023s-ucore-6ziv](https://github.com/LearningOS/2023s-ucore-6ziv)）

#### 编程作业1:合并chapter5的代码

和之前类似地，我们模仿`exec`函数实现`spawn`函数：

首先用`namei`获得可执行文件所在的`inode`：

```c
struct inode *ip;
if ((ip = namei(path)) == 0) {
    errorf("invalid file name %s\n", path);
    return -1;
}
```

接下来，我们`allocproc()`并填充相关字段：

```c
struct proc *np;
struct proc *p = curr_proc();
// Allocate process.
if ((np = allocproc()) == 0) {
	return -1;
}
init_stdio(np);
np->parent = p;
np->state = RUNNABLE;
np->max_page = 0;
```

`fork+exec`模式的进程创建会复制原来进程的`file descriptor`，所以不需要特意进行`init_stdio`来初始化相关句柄。正常情况下，`spawn`是否复制`fd`是由`fd`的`FD_CLOEXEC`标识决定的——但是我们没有实现这一标识，因此就默认采用了不复制文件标识符的策略。



最后，调用`bin_loader`载入可执行文件，调用`iput`释放`inode`的引用计数，通过`push_argv`载入程序参数并调用`add_task`。

因为我们提供的`spawn`的函数签名中不接受`argv`，所以我们手动进行构造：

`char* argv[2] = {path,NULL}`



```c
bin_loader(ip, np);
iput(ip);
char *argv[2] = { path, NULL };
push_argv(np, argv);
add_task(np);
return np->pid;
```

#### 编程作业2：添加硬链接的支持。

我们在`os/fs.h`和`nfs/fs.h`中更改`struct dinode`的定义：从`pad`中取出一个`short`用来记录硬链接数目。

于是，新的`dinode`定义如下：

```c
struct dinode {
	short type; // File type
	short nlink;
	short pad[2];
	uint size; // Size of file (bytes)
	uint addrs[NDIRECT + 1]; // Data block addresses
};
```

之后，我们更改`os/fs.c`和`nfs/fs.c`中的`ialloc`，将`dinode`的`nlink`初始化为`1`。特别地，在`nfs/fs.c`中需要考虑字节序的问题，所以用`xshort(1)`来初始化；而`os/fs.c`中则不需要。

同时，修改`os/file.h`中`struct inode`的定义，添加一个`short nlink`字段用来保存硬链接数目。



接下来，我们更新`ivalid`,`iupdate`以及`iput`。在`ivalid`中读取`ip->size`之后加上一行`ip->nlink = dip->nlink`，用来读取`nlink`；在`iupdate`中保存`dip->size`之后加上`dip->nlink = ip->nlink`，将`nlink`保存到硬盘。

同时，我们把`iput`中的注释去掉，此时”释放对应`inode`“的条件为：

`ip->ref == 1 && ip->valid && ip->nlink == 0`



之后，我们添加`dirunlink`的定义。在`dirunlink`中，首先进行`dirlookup`，确定要删除的文件是否存在，并记录文件对应的`inode`；然后遍历目录下的所有`dirent`，将匹配的`dirent`清空。最后，将`inode`的`nlink`减一，并通过`iput`释放当前函数持有的引用。

也就是说，如下所示：

```c
int dirunlink(struct inode *dp, char *name)
{
	infof("dirunlinking file %s", name);
	struct inode *ip;
	// Check that name is not present.
	if ((ip = dirlookup(dp, name, 0)) == 0) {
		errorf("dirunlink: file not found.");
		return -1;
	}
	int off;
	struct dirent de;
	for (off = 0; off < dp->size; off += sizeof(de)) {
		if (readi(dp, 0, (uint64)&de, off, sizeof(de)) != sizeof(de))
			panic("dirunlink read");
		if (0 == strncmp(name, de.name,sizeof(de.name))) {
			memset(&de, 0, sizeof(de));
			if (writei(dp, 0, (uint64)&de, off, sizeof(de)) != sizeof(de))
				panic("dirunlink write");
		}
	}
	ivalid(ip);
	--ip->nlink;
	iupdate(ip);
	iput(ip);
	return 0;
}
```



之后，我们在`file.h`中加入`struct Stat`的定义，声明`link,unlink,fstat`这三个函数，并定义宏`#define MAX_PATH (200)`。

我们实现上述三个函数：

在`int link(char *path, struct inode *ip)`函数中，先通过`dirlookup`确定目标文件名不存在；然后通过`dirlink`建立`path`对应的`dirent`项，并关联到`ip`上；最后，更新`ip->nlink`。

在`int unlink(char *path)`中，同样，先通过`dirlookup`确定目标文件存在，并获得其对应的`inode`。然后通过`dirunlink`释放即可。

在`int fstat(struct file *file, struct Stat *st)`中，根据`file`中的各个字段填充`st`并返回。



为了方便起见，我们在`syscall.c`中，分别给上面提到的三个函数添加了一层封装：对于`unlink`和`fstat`，我们只是简单地进行转发，并丢弃掉没有使用的参数。而对于`link`，我们在封装层中把传入的路径转换为`inode`。

最后，我们来实现这三个系统调用。对于`sys_linkat`和`sys_unlinkat`，我们分别把用到的路径复制到一个长度为`MAX_PATH`的`char`数组中，并传递给对应的函数即可；对于`sys_fstat`，我们利用传入的`fd`，从`struct proc`结构体中获取`struct file`，并传递给对应的实现函数，最后把得到的`struct Stat` 复制回传入的地址即可。



#### 问答题：

> 在我们的文件系统中，root inode起着什么作用？如果root inode中的内容损坏了，会发生什么？

实际上我们一切的文件操作都是在`root inode`的基础上向下逐层进行的。也就是说，`root inode`作为根节点，是我们通过`dirlookup`或者`namei`等函数访问文件的起点。如果`root inode`出错，文件系统仍然可以正常运行，但是我们无法正常访问到被影响的文件。



> 举出使用 pipe 的一个实际应用的例子。

* 我们可以通过`cat file|wc -l `来统计文件行数

* 我们可以通过`yes|some_command`来在程序运行过程中自动输入`y`

* 我们可以将`curl`得到的结果送入`grep`来查找特定的内容

* 我们也可以将`ls`的结果送进`xargs`，从而对每个文件运行一次特定的命令



> 如果需要在多个进程间互相通信，则需要为每一对进程建立一个管道，非常繁琐，请设计一个更易用的多进程通信机制。

我们可以为每个进程/线程维护一个消息队列，其它进程/线程通过进程id或者句柄向消息队列中发送消息。

我们也可以允许多进程之间共享内存，并通过信号量或者互斥量进行同步。
