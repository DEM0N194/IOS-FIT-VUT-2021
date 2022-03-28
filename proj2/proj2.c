/**
 * @file	proj2.c
 * @brief	IOS - project 2 (synchronization)
 * @atuhor	xlacko08
 * @date	01/05/2021
 */

#include <errno.h>
#include <fcntl.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/prctl.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

// helper macros
#define DEBUG 0
#if DEBUG == 1
#define DPRINT(msg) fprintf(stderr, "DEBUG: %s\n", msg)
#define OUTPUT stdout
#else
#define DPRINT(msg)
#define OUTPUT output
#endif

// sem_init + error checking
#define SEM_INIT(name, value)                       \
	do {                                            \
		if (sem_init(&shm->sem_##name, 1, value)) { \
			perror("sem_init(" #name ") failed");   \
			exit(EXIT_FAILURE);                     \
		}                                           \
	} while (0)

// sem_wait + error checking
#define SEM_WAIT(name)                            \
	do {                                          \
		if (sem_wait(&shm->sem_##name)) {         \
			perror("sem_wait(" #name ") failed"); \
			_exit(EXIT_FAILURE);                  \
		}                                         \
	} while (0)

// sem_post + error checking
#define SEM_POST(name)                            \
	do {                                          \
		if (sem_post(&shm->sem_##name)) {         \
			perror("sem_post(" #name ") failed"); \
			_exit(EXIT_FAILURE);                  \
		}                                         \
	} while (0)

// named constants
#define REQ_ARGC 5
#define BASE_TEN 10
#define REQ_ELVES 3
#define MIN_NE 1
#define MAX_NE 999
#define MIN_NR 1
#define MAX_NR 19
#define MIN_TE 0
#define MAX_TE 1000
#define MIN_TR 0
#define MAX_TR 1000

// global variables
int NE, NR, TE, TR;
FILE *output;
struct shm_t
{
	sem_t sem_wake_santa;
	sem_t sem_thanks_santa;
	sem_t sem_elf_queue;
	sem_t sem_help;
	sem_t sem_hitch_reindeer;
	sem_t sem_sleigh_ready;
	sem_t sem_mutex;
	int A;
	int elves;
	int reindeer;
	int workshop_closed;
} * shm;

// forward declarations
int str2i(char *str, int *out);
void argparse(int argc, char *argv[]);
void initialize();
void create_process(void (*proc)(int), int arg);
void proc_santa(int arg);
void proc_elf(int elfID);
void proc_reindeer(int rdID);

// Clean up functions to be registered at exit
void close_output_file();
void delete_shm();

int main(int argc, char *argv[]) {
	argparse(argc, argv);
	initialize();

	// create processes
	create_process(proc_santa, 0);
	for (int id = 1; id <= NE; id++) {
		create_process(proc_elf, id);
	}
	for (int id = 1; id <= NR; id++) {
		create_process(proc_reindeer, id);
	}

	// wait for all proccesses to exit
	for (int status, i = 0, N = NR + NE + 1; i < N; i++) {
		if (waitpid(0, &status, 0) < 0) {
			perror("waitpid failed");
			exit(EXIT_FAILURE);
		}
		if (WIFEXITED(status) && WEXITSTATUS(status)) {
			exit(EXIT_FAILURE);
		} else if (!WIFEXITED(status)) {
			exit(EXIT_FAILURE);
		}
	}
	exit(EXIT_SUCCESS);
}

/**
 * @brief	actions taken by santa
*/
void proc_santa(int arg) {
	(void)arg;

	while (1) {
		SEM_WAIT(mutex);
		fprintf(OUTPUT, "%d: Santa: going to sleep\n", shm->A++);
		SEM_POST(mutex);

		SEM_WAIT(wake_santa);
		SEM_WAIT(mutex);
		if (shm->reindeer == NR) {
			shm->reindeer = 0;
			fprintf(OUTPUT, "%d: Santa: closing workshop\n", shm->A++);
			shm->workshop_closed = 1;
			SEM_POST(mutex);
			SEM_POST(help);
			SEM_POST(elf_queue);
			for (int i = 0; i < NR; i++) {
				SEM_POST(hitch_reindeer);
			}

			SEM_WAIT(sleigh_ready);
			SEM_WAIT(mutex);
			fprintf(OUTPUT, "%d: Santa: Christmas started\n", shm->A++);
			SEM_POST(mutex);
			return;
		} else if (shm->elves == REQ_ELVES) {
			fprintf(OUTPUT, "%d: Santa: helping elves\n", shm->A++);
			SEM_POST(mutex);
			for (int i = 0; i < REQ_ELVES; i++) {
				SEM_POST(help);
			}

			SEM_WAIT(thanks_santa);
		}
	}
}

/**
 * @brief	actions taken by an elf
*/
void proc_elf(int elfID) {
	SEM_WAIT(mutex);
	fprintf(OUTPUT, "%d: Elf %d: started\n", shm->A++, elfID);
	SEM_POST(mutex);

	while (1) {
		useconds_t work_time = (rand() % (TE + 1)) * 1000;
		usleep(work_time);

		SEM_WAIT(elf_queue);
		SEM_WAIT(mutex);
		fprintf(OUTPUT, "%d: Elf %d: need help\n", shm->A++, elfID);
		if (shm->workshop_closed) {
			SEM_POST(mutex);
			break;
		}
		shm->elves++;
		if (shm->elves == REQ_ELVES) {
			SEM_POST(wake_santa);
		} else {
			SEM_POST(elf_queue);
		}
		SEM_POST(mutex);

		SEM_WAIT(help);
		if (shm->workshop_closed) {
			break;
		}
		SEM_WAIT(mutex);
		fprintf(OUTPUT, "%d: Elf %d: get help\n", shm->A++, elfID);
		shm->elves--;
		if (shm->elves == 0) {
			SEM_POST(thanks_santa);
			SEM_POST(elf_queue);
		}
		SEM_POST(mutex);
	}

	SEM_WAIT(mutex);
	fprintf(OUTPUT, "%d: Elf %d: taking holidays\n", shm->A++, elfID);
	SEM_POST(mutex);
	SEM_POST(elf_queue);
	SEM_POST(help);
}

/**
 * @brief	actions taken by a reindeer
*/
void proc_reindeer(int rdID) {
	SEM_WAIT(mutex);
	fprintf(OUTPUT, "%d: RD %d: rstarted\n", shm->A++, rdID);
	SEM_POST(mutex);

	const int HTR = TR / 2;
	useconds_t vacation_time = (rand() % (TR - HTR + 1) + HTR) * 1000;
	usleep(vacation_time);

	SEM_WAIT(mutex);
	fprintf(OUTPUT, "%d: RD %d: return home\n", shm->A++, rdID);
	shm->reindeer++;
	if (shm->reindeer == NR) {
		SEM_POST(wake_santa);
	}
	SEM_POST(mutex);

	SEM_WAIT(hitch_reindeer);
	SEM_WAIT(mutex);
	fprintf(OUTPUT, "%d: RD %d: get hitched\n", shm->A++, rdID);
	shm->reindeer++;
	if (shm->reindeer == NR) {
		SEM_POST(sleigh_ready);
	}
	SEM_POST(mutex);
}

/**
 * @brief	starts up a new process
*/
void create_process(void (*proc)(int), int arg) {
	pid_t original_ppid = getpid();
	pid_t pid = fork();
	if (pid < 0) {
		perror("fork failed");
		exit(EXIT_FAILURE);
	}
	if (pid == 0) {
		// Set the parent-death signal for the child
		int r = prctl(PR_SET_PDEATHSIG, SIGTERM);
		if (r == -1) {
			perror(0);
			_exit(EXIT_FAILURE);
		}
		// test in case the original parent exited before prctl call
		if (original_ppid != getppid()) {
			_exit(EXIT_FAILURE);
		}
		// execute child process function
		proc(arg);
		_exit(EXIT_SUCCESS);
	}
}

/**
 * @brief	initialize program resoureces and register cleanup functions at exit
*/
void initialize() {
	srand(time(NULL));
	// open file for output
	output = fopen("proj2.out", "w");
	if (output == NULL) {
		perror("Failed to open proj2.out");
		exit(EXIT_FAILURE);
	}
	atexit(close_output_file);
	setlinebuf(output);
	DPRINT("output file opened");

	// create shared memory between processes
	int shm_fd = shm_open("/xlacko08_proj2", O_CREAT | O_RDWR | O_TRUNC, S_IRUSR | S_IWUSR);
	if (shm_fd < 0) {
		perror("shm_open() failed");
		exit(EXIT_FAILURE);
	}
	atexit(delete_shm);
	if (ftruncate(shm_fd, sizeof(struct shm_t))) {
		perror("ftruncate() failed");
		exit(EXIT_FAILURE);
	}
	shm = (struct shm_t *)mmap(NULL, sizeof(struct shm_t), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
	if (shm == MAP_FAILED) {
		perror("mmap() failed");
		exit(EXIT_FAILURE);
	}
	if (close(shm_fd)) {
		perror("close(shm_fd) failed");
		exit(EXIT_FAILURE);
	}
	DPRINT("shm created");

	// initialize semaphores
	SEM_INIT(wake_santa, 0);
	SEM_INIT(thanks_santa, 0);
	SEM_INIT(elf_queue, 1);
	SEM_INIT(help, 0);
	SEM_INIT(hitch_reindeer, 0);
	SEM_INIT(sleigh_ready, 0);
	SEM_INIT(mutex, 1);
	DPRINT("sem init");

	// initialize shared variables
	shm->A = 1;
	shm->elves = 0;
	shm->reindeer = 0;
	shm->workshop_closed = 0;
}

/**
* @brief	closes file when main process terminates
*/
void close_output_file() {
	if (fclose(output) == EOF) {
		perror("fclose(output) failed");
	}
	DPRINT("Output file closed");
}

/**
 * @brief	deletes shared memory when main process terminates
*/
void delete_shm() {
	if (munmap(shm, sizeof(struct shm_t))) {
		perror("munmap failed");
	}
	if (shm_unlink("/xlacko08_proj2")) {
		perror("shm_unlink() failed");
	}
	DPRINT("shm deleted");
}

/**
 * @brief	convert string to integer
 * @return	0 on success, 1 on failure
*/
int str2i(char *str, int *out) {
	char *pEnd = NULL;
	*out = (int)strtol(str, &pEnd, BASE_TEN);
	return (*pEnd != '\0' || *str == '\0');
}

/**
 * @brief	parse arguments and exit on errors
*/
void argparse(int argc, char *argv[]) {
	if (argc != REQ_ARGC) {
		fprintf(stderr, "Invalid argument count.\n");
		exit(EXIT_FAILURE);
	}
	int err = 0;
	err += str2i(argv[1], &NE);
	err += str2i(argv[2], &NR);
	err += str2i(argv[3], &TE);
	err += str2i(argv[4], &TR);
	if (err) {
		perror("argument conversion to integer failed");
		exit(EXIT_FAILURE);
	}
	if (NE < MIN_NE|| NE > MAX_NE) {
		fprintf(stderr, "Number of elves has to be from the interval <1,999>.\n");
		exit(EXIT_FAILURE);
	}
	if (NR < MIN_NR || NR > MAX_NR) {
		fprintf(stderr, "Number of reindeer has to be from the interval <1,19>.\n");
		exit(EXIT_FAILURE);
	}
	if (TE < MIN_TE || TE > MAX_TE) {
		fprintf(stderr, "Max work time for elves has to be from the interval <0,1000>.\n");
		exit(EXIT_FAILURE);
	}
	if (TR < MIN_TR || TR > MAX_TR) {
		fprintf(stderr, "Max work time for elves has to be from the interval <0,1000>.\n");
		exit(EXIT_FAILURE);
	}
}
