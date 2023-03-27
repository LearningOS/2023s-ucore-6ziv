#include "syscall.h"
#include "defs.h"
#include "loader.h"
#include "syscall_ids.h"
#include "timer.h"
#include "trap.h"
#include "proc.h"
uint64 sys_write(int fd, char *str, uint len)
{
	debugf("sys_write fd = %d str = %x, len = %d", fd, str, len);
	if (fd != STDOUT)
		return -1;
	for (int i = 0; i < len; ++i) {
		console_putchar(str[i]);
	}
	return len;
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

uint64 sys_gettimeofday(TimeVal *val, int _tz)
{
	uint64 cycle = get_cycle();
	val->sec = cycle / CPU_FREQ;
	val->usec = (cycle % CPU_FREQ) * 1000000 / CPU_FREQ;
	return 0;
}

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
	ti->status = get_task_status(curr_proc());
	
#ifdef ONLY_RUNNING_TIME
	ti->time = ((curr_proc()->state == RUNNING) ?
			    (get_cycle() / (CPU_FREQ / 1000) -
			     curr_proc()->time_scheduled) :
			    0) +
		   curr_proc()->total_used_time;
#else
	ti->time = get_cycle() / (CPU_FREQ / 1000) -
		   curr_proc()->time_scheduled;
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
		ret = sys_write(args[0], (char *)args[1], args[2]);
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
