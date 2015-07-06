#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <search.h>
#include "libapkg.h"

#define TMPDIR  	"/tmp"
#define TMPAPKGDIR  TMPDIR   "/apkg"
#define APKGCIDIR	TMPAPKGDIR   "/tmp.ci/"

#define RED(fmt,args...)  fprintf(stderr,"\033[1;31m" fmt "\033[0m\n", ##args)
#define GREEN(fmt,args...)  printf("\033[1;32m" fmt "\033[0m\n", ##args)
#define YELLOW(fmt,args...)  printf("\033[1;33m" fmt "\033[0m\n", ##args)
#define BLUE(fmt,args...)  printf("\033[1;34m" fmt "\033[0m\n", ##args)
#define PINK(fmt,args...)  printf("\033[1;35m" fmt "\033[0m\n", ##args)
#define OBLUE(fmt,args...)  printf("\033[1;36m" fmt "\033[0m\n", ##args)
#define WHITE(fmt,args...)  printf("\033[1;37m" fmt "\033[0m\n", ##args)

#define OPERATION_INSTALL 			(1 << 0)
#define OPERATION_REMOVE 			(1 << 1)
#define OPERATION_LIST 				(1 << 2)
#define OPERATION_UNPACK 			(1 << 3)
#define OPERATION_CONFIGURE			(1 << 4)
#define OPERATION_STATUS 			(1 << 5)
#define OPERATION_CONFALL 			(1 << 6)
#define OPERATION_DEPENDS 			(1 << 7)
#define OPERATION_ALLDEPENDS 		(1 << 8)
#define OPERATION_FILES	 			(1 << 9)
#define OPERATION_INIT	 			(1 << 10)
#define OPERATION_INFO	 			(1 << 11)
