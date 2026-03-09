#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

#define DEVICE_NAME "/dev/my_misc_device"

int main(void)
{
    const char *dev_name = DEVICE_NAME;
    int fd;
    int value = 42;
    char write_buf[32];
    char read_buf[32];
    ssize_t n;

    fd = open(dev_name, O_RDWR);
    if (fd < 0) {
        perror("open");
        return EXIT_FAILURE;
    }

    snprintf(write_buf, sizeof(write_buf), "%d\n", value);
    n = write(fd, write_buf, strlen(write_buf));
    if (n < 0) {
        perror("write");
        close(fd);
        return EXIT_FAILURE;
    }
    printf("Written: %s", write_buf);

    /* Seek back to the beginning before reading */
    if (lseek(fd, 0, SEEK_SET) < 0) {
        perror("lseek");
        close(fd);
        return EXIT_FAILURE;
    }

    n = read(fd, read_buf, sizeof(read_buf) - 1);
    if (n < 0) {
        perror("read");
        close(fd);
        return EXIT_FAILURE;
    }
    read_buf[n] = '\0';
    printf("Read back: %s", read_buf);

    close(fd);
    return EXIT_SUCCESS;
}
