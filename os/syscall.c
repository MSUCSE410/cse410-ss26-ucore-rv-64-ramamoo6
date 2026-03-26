#include "syscall.h"
#include "defs.h"
#include "loader.h"
#include "syscall_ids.h"
#include "timer.h"
#include "trap.h"
#include "vm.h"
#include "kalloc.h"

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

uint64 sys_gettimeofday(TimeVal *val, int _tz) // TODO: implement sys_gettimeofday in pagetable. (VA to PA)
{
    struct proc *p = curr_proc();
    uint64 cycle = get_cycle();
    TimeVal kval;
    kval.sec = cycle / CPU_FREQ;
    kval.usec = (cycle % CPU_FREQ) * 1000000 / CPU_FREQ;
    copyout(p->pagetable, (uint64)val, (char *)&kval, sizeof(TimeVal));
    return 0;
}

// TODO: add support for mmap and munmap syscall.
// hint: read through docstrings in vm.c. Watching CH4 video may also help.
// Note the return value and PTE flags (especially U,X,W,R)
/*
* LAB1: you may need to define sys_task_info here
*/
uint64 sys_task_info(TaskInfo *ti)
{
    struct proc *p = curr_proc();
    TaskInfo kinfo;
    kinfo.status = Running;
    kinfo.time = (int)(get_cycle() * 1000 / CPU_FREQ - p->first_time);
    memmove(kinfo.syscall_times, p->syscall_times, sizeof(p->syscall_times));
    copyout(p->pagetable, (uint64)ti, (char *)&kinfo, sizeof(TaskInfo));
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
	curr_proc()->syscall_times[id]++;
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
	/*
	* LAB1: you may need to add SYS_taskinfo case here
	*/
	case SYS_task_info:
		ret = sys_task_info((TaskInfo *)args[0]);
		break;

	case SYS_mmap:
		ret = sys_mmap(args[0], args[1], args[2], args[3], args[4]);
		break;
	case SYS_munmap:
		ret = sys_munmap(args[0], args[1]);
		break;
	default:
		ret = -1;
		errorf("unknown syscall %d", id);
	}
	trapframe->a0 = ret;
	tracef("syscall ret %d", ret);
}

uint64 sys_mmap(uint64 start, uint64 len, int port, int flag, int fd)
{
    if (len == 0) return 0;
    if (port & ~0x7) return -1;        
    if (!(port & 0x7)) return -1;      
    if (start % PGSIZE != 0) return -1;
    if (len > 1024*1024*1024) return -1;

    struct proc *p = curr_proc();
    int perm = PTE_U;
    if (port & 1) perm |= PTE_R;
    if (port & 2) perm |= PTE_W;
    if (port & 4) perm |= PTE_X;

    uint64 pages = (len + PGSIZE - 1) / PGSIZE;
    for (uint64 i = 0; i < pages; i++) {
        void *pa = kalloc();
        if (pa == 0) return -1;
        memset(pa, 0, PGSIZE);
        if (mappages(p->pagetable, start + i * PGSIZE, PGSIZE, (uint64)pa, perm) != 0) {
            kfree(pa);
            return -1;
        }
    }
    return 0;
}

uint64 sys_munmap(uint64 start, uint64 len)
{
    if (start % PGSIZE != 0) return -1;
    struct proc *p = curr_proc();
    uint64 pages = (len + PGSIZE - 1) / PGSIZE;
    for (uint64 i = 0; i < pages; i++) {
        uint64 va = start + i * PGSIZE;
        if (walkaddr(p->pagetable, va) == 0) return -1;
    }
    uvmunmap(p->pagetable, start, pages, 1);
    return 0;
}