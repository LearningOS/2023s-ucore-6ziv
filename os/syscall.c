#include "syscall.h"
#include "defs.h"
#include "loader.h"
#include "syscall_ids.h"
#include "timer.h"
#include "trap.h"
#include "proc.h"
#include "vm.h"
uint64 sys_write(int fd, uint64 va, uint len)
{
	debugf("sys_write fd = %d va = %x, len = %d", fd, va, len);
	if (fd != STDOUT)
		return -1;
	struct proc *p = curr_proc();
	char str[MAX_STR_LEN];
	int size = copyinstr(p->pagetable, str, va, MIN(len, MAX_STR_LEN));
	debugf("size = %d", size);
	for (int i = 0; i < size; ++i) {
		console_putchar(str[i]);
	}
	return size;
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
#define TRANSLATE_ADDR(TYPE, PTR)                                              \
	{                                                                      \
		struct proc *p = curr_proc();                                  \
		uint64 pa = useraddr(p->pagetable, (uint64)PTR);               \
		if (pa == 0)                                                   \
			panic("bad address");                                  \
		PTR = (TYPE *)pa;                                              \
	}
uint64 sys_gettimeofday(
	TimeVal *val,
	int _tz) // TODO: implement sys_gettimeofday in pagetable. (VA to PA)
{
	TRANSLATE_ADDR(TimeVal, val);
	//struct proc *p = curr_proc();
	//uint64 pa = useraddr(p->pagetable, (uint64)val);
	//val = (TimeVal *)pa;
	// YOUR CODE
	uint64 cycle = get_cycle();
	val->sec = cycle / CPU_FREQ;
	val->usec = (cycle % CPU_FREQ) * 1000000 / CPU_FREQ;

	/* The code in `ch3` will leads to memory bugs*/

	// uint64 cycle = get_cycle();
	// val->sec = cycle / CPU_FREQ;
	// val->usec = (cycle % CPU_FREQ) * 1000000 / CPU_FREQ;
	return 0;
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
// TODO: add support for mmap and munmap syscall.
// hint: read through docstrings in vm.c. Watching CH4 video may also help.
// Note the return value and PTE flags (especially U,X,W,R)
/*
* LAB1: you may need to define sys_task_info here
*/
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
uint64 sys_getpid()
{
	return curr_proc()->pid;
}
uint64 sys_task_info(TaskInfo *ti)
{
	TRANSLATE_ADDR(TaskInfo, ti);
	ti->status = get_task_status(curr_proc());

#ifdef ONLY_RUNNING_TIME
	ti->time = ((curr_proc()->state == RUNNING) ?
			    (get_cycle() / (CPU_FREQ / 1000) -
			     curr_proc()->time_scheduled) :
			    0) +
		   curr_proc()->total_used_time;
#else
	ti->time =
		get_cycle() / (CPU_FREQ / 1000) - curr_proc()->time_scheduled;
#endif
	memmove(ti->syscall_times, curr_proc()->syscall_counter,
		sizeof(unsigned int) * MAX_SYSCALL_NUM);
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
	/*
	* LAB1: you may need to update syscall counter for task info here
	*/
	++(curr_proc()->syscall_counter[id]);
	switch (id) {
	case SYS_write:
		ret = sys_write(args[0], args[1], args[2]);
		break;
	case SYS_exit:
		sys_exit(args[0]);
		// __builtin_unreachable();
	case SYS_sched_yield:
		ret = sys_sched_yield();
		break;
	case SYS_gettimeofday:
		ret = sys_gettimeofday((TimeVal *)args[0], args[1]);
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
	case SYS_getpid:
		ret = sys_getpid();
		break;
	/*
	* LAB1: you may need to add SYS_taskinfo case here
	*/
	default:
		ret = -1;
		errorf("unknown syscall %d", id);
	}
	trapframe->a0 = ret;
	tracef("syscall ret %d", ret);
}
