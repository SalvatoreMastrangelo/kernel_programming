#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <sys/time.h>

#define DEVICE_NAME "/dev/periodic_device"

int main(void)
{
    const char *dev_name = DEVICE_NAME;
    int fd;
    __uint64_t written_value;
    __uint64_t read_value;  
    ssize_t n;

    printf("Enter a period in milliseconds: ");
    scanf("%llu", (unsigned long long *)&written_value);

    fd = open(dev_name, O_RDWR);
    if (fd < 0) {
        perror("open");
        return EXIT_FAILURE;
    }

    n = write(fd, &written_value, sizeof(written_value));
    if (n < 0) {
        perror("write");
        close(fd);
        return EXIT_FAILURE;
    }
    printf("Written: %llu\n", (unsigned long long)written_value);

    struct timeval start, end;
    gettimeofday(&start, NULL);

    n = read(fd, &read_value, sizeof(read_value));
    if (n < 0) {
        perror("read");
        close(fd);
        return EXIT_FAILURE;
    }

    gettimeofday(&end, NULL);
    long elapsed_us = (end.tv_sec - start.tv_sec) * 1000000 +
                      (end.tv_usec - start.tv_usec);
    printf("Elapsed time: %ld us (%.3f ms)\n", elapsed_us, elapsed_us / 1000.0);

    close(fd);
    return EXIT_SUCCESS;
}
