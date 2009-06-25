/*
 * fcp - A fast copy mechanism for ext2/3
 *
 * Copyright (C) 2009 Joel Fernandes
 *
 * This file may be redistributed under the terms of the GNU General Public
 * License.
 */

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <stdlib.h>

#define USAGE "usage: %s [-n] [-v] filesystem source destination ...\n"

int set_inode_parent(fd, i_no)
{
  return ioctl(fd, 100, i_no);
}

int get_inode_parent(fd)
{
  int ret = 0;
  ioctl(fd, 101, &ret);
  return ret;
}

int main(int argc, char **argv)
{
  int fd, ret, value;

  if(argc < 3) {
    perror("Usage: fcp g|c filename [value]\n");
    return 1;
  }
  fd = open(argv[2], O_RDWR);
  if(fd < 0)
    {
      perror("couldn't open file");
      return 1;
    }

  if (argv[1][0] == 's') {
    if(argc < 4) {
      perror("Usage: fcp g|c filename [value]\n");
      return 1;
    }
    value = atoi(argv[3]);
    return set_inode_parent(fd, value);
  } else {
    value = get_inode_parent(fd);
    printf("inode parent %d\n", value);    
    return 0;
  }
  return 0;
}
