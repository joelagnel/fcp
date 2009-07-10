/*
 * fcp - A fast file copy utility for ext2/3
 *
 * Copyright (C) 2009 Joel Fernandes.
 *
 * Description: userpace utility to talk to the ext2 cow layer.
 *
 * For a long time I wished to have copy-on-write in ext2. Here's an implementation of
 * copy-on-write in ext2, in < 500 lines of code (kernel patch + fcp).
 * fcp heavily depends on the kernel patch which should compile on all new kernels.
 *
 * This is original work, modifying and redestribution is allowed provided this header
 * is kept intact. This file may be redistributed under the terms of the GPL.
 */

#define _GNU_SOURCE 1
 
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>

#define DEBUG_FCP 1
#define EXT2_SET_PARENT 100
#define EXT2_GET_PARENT 101
#define EXT2_GET_CHILDR 102
#define END_CHILD_LIST 000

int set_inode_parent(int fd, int i_no)
{
  return ioctl(fd, EXT2_SET_PARENT, i_no);
}

int get_inode_parent(int fd)
{
  int ret = 0;
  ioctl(fd, EXT2_GET_PARENT, &ret);
  return ret;
}

int get_inode_children(int fd, int * list)
{
  int count = 0;
  ioctl(fd, EXT2_GET_CHILDR, list);
  while(*list)
    {
      count++;
      list++;
    }
  return count;
}

int create_child(int source_fd, char *dest)
{
  int fd, ret, source_inode;
  long source_size = 0;
  struct stat buf;
  fd  = open(dest, O_WRONLY|O_CREAT|O_EXCL|O_LARGEFILE);
  if(fd < 0)
    {
      printf("couldn't open destination file %s ", dest);
      return 1;
    }
  ret = fstat(source_fd, &buf);
  if(ret < 0)
    {
      printf("Stat failed on source fd\n");
      return ret;
    }
  source_size = (long)buf.st_size;
  source_inode = (int)buf.st_ino;
  // lseek(fd, source_size + 9000, SEEK_SET); /* not required, we're doing this in the kernel */
  // write(fd, "\0", 1);
  set_inode_parent(fd, source_inode);
  close(fd);
  return 0;
}

int main(int argc, char **argv)
{
  int fd, source_fd, ret, value;
  int *list, count, i, mv = 1;
  char *source, *dest, *name, *suffix = ".root", *buff;

  if(argc < 3) {
    perror("Usage: fcp g|c filename [value]\n");
    return 1;
  }

  fd = open(argv[2], O_RDONLY);
  if(fd < 0)
    {
      perror("couldn't open file");
      return 1;
    }

  switch(argv[1][0])
    {
    case 'c':			// remove this option, moves happen ALWAYS.
      mv = 0;
    case 'r': /* fast copy */
      if(argc < 4) {
	perror("Usage: fcp g|c|r|l|n source-filename [destination-filename|value]\n");
      }
      else {
	if(mv)
	  {
	    source = argv[2];
	    close(fd);
	    name = (char *)malloc(strlen(source) + strlen(suffix) + 1);
	    strcpy(name, source);
	    strcat(name, suffix);
	    rename(source, name); /* the old inode is supposed to disappear, rename for now */
	    fd = open(name, O_RDONLY);
	    ret = create_child(fd, source);
	    if(ret)
	      {
		printf("Couldn't create child for source %s\n", source);
		return 1;
	      }
	    else
	      printf("source child created\n");
	  }
        for(i = 3; i < argc; i++)
	  {
	    dest = argv[i];
	    ret = create_child(fd, dest);
	    printf("ret = %d\n", ret);
	    if(ret)
	      {
		printf("ret=%d, Couldn't create child for destination %s.\n", ret, dest);
		return 1;
	      }
	    else
	      printf("destination child created: %s\n", dest);
	  }
        unlink(name);
	printf("All done.\n");
	return 0;
      }

    case 't':
      source = argv[2];
      fd = open(source, O_WRONLY);
      if(fd > 0)
	{
	  lseek(fd, 4096, SEEK_SET);
	  buff = (char *)malloc(10000);
	  ret = write(fd, buff, 10000);
	  printf("%d bytes written\n", ret);
	}
      else
	{
	  printf("couldn't open file\n");
	  close(fd);
	}
      return 0;

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
      list = (int *)malloc(sizeof(int) * 200);
      count = get_inode_children(fd, list);
      if(!count)
	{
	  printf("No children found.\n");
	}
      else
	{
	  for(i=0; i<count; i++)
	    {
	      printf("child: %d\n", list[i]);
	    }      
	}
      break;
    default:
      perror("Usage: fcp g|c filename [value]\n");
      return 1;
    }

  return 0;
}
