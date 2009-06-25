/*
 * fcp - A fast file copy utility for ext2/3
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
#define EXT2_GET_PARENT 100
#define EXT2_SET_PARENT 101
#define EXT2_GET_CHILDR 102
#define END_CHILD_LIST 000
#define USAGE "usage: %s [-n] [-v] filesystem source destination ...\n"

struct child_inodes {
  int *start;
  int length;
};

int set_inode_parent(fd, i_no)
{
  return ioctl(fd, EXT2_SET_PARENT, i_no);
}

int get_inode_parent(fd)
{
  int ret = 0;
  ioctl(fd, EXT2_GET_PARENT, &ret);
  return ret;
}

struct child_inodes get_inode_children(fd)
{
  struct child_inodes ci;
  int * list, count = 0;
  ioctl(fd, EXT2_GET_CHILDREN, list);
  if(!list)
    return NULL;
  while(*list)
    {
      count++;
      list++;
    }
  ci.start = list;
  ci.length = count;
  return ci;
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

  switch(argv[1][0])
    {
    case 's':
      if(argc < 4) {
	perror("Usage: fcp g|c filename [value]\n");
	return 1;
      }
      value = atoi(argv[3]);
      return set_inode_parent(fd, value);
    case 'g':
      value = get_inode_parent(fd);
      if(!value)
	printf("no parent found\n");
      else
	printf("inode parent %d\n", value);    
      break;
    case 'l':
      for(i=0; i<ci.length; i++)
	{
	  printf("child: %d\n", ci.list[i])
	}      
      break;
    default:
      perror("Usage: fcp g|c filename [value]\n");
      return 1;
    }

  return 0;
}
