#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <time.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>
#include <pthread.h>
#include <sys/mman.h>
#include <errno.h>
#include <sys/epoll.h>

#define LOOPS 10000000
#define BENCH_SIGNAL (SIGUSR1)

int pollfd = -1;

struct {
	uint32_t is_alive;
	pthread_mutex_t mutex;
} *shared_memory;

static int (*is_process_alive)(int pid);

uint64_t gettime_us(void)
{
	struct timespec ts = { 0 };
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return ts.tv_sec * 1000000ULL + ts.tv_nsec / 1000ULL;
}

void wait_for_parent(void)
{
	sigset_t s;
	sigemptyset(&s);
	sigaddset(&s, BENCH_SIGNAL);
	sigprocmask(SIG_BLOCK, &s, NULL);
	int sig;
	sigwait(&s, &sig);
}

void signal_children(void)
{
	sigset_t s;
	sigemptyset(&s);
	sigaddset(&s, BENCH_SIGNAL);
	sigprocmask(SIG_BLOCK, &s, NULL);

	usleep(100000);
	kill(0, BENCH_SIGNAL);
}

int is_process_alive_dummy(int pid)
{
	return 1;
}

int is_process_alive_kill(int pid)
{
	return kill(pid, 0) == 0;
}

int is_process_alive_procfs(int fd)
{
	return faccessat(fd, "stat", R_OK, 0) == 0;
}

int is_process_alive_mutex(int fd)
{
	(void)fd;

	int rc = pthread_mutex_trylock(&shared_memory->mutex);

	switch (rc) {
	case EOWNERDEAD:
		pthread_mutex_consistent(&shared_memory->mutex);
		rc = 0;
	case 0:
		pthread_mutex_unlock(&shared_memory->mutex);
	}

	return rc != 0;
}

int is_process_alive_load(int fd)
{
	return __atomic_load_n(&shared_memory->is_alive, __ATOMIC_RELAXED);
}

int is_process_alive_epoll(int pid)
{
	struct epoll_event events;
	epoll_wait(pollfd, &events, 1, 0);
	return 1;
}

int spawn_process(void)
{
	pid_t pid;

	pid = fork();
	if (pid < 0) {
		perror("fork");
		return -1;
	}

	if (pid > 0)
		return pid;

	pthread_mutex_lock(&shared_memory->mutex);
	shared_memory->is_alive = 1;

	wait_for_parent();

	shared_memory->is_alive = 0;
	pthread_mutex_unlock(&shared_memory->mutex);

	exit(0);
	return 0;
}

void init_mutex(void)
{
	shared_memory = mmap(NULL, sizeof(*shared_memory),
			     PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS,
			     -1, 0);
	assert(shared_memory);

	memset(shared_memory, 0, sizeof(*shared_memory));

	pthread_mutexattr_t attr;
	pthread_mutexattr_init(&attr);
	pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);
	pthread_mutexattr_setrobust(&attr, PTHREAD_MUTEX_ROBUST);

	int rc = pthread_mutex_init(&shared_memory->mutex, &attr);
	assert(rc == 0);
}

int main(int argc, char* argv[])
{
	const char* method = argv[1];
	int fd;

	pollfd = epoll_create1(0);
	assert(pollfd >= 0);

	init_mutex();

	int pid = spawn_process();

	if (!method || strcmp(method, "kill") == 0)
		is_process_alive = is_process_alive_kill;
	else if (strcmp(method, "dummy") == 0)
		is_process_alive = is_process_alive_dummy;
	else if (strcmp(method, "mutex") == 0)
		is_process_alive = is_process_alive_mutex;
	else if (strcmp(method, "load") == 0)
		is_process_alive = is_process_alive_load;
	else if (strcmp(method, "epoll") == 0)
		is_process_alive = is_process_alive_epoll;
	else if (strcmp(method, "procfs") == 0) {
		is_process_alive = is_process_alive_procfs;
		char path[256];
		sprintf(path, "/proc/%d", pid);
		fd = open(path, O_RDONLY);
		assert(fd >= 0);
		pid = fd;
	}

	uint32_t count = 0;

	uint64_t t, t0 = gettime_us();

	usleep(10000);

	while (1) {
		int alive = is_process_alive(pid);
		assert(alive);

		count++;
		if (count >= LOOPS)
			break;
	}

	t = gettime_us();

	signal_children();
	waitpid(-1, NULL, 0);

	int alive = is_process_alive(pid);

	int is_dummy = is_process_alive == is_process_alive_dummy
		    || is_process_alive == is_process_alive_epoll;

	assert(is_dummy || !alive);

	double dt = (double)(t - t0) * 1e-6;

	printf("%.2f s\n", dt);

	return 0;
}
