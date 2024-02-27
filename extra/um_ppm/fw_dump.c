#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>

#define DEVICE_FILE "/dev/ucsi_um_test-0"
#define IOCTL_TRANSFER _IOW('U', 1, struct transfer_data)

struct transfer_data {
    int num_bytes;
    int done;
    int data_index;
    char data[255];
};

int main() {
    int device_fd;
    struct transfer_data data;
    data.done = 0;
    data.data_index = 0;
    // Open the device file
    device_fd = open(DEVICE_FILE, O_RDWR);
    if (device_fd < 0) {
        perror("Failed to open the device file");
        return 1;
    }

    // Read a file
    FILE *file = fopen("/usr/local/input.txt", "r");
    if (file == NULL) {
        perror("Failed to open input file");
        close(device_fd);
        return 1;
    }

    // Read data from the file and transfer it via ioctl
    while ((data.num_bytes = fread(data.data, sizeof(char), sizeof(data.data), file)) > 0) {
        data.done = (feof(file) != 0);
        printf("done:%d index: %d %.*s\n",data.done,data.data_index, data.num_bytes,data.data);
        if (ioctl(device_fd, IOCTL_TRANSFER, &data) < 0) {
            perror("ioctl failed");
            fclose(file);
            close(device_fd);
            return 1;
        }
        data.data_index += 1;
    }

    fclose(file);
    close(device_fd);
    return 0;
}