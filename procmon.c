#include <unistd.h>
#include <assert.h>
#include <stddef.h>
#include <stdlib.h>
#include <sched.h>
#include <linux/futex.h>
#include <sys/types.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <signal.h>
#include <errno.h>

#include "atomics.h"
#include "procmon.h"

#define STACK_SIZE 4096

struct procmon_head {
	struct procmon_entry* next;
	long offset;
	struct procmon_entry* pending;
};

static int procmon_ref = 0;

static int procmon_tls = 0;

static char procmon_thread_stack[STACK_SIZE];

static int procmon_futex = 0;
static int procmon_tid = -1;
static struct procmon_head robust_list_head;

static int futex(int *uaddr, int op, int val, int val2, int uaddr2, int val3)
{
	return syscall(SYS_futex, uaddr, op, val, val2, uaddr2, val3);
}

static int futex_wait(int* addr, int expected)
{
	return futex(addr, FUTEX_WAIT, expected, 0, 0, 0);
}

static int futex_wake(int* addr, int nthreads)
{
	return futex(addr, FUTEX_WAKE, nthreads, 0, 0, 0);
}

static long set_robust_list(struct procmon_head *head, size_t len)
{
	return syscall(SYS_set_robust_list, head, len);
}

int procmon__thread_start(void* arg)
{
	(void)arg;

	long rc = set_robust_list(&robust_list_head, sizeof(robust_list_head));
	assert(rc == 0);

	sigset_t sigset;
	sigfillset(&sigset);
	sigprocmask(SIG_BLOCK, &sigset, NULL);

	// Wake up the parent in case it's waiting
	int expected = 1;
	if (atomic_cas(&procmon_futex, &expected, 0))
		futex_wake(&procmon_futex, 1);

	// Hang around until the parent exits
	atomic_store(&procmon_futex, 1);
	futex_wait(&procmon_futex, 1);

	return 0;
}

int procmon_init(void)
{
	if (atomic_fetch_add(&procmon_ref, 1) > 0)
		return 0;

	int ptid = 0;

	robust_list_head.offset = offsetof(struct procmon_entry, lock_word);
	robust_list_head.pending = NULL;
	robust_list_head.next = (struct procmon_entry*)&robust_list_head;

	char* stack_top = (char*)&procmon_thread_stack + STACK_SIZE;

	/* Not all those flags are needed, but valgrind won't run with any other
	 * combination
	 */
	unsigned flags = CLONE_VM | CLONE_FS | CLONE_FILES | CLONE_SIGHAND
		| CLONE_THREAD | CLONE_SYSVSEM | CLONE_SETTLS
		| CLONE_PARENT_SETTID | CLONE_CHILD_SETTID | CLONE_DETACHED;

	int rc = clone(procmon__thread_start, stack_top, flags,
		       NULL, &ptid, &procmon_tls, &procmon_tid);

	assert(rc != EINVAL);

	// Wait for the thread to start
	int expected = 0;
	if (atomic_cas(&procmon_futex, &expected, 1))
		futex_wait(&procmon_futex, 1);

	return rc;
}

void procmon_destroy(void)
{
	if (atomic_sub_fetch(&procmon_ref, 1) > 0)
		return;

	futex_wake(&procmon_futex, 1);
}

void procmon_add(struct procmon_entry* entry)
{
	struct procmon_entry* head = atomic_load(&robust_list_head.next);

	entry->lock_word = procmon_tid;

	do entry->next = head;
	while (!atomic_cas_weak(&robust_list_head.next, &head, entry));
}

void procmon_remove(struct procmon_entry* entry)
{
	struct procmon_entry* node = atomic_load(&robust_list_head.next);

	while (node->next != node && node->next != entry)
		node = atomic_load(&node->next);

	// TODO: Figure out how to make this thread-safe
	if (node->next == entry)
		atomic_store(&node->next, entry->next);
}

int procmon_is_alive(struct procmon_entry* entry)
{
	return !(atomic_load(&entry->lock_word) & (1 << 30));
}
