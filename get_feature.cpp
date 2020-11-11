#include <linux/types.h>
#include <linux/input.h>
#include <linux/hidraw.h>

#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include <cstdio>
#include <iostream>

using namespace std;

void get_feature(int fd) {
  char buf[256] = {};
  if (int res = ioctl(fd, HIDIOCGFEATURE(256), buf); res < 0) {
    perror("HIDIOCGFEATURE");
    return;
  } else {
    printf("feature\n");
    for (int i = 0; i < res; i++)
      printf("%02x ", buf[i]);
    printf("\n");
  }
}

void get_desc(int fd) {
  int desc_size;
  if (int res = ioctl(fd, HIDIOCGRDESCSIZE, &desc_size); res < 0) {
    perror("HIDIOCGRDESCSIZE");
    return;
  }

  struct hidraw_report_descriptor rpt_desc = {.size = (uint32_t)desc_size};
  if (int res = ioctl(fd, HIDIOCGRDESC, &rpt_desc); res < 0) {
    perror("HIDIOCGRDESC");
    return;
  }

  printf("desc\n");
  for (int i = 0; i < desc_size; i++)
    printf("%02x ", rpt_desc.value[i]);
  printf("\n");
}

int main() {
  int fd = open("/dev/hidraw1", O_RDWR | O_NONBLOCK);
  if (fd < 0) {
    perror("open");
    return 1;
  }

  get_desc(fd);
  get_feature(fd);

  close(fd);
}
