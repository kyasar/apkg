#include <stdio.h>

#define ASSERT(x) /* nothing */
#define DPRINTF(fmt,args...) /* nothing */

#define PRINTF(fmt,args...)  printf(fmt, ##args)
#define FPRINTF(str,fmt,args...)  fprintf(str, fmt, ##args)

#define BUFSIZE		4096
#define ETCDIR  	"/etc"
#define ADMINDIR  	ETCDIR   "/apkg"
#define STATUSFILE	ADMINDIR "/status"
#define INFODIR		ETCDIR   "/apkg/info/"

#define TMPDIR  	"/tmp"
#define TMPAPKGDIR  TMPDIR   "/apkg"
#define APKGCIDIR	TMPAPKGDIR   "/tmp.ci/"

#define STATUS_WANTSTART		(0)
#define STATUS_WANTUNKNOWN		(1 << 0)
#define STATUS_WANTINSTALL		(1 << 1)
#define STATUS_WANTHOLD			(1 << 2)
#define STATUS_WANTDEINSTALL		(1 << 3)
#define STATUS_WANTPURGE		(1 << 4)
#define STATUS_WANTMASK			~(STATUS_WANTUNKNOWN | STATUS_WANTINSTALL | STATUS_WANTHOLD | STATUS_WANTDEINSTALL | STATUS_WANTPURGE)

#define STATUS_FLAGSTART		(5)
#define STATUS_FLAGOK			(1 << 5)
#define STATUS_FLAGREINSTREQ	(1 << 6)
#define STATUS_FLAGHOLD			(1 << 7)
#define STATUS_FLAGHOLDREINSTREQ	(1 << 8)
#define STATUS_FLAGMASK			~(STATUS_FLAGOK | STATUS_FLAGREINSTREQ | STATUS_FLAGHOLD | STATUS_FLAGHOLDREINSTREQ)

#define STATUS_STATUSSTART		(9)
#define STATUS_STATUSNOTINSTALLED	(1 << 9)
#define STATUS_STATUSUNPACKED		(1 << 10)
#define STATUS_STATUSHALFCONFIGURED	(1 << 11)
#define STATUS_STATUSINSTALLED		(1 << 12)
#define STATUS_STATUSHALFINSTALLED	(1 << 13)
#define STATUS_STATUSCONFIGFILES	(1 << 14)
#define STATUS_STATUSPOSTINSTFAILED	(1 << 15)
#define STATUS_STATUSREMOVALFAILED	(1 << 16)
#define STATUS_STATUSMASK		~(STATUS_STATUSNOTINSTALLED | STATUS_STATUSUNPACKED | STATUS_STATUSHALFCONFIGURED | STATUS_STATUSINSTALLED | STATUS_STATUSCONFIGFILES | STATUS_STATUSPOSTINSTFAILED | STATUS_STATUSREMOVALFAILED | STATUS_STATUSHALFINSTALLED)
#define DEPENDSMAX 				64
#define PACKAGE_EXTENSION		".opk"

#define TITLE(fmt,args...)  printf("\033[1;37m" fmt "\033[0m", ##args)
#define PRINT(fmt,args...)  printf( fmt, ##args)
#define ERROR(fmt,args...) printf("\033[1;31mError: \033[0m"fmt, ##args);
#define WARNING(fmt,args...) printf("\033[1;33mWarning: \033[0m"fmt, ##args);

#define	SILENT				0

typedef struct package_t * package_ptr;
typedef struct dep_node * dep_node_ptr;
typedef void (*apkg_callback_function) (package_ptr pkg);

typedef enum {
	SUCCESS,
	DIR_NOT_EXIST,
	DIR_EXIST,
	DIR_OPEN_FAILED,
	DIR_CREATE_FAILED,
	DIR_CLEAN_FAILED,
	DIR_CH_FAILED,
	FILE_NOT_EXIST,
	FILE_EXIST,
	FILE_CREATE_FAILED,
	FILE_READ_FAILED,
	FILE_RENAME_FAILED,
	FILE_REMOVE_FAILED,
	CONF_EXEC_FAILED,
	CONF_FAILED,
	UNKNOWN_COMP_TYPE,
	AR_ERROR,
	NO_PACKAGE_FOUND,
	FAILED,
}APKG_RET_CODE;

struct package_t {
	char file[256];
	char package[24];
	unsigned long status;
	char priority[24];
	char section[12];
	char maintainer[45];
	char version[45];
	char depends[128];
	char architecture[8];
	char description[256];

	struct package_t *next;
};

struct dep_node {
	char depname[24];
	dep_node_ptr next;
};


