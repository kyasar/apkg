#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <search.h>
#include <utime.h>
#include <getopt.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/utsname.h>

#include "apkg.h"
#include "list.h"
#include "version.h"

package_ptr packages;	// non static for list
char *offline_root, *cached_dir, *custom_db;
int offline;
int cached;		// is different database used ?
int verbose;	// non static for status
int user;	// non static for status
int nodatabase;	// non static for status

void usage() {
	printf("\nAtlas Package Manager (APKG)\nLightweight Embedded Package Manager");
	printf(
			"\nThis application is used to install, remove, unpack and configure\n"
					"the debian formatted packages in your system.\n");
	printf("\nUsage: apkg [Options] [Command] <Pkgs>\n\n");
	printf("  OPTIONS\n");
	printf("\t  --offline=<path/to/dir>  		: offline installing of the packages, useful for host system\n");
	printf("\t  --cached=<path/to/dir>   		: prints the whole dependency list of packages in a directory \n");
	printf("\t  --no-database                 	: package installation details will not be saved to database\n");
	printf("\t  --custom-db=<name_of_custom_database> 	: does all operations in custom database /etc/apkg.<name>/ \n");
	printf("\t  --verbose                 		: for more output\n");
	printf("\t  -h                        		: display help message\n");
	printf("\t  -v                        		: display version info\n\n");
	printf("  COMMANDS\n");
	printf("\t  -i, --install   <pkgs>    		: install packages to system. At first unpacks them and then \n"
		   "\t   					  configure the packages unpacked successfully.\n"
		   "\t   					  full name of each package must be entered.\n");
	printf("\t  -r, --remove    <pkgs>    		: uninstall packages according to its names.\n");
	printf("\t  -l, --list                		: list all of installed packages with their statuses in database.\n");
	printf("\t  -u, --unpack    <pkgs>    		: unpack the given packages.\n");
	printf("\t  -c, --configure <pack-name>    	: configure the given packages. Be careful ! All given packages\n"
			"\t   					  must be unpacked already\n");
	printf("\t  -a, --confall             		: configure all packages which are only unpacked in database. \n"
		   "\t   					  You do not need to enter their names.\n");
	printf("\t  -f, --files     <pack-name>		: prints the contens of package.\n");
	printf("\t  -s, --status    <pack-name>		: gives the status of the package\n");
	printf("\t  -p, --init    --cached=<dir>	: Creates a database containing the infos about the packages in given directory\n\n");
	printf("\t  -d, --depends   <pkg>     		: prints all dependencies of a package to stdout\n\n");
	printf("\t  -t              <pkg-file-dest> 	: prints package info (control file contents)\n\n");

	printf("  PACKAGES\n");
	printf("\t <pkgs>       :  package list with full names in a directory\n");
	printf("\t <pack-name> 	:  name of a package (no need to extension)\n\n");


	printf("  EXAMPLES\n\n");
	printf("  offline install of waitind package\n");
	printf(
			"\tapkg --offline=/tmp/target/ -i waitind_1_00_0006_2012_01_08_17_54_7c7a7c1.opk\n\n");

	printf("  install updatemng package and print all outputs to stdout\n");
	printf(
			"\tapkg --verbose --install updatemng_1_00_0010_2012_01_11_16_00_9eecb5a.opk\n\n");

	printf("  remove waitind and updatemng packages\n");
	printf("\tapkg -r waitind updatemng\n\n");

	printf("  install iperf but do not insert it to database\n");
		printf("\tapkg --no-database -i iperf_2.0.5.opk\n\n");

	printf("  do unpack only waitind and updatemng packages\n");
	printf(
			"\tapkg -u waitind_1_00_0006_2012_01_08_17_54_7c7a7c1.opk  updatemng_1_00_0010_2012_01_11_16_00_9eecb5a.opk\n\n");

	printf(
			"  configure all unpacked packages in databases (eg. waitind and updatemng)\n");
	printf("\tapkg -a \n\n");
	printf(
			"  find all sub-dependencies of a package in a directory (cached) \n  (eg. all dependencies of waitind in cached directory /root/pack )\n");
		printf("\tapkg --cached=/root/pack --depends waitind \n\n");
	printf(
				"  find all sub-dependencies of all packages in a directory (cached) \n  (eg. all dependencies of cached directory /root/pack )\n");
			printf("\tapkg --cached=/root/pack --all-depends \n\n");
}

void create_packages_from_argv(int argc, char **argv, int *index)
{
	char *cwd = getcwd(0, 0);
	package_ptr p;
	char file_buffer[256];
	int i = *index - 1, c=0;

	while (i < argc && *argv[i] != '-') {
			p = alloc_package();
			if (*argv[i] == '/') {
				strcpy(p->file, argv[i]);
			} else {
				snprintf(file_buffer, sizeof(file_buffer), "%s/%s", cwd, argv[i]);
				strcpy(p->file, file_buffer);
			}
			strcpy(p->package, argv[i]);

			add_package(&packages, p);
			(*index)++;
			i++;
			c++;
	}
	(*index)--;
//	for(p=packages; p!= NULL; p=p->next)
//		printf("%d package: %s %s\n", c, p->package, p->file);
}

int main(int argc, char **argv)
{
	char *package_name = NULL;
	int operations = 0;

	struct option longopts[] = {
			{ "install", 		required_argument, 	0, 			'i' },
			{ "remove", 		required_argument, 	0, 			'r' },
			{ "list", 			no_argument, 		0, 			'l' },
			{ "unpack", 		required_argument, 	0, 			'u' },
			{ "configure", 		required_argument, 	0, 			'c' },
			{ "status", 		required_argument, 	0, 			's' },
			{ "offline", 		required_argument, 	0,			'o' },
			{ "verbose", 		no_argument, 		0, 			'V' },
			{ "no-database", 	no_argument, 		0,			'n' },
			{ "confall", 		no_argument, 		0, 			'a' },
			{ "depends", 		required_argument, 	0, 			'd' },
			{ "all-depends", 	no_argument, 		0, 			'x' },
			{ "files", 			required_argument, 	0, 			'f' },
			{ "init", 			required_argument, 	0, 			'p' },
			{ "cached", 		required_argument, 	0, 			'C' },
			{ "custom-db", 		required_argument, 	0, 			'D' },
			{ "info", 			required_argument, 	0, 			't' },
			{ "version", 		no_argument, 		0, 			'v' },
			{ "help", 			no_argument, 		0, 			'h' },
			{ NULL, 			0, 					NULL,			0 },
	};

	while (optind < argc) {
		switch (getopt_long(argc, argv, "i:r:lu:c:s:o:Vnad:xf:p:C:D:vht:", longopts, &optind)) {
			case 'i':
				operations |= OPERATION_INSTALL;
				create_packages_from_argv(argc, argv, &optind);
				break;
			case 'r':
				operations |= OPERATION_REMOVE;
				create_packages_from_argv(argc, argv, &optind);
				break;
			case 'l':
				operations |= OPERATION_LIST;
				break;
			case 'u':
				operations |= OPERATION_UNPACK;
				create_packages_from_argv(argc, argv, &optind);
				break;
			case 'c':
				operations |= OPERATION_CONFIGURE;
				create_packages_from_argv(argc, argv, &optind);
				break;
			case 's':
				operations |= OPERATION_STATUS;
				create_packages_from_argv(argc, argv, &optind);
				break;
			case 'o':
				offline = 1;
				offline_root = strdup(optarg);
				break;
			case 'V':
				verbose = 1;
				break;
			case 'n':
				nodatabase = 1;
				break;
			case 'a':
				operations |= OPERATION_CONFALL;
				break;
			case 'd':
				operations |= OPERATION_DEPENDS;
				create_packages_from_argv(argc, argv, &optind);
				break;
			case 'x':
				operations |= OPERATION_ALLDEPENDS;
				break;
			case 'f':
				operations |= OPERATION_FILES;
				create_packages_from_argv(argc, argv, &optind);
				return 0;
				break;
			case 'p':
				operations |= OPERATION_INIT;
				break;
			case 'C':
				cached_dir = strdup(optarg);
				break;
			case 'D':
				custom_db = strdup(optarg);
				break;
			case 't':
				operations |= OPERATION_INFO;
				package_name = strdup(optarg);
				break;
			case 'v':
				printf("Version    : %s\n", pkg_version);
				printf("Build Date : "__DATE__" "__TIME__"\n");
				exit(EXIT_SUCCESS);
			case 'h':
				usage();
				exit(EXIT_SUCCESS);
			default:
				exit(EXIT_FAILURE);
		}
	}

	// set Apkg library settings
	apkg_settings(verbose, offline, nodatabase, offline_root);

	if (operations & OPERATION_INSTALL) {
		return apkg_install(custom_db, packages);
	}
	else if (operations & OPERATION_REMOVE) {
		return apkg_remove(custom_db, packages);
	}
	else if (operations & OPERATION_LIST) {
		return apkg_print_packages(custom_db); 	// list all packages in status file
	}
	else if (operations & OPERATION_UNPACK) {
		return apkg_unpack(packages);
	}
	else if (operations & OPERATION_CONFIGURE) {
		return apkg_configure(packages);
	}
	else if (operations & OPERATION_CONFALL) {
		return apkg_install_all_unpacked();
	}
	else if (operations & OPERATION_STATUS) {
		return apkg_status_of(custom_db, packages);
	}
	else if (operations & OPERATION_DEPENDS) {
		return apkg_print_dep_tree(cached_dir, packages->package);
	}
	else if (operations & OPERATION_ALLDEPENDS) {
		return apkg_print_dep_tree_of_dir(cached_dir);
	}
	else if (operations & OPERATION_FILES) {
		return apkg_files_of_package(custom_db, packages);
	}
	else if (operations & OPERATION_INIT) {
		return apkg_init(cached_dir);
	}
	else if (operations & OPERATION_INFO) {
		return package_info(package_name);
	}
	else
		return FAILED;
}
