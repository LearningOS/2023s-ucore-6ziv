#include "syscall.h"
#include "console.h"
#include "defs.h"
#include "loader.h"
#include "syscall_ids.h"
#include "timer.h"
#include "trap.h"
#include "vm.h"

uint64 console_write(uint64 va, uint64 len)
{
	struct proc *p = curr_proc();
	char str[MAX_STR_LEN];
	int size = copyinstr(p->pagetable, str, va, MIN(len, MAX_STR_LEN));
	tracef("write size = %d", size);
	for (int i = 0; i < size; ++i) {
		console_putchar(str[i]);
	}
	return len;
}

uint64 console_read(uint64 va, uint64 len)
{
	struct proc *p = curr_proc();
	char str[MAX_STR_LEN];
	tracef("read size = %d", len);
	for (int i = 0; i < len; ++i) {
		int c = consgetc();
		str[i] = c;
	}
	copyout(p->pagetable, va, str, len);
	return len;
}

uint64 sys_write(int fd, uint64 va, uint64 len)
{
	if (fd < 0 || fd > FD_BUFFER_SIZE)
		return -1;
	struct proc *p = curr_proc();
	struct file *f = p->files[fd];
	if (f == NULL) {
		errorf("invalid fd %d\n", fd);
		return -1;
	}
	switch (f->type) {
	case FD_STDIO:
		return console_write(va, len);
	case FD_INODE:
		return inodewrite(f, va, len);
	default:
		panic("unknown file type %d\n", f->type);
	}
}

uint64 sys_read(int fd, uint64 va, uint64 len)
{
	if (fd < 0 || fd > FD_BUFFER_SIZE)
		return -1;
	struct proc *p = curr_proc();
	struct file *f = p->files[fd];
	if (f == NULL) {
		errorf("invalid fd %d\n", fd);
		return -1;
	}
	switch (f->type) {
	case FD_STDIO:
		return console_read(va, len);
	case FD_INODE:
		return inoderead(f, va, len);
	default:
		panic("unknown file type %d\n", f->type);
	}
}

__attribute__((noreturn)) void sys_exit(int code)
{
	exit(code);
	__builtin_unreachable();
}

uint64 sys_sched_yield()
{
	yield();
	return 0;
}

uint64 sys_gettimeofday(uint64 val, int _tz)
{
	struct proc *p = curr_proc();
	uint64 cycle = get_cycle();
	TimeVal t;
	t.sec = cycle / CPU_FREQ;
	t.usec = (cycle % CPU_FREQ) * 1000000 / CPU_FREQ;
	copyout(p->pagetable, val, (char *)&t, sizeof(TimeVal));
	return 0;
}

uint64 sys_getpid()
{
	return curr_proc()->pid;
}

uint64 sys_getppid()
{
	struct proc *p = curr_proc();
	return p->parent == NULL ? IDLE_PID : p->parent->pid;
}

uint64 sys_clone()
{
	debugf("fork!");
	return fork();
}

static inline uint64 fetchaddr(pagetable_t pagetable, uint64 va)
{
	uint64 *addr = (uint64 *)useraddr(pagetable, va);
	return *addr;
}

uint64 sys_exec(uint64 path, uint64 uargv)
{
	struct proc *p = curr_proc();
	char name[MAX_STR_LEN];
	copyinstr(p->pagetable, name, path, MAX_STR_LEN);
	uint64 arg;
	static char strpool[MAX_ARG_NUM][MAX_STR_LEN];
	char *argv[MAX_ARG_NUM];
	int i;
	for (i = 0; uargv && (arg = fetchaddr(p->pagetable, uargv));
	     uargv += sizeof(char *), i++) {
		copyinstr(p->pagetable, (char *)strpool[i], arg, MAX_STR_LEN);
		argv[i] = (char *)strpool[i];
	}
	argv[i] = NULL;
	return exec(name, (char **)argv);
}

uint64 sys_wait(int pid, uint64 va)
{
	struct proc *p = curr_proc();
	int *code = (int *)useraddr(p->pagetable, va);
	return wait(pid, code);
}

uint64 sys_spawn(uint64 va)
{
	// TODO: your job is to complete the sys call
	struct proc *p = curr_proc();
	char name[200];
	copyinstr(p->pagetable, name, va, 200);
	return spawn(name);
}

uint64 sys_set_priority(long long prio)
{
	// TODO: your job is to complete the sys call
	if (prio >= 2 && prio <= INT_MAX) {
		struct proc *p = curr_proc();
		p->pass = BIG_STRIDE / prio;
		return prio;
	}
	return -1;
}

uint64 sys_openat(uint64 va, uint64 omode, uint64 _flags)
{
	struct proc *p = curr_proc();
	char path[200];
	copyinstr(p->pagetable, path, va, 200);
	return fileopen(path, omode);
}

uint64 sys_close(int fd)
{
	if (fd < 0 || fd > FD_BUFFER_SIZE)
		return -1;
	struct proc *p = curr_proc();
	struct file *f = p->files[fd];
	if (f == NULL) {
		errorf("invalid fd %d", fd);
		return -1;
	}
	fileclose(f);
	p->files[fd] = 0;
	return 0;
}

#define UNUSED(x) (void)(x)
int linkat(int olddirfd, char *oldpath, int newdirfd, char *newpath,
	   unsigned int flags)
{
	UNUSED(olddirfd);
	UNUSED(newdirfd);
	UNUSED(flags);

	struct inode *old_inode = namei(oldpath);
	if (old_inode == NULL) {
		errorf("linked file does not exist.");
		return -1;
	}
	int ret = link(newpath, old_inode);
	iput(old_inode);
	return ret;
}
int unlinkat(int dirfd, char *path, unsigned int flags)
{
	UNUSED(dirfd);
	UNUSED(flags);

	return unlink(path);
}
int fstatat(struct file *file, struct Stat *st)
{
	return fstat(file, st);
}

int sys_fstat(int fd, uint64 stat)
{
	//TODO: your job is to complete the syscall
	struct proc *p = curr_proc();
	struct file *f = p->files[fd];
	if (f == NULL) {
		errorf("invalid fd %d", fd);
		return -1;
	}
	struct Stat istat;
	fstatat(f, &istat);
	copyout(p->pagetable, stat, (char *)(&istat), sizeof(struct Stat));
	//fstatat()
	return 0;
	//return -1;
}

int sys_linkat(int olddirfd, uint64 oldpath, int newdirfd, uint64 newpath,
	       uint64 flags)
{
	char old_path[MAX_PATH];
	char new_path[MAX_PATH];
	struct proc *p = curr_proc();
	copyinstr(p->pagetable, old_path, oldpath, MAX_PATH);
	copyinstr(p->pagetable, new_path, newpath, MAX_PATH);
	return linkat(olddirfd, old_path, newdirfd, new_path, flags);
	//TODO: your job is to complete the syscall
	//return -1;
}

int sys_unlinkat(int dirfd, uint64 name, uint64 flags)
{
	char filepath[MAX_PATH];
	struct proc *p = curr_proc();
	copyinstr(p->pagetable, filepath, name, MAX_PATH);
	return unlinkat(dirfd, filepath, flags);
	//TODO: your job is to complete the syscall
	//return -1;
}

uint64 sys_sbrk(int n)
{
	uint64 addr;
	struct proc *p = curr_proc();
	addr = p->program_brk;
	if (growproc(n) < 0)
		return -1;
	return addr;
}
inline int translate_port(int port)
{
	return (port << 1) | PTE_U;
}
int sys_mmap(void *start, unsigned long long len, int port, int flag, int fd)
{
	struct proc *p = curr_proc();

	if (0 != (((uint64)start) % PGSIZE)) {
		errorf("not aligned");
		return -1;
	}
	if (len > 0x40000000) {
		errorf("mmaping more than 1GiB");
		return -1;
	}
	if (((port & (~0x7)) != 0) || ((port & 0x7) == 0)) {
		errorf("Bad port %x", port);
		return -1;
	}
	uint64 pages = (len + PGSIZE - 1) / PGSIZE;
	for (uint64 j = 0; j < pages; j++) {
		void *ptr = kalloc();
		if (ptr == (void *)0) {
			//
			uvmunmap(p->pagetable, (uint64)start, j, 1);
			errorf("no enough memory");
			return -1;
		}
		if (0 != mappages(p->pagetable, (uint64)start + j * PGSIZE,
				  PGSIZE, (uint64)ptr, translate_port(port))) {
			//
			kfree(ptr);
			uvmunmap(p->pagetable, (uint64)start, j, 1);
			errorf("cannot map memory");
			return -1;
		}
		uint64 page_id = ((uint64)start) / PGSIZE + j;
		if (page_id >= p->max_page)
			p->max_page = page_id + 1;
	}
	return 0;
}
int sys_munmap(void *start, unsigned long long len)
{
	struct proc *p = curr_proc();

	if (0 != (uint64)start % PGSIZE) {
		errorf("not aligned");
		return -1;
	}
	if (len > 0x40000000) {
		errorf("munmaping more than 1GiB");
		return -1;
	}

	uint64 pages = (len + PGSIZE - 1) / PGSIZE;
	for (uint64 j = 0; j < pages; j++) {
		if (0 == walkaddr(p->pagetable, (uint64)(start + j * PGSIZE)))
			return -1;
	}

	uvmunmap(p->pagetable, (uint64)start, pages, 1);
	return 0;
}
inline TaskStatus get_task_status(struct proc *p)
{
	switch (p->state) {
	case RUNNING:
		return Running;
	case SLEEPING:
	case RUNNABLE:
		if (p->time_scheduled == (uint64)-1)
			return UnInit;
		else
			return Ready;
	case ZOMBIE:
		return Exited;
	case UNUSED:
	case USED:
	default:
		panic("Unexpected task statis %d.", p->state);
		return Exited;
	}
}

uint64 sys_task_info(TaskInfo *ti)
{
	struct proc *p = curr_proc();

	TaskInfo tti;
	//TRANSLATE_ADDR(TaskInfo, ti);
	tti.status = get_task_status(p);

#ifdef ONLY_RUNNING_TIME
	tti.time =
		((p->state == RUNNING) ?
			 (get_cycle() / (CPU_FREQ / 1000) - p->time_scheduled) :
			 0) +
		p->total_used_time;
#else
	tti.time = get_cycle() / (CPU_FREQ / 1000) - p->time_scheduled;
#endif
	memmove(tti.syscall_times, p->syscall_counter,
		sizeof(unsigned int) * MAX_SYSCALL_NUM);
	copyout(p->pagetable, (uint64)ti, (char *)&tti, sizeof(TaskInfo));
	return 0;
}

extern char trap_page[];

void syscall()
{
	struct trapframe *trapframe = curr_proc()->trapframe;
	int id = trapframe->a7, ret;
	uint64 args[6] = { trapframe->a0, trapframe->a1, trapframe->a2,
			   trapframe->a3, trapframe->a4, trapframe->a5 };
	tracef("syscall %d args = [%x, %x, %x, %x, %x, %x]", id, args[0],
	       args[1], args[2], args[3], args[4], args[5]);
	++(curr_proc()->syscall_counter[id]);
	switch (id) {
	case SYS_write:
		ret = sys_write(args[0], args[1], args[2]);
		break;
	case SYS_read:
		ret = sys_read(args[0], args[1], args[2]);
		break;
	case SYS_openat:
		ret = sys_openat(args[0], args[1], args[2]);
		break;
	case SYS_close:
		ret = sys_close(args[0]);
		break;
	case SYS_exit:
		sys_exit(args[0]);
		// __builtin_unreachable();
	case SYS_sched_yield:
		ret = sys_sched_yield();
		break;
	case SYS_gettimeofday:
		ret = sys_gettimeofday(args[0], args[1]);
		break;
	case SYS_getpid:
		ret = sys_getpid();
		break;
	case SYS_getppid:
		ret = sys_getppid();
		break;
	case SYS_clone: // SYS_fork
		ret = sys_clone();
		break;
	case SYS_execve:
		ret = sys_exec(args[0], args[1]);
		break;
	case SYS_wait4:
		ret = sys_wait(args[0], args[1]);
		break;
	case SYS_fstat:
		ret = sys_fstat(args[0], args[1]);
		break;
	case SYS_linkat:
		ret = sys_linkat(args[0], args[1], args[2], args[3], args[4]);
		break;
	case SYS_unlinkat:
		ret = sys_unlinkat(args[0], args[1], args[2]);
		break;
	case SYS_spawn:
		ret = sys_spawn(args[0]);
		break;
	case SYS_sbrk:
		ret = sys_sbrk(args[0]);
		break;
	case SYS_mmap:
		ret = sys_mmap((void *)args[0], args[1], args[2], args[3],
			       args[4]);
		break;
	case SYS_munmap:
		ret = sys_munmap((void *)args[0], args[1]);
		break;
	case SYS_task_info:
		ret = sys_task_info((TaskInfo *)args[0]);
		break;
	case SYS_setpriority:
		ret = sys_set_priority((long long)args[0]);
		break;
	default:
		ret = -1;
		errorf("unknown syscall %d", id);
	}
	trapframe->a0 = ret;
	tracef("syscall ret %d", ret);
}
