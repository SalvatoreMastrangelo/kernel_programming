#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/time.h>

#define DEVICE_NAME "/dev/periodic_device"

static int n_threads;

struct thread_arg {
    int tid;
    int fd;
};

static void *thread_fn(void *arg)
{
    struct thread_arg *targ = (struct thread_arg *)arg;
    int tid = targ->tid;
    int fd = targ->fd;

    __uint64_t written_value = 50 * (tid + 1);
    __uint64_t read_value;
    ssize_t n;

    n = write(fd, &written_value, sizeof(written_value));
    if (n < 0) {
        perror("write");
        return NULL;
    }
    printf("[tid %d] Written: %llu\n", tid, (unsigned long long)written_value);

    struct timeval start, end;
    gettimeofday(&start, NULL);

    n = read(fd, &read_value, sizeof(read_value));
    if (n < 0) {
        perror("read");
        return NULL;
    }

    gettimeofday(&end, NULL);
    long elapsed_us = (end.tv_sec - start.tv_sec) * 1000000 +
                      (end.tv_usec - start.tv_usec);
    printf("[tid %d] Read: %llu | Elapsed: %ld us (%.3f ms)\n",
           tid, (unsigned long long)read_value, elapsed_us, elapsed_us / 1000.0);

    return NULL;
}

int main(int argc, char *argv[])
{
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <n_threads>\n", argv[0]);
        return EXIT_FAILURE;
    }

    n_threads = atoi(argv[1]);
    if (n_threads <= 0) {
        fprintf(stderr, "n_threads must be > 0\n");
        return EXIT_FAILURE;
    }

    int fd = open(DEVICE_NAME, O_RDWR);
    if (fd < 0) {
        perror("open");
        return EXIT_FAILURE;
    }

    pthread_t *threads = malloc(n_threads * sizeof(pthread_t));
    struct thread_arg *args = malloc(n_threads * sizeof(struct thread_arg));
    if (!threads || !args) {
        perror("malloc");
        close(fd);
        return EXIT_FAILURE;
    }

    for (int i = 0; i < n_threads; i++) {
        args[i].tid = i;
        args[i].fd = fd;
        if (pthread_create(&threads[i], NULL, thread_fn, &args[i]) != 0) {
            perror("pthread_create");
            close(fd);
            return EXIT_FAILURE;
        }
    }

    for (int i = 0; i < n_threads; i++)
        pthread_join(threads[i], NULL);

    free(threads);
    free(args);
    close(fd);
    return EXIT_SUCCESS;
}
