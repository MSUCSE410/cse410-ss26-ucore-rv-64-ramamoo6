#include "console.h"
#include "defs.h"
#include "loader.h"
#include "timer.h"
#include "trap.h"

void clean_bss()
{
	extern char s_bss[];
	extern char e_bss[];
	memset(s_bss, 0, e_bss - s_bss);
}

void main()
{
    clean_bss();
    printf("hello world!\n");
    kinit();        // ← must be first, sets up kalloc
    proc_init();
    kvm_init();
    trap_init();    // ← set up trap handler before timer
    loader_init();
    timer_init();
    run_all_app();
    infof("start scheduler!");
    scheduler();
}

