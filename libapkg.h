/*
 * apkgh.h
 *
 *  Created on: Feb 7, 2012
 *      Author: root
 */

/* function prototypes */
#include "libapkg_defs.h"

void  apkg_get_dependency_list_of_directory();
void  apkg_get_dependency_list_of_package(char *package);
char* apkg_get_next_dependency();
void  apkg_release_dependency_list();
int   apkg_rmdir(char *);
int   apkg_rmdir_content(char *path);
int   apkg_mv(char *source, char *destination);
package_ptr construct_package_content(char *name, char *dir);
package_ptr get_package_file_info(char *file_name);
APKG_RET_CODE package_info(char *file);

APKG_RET_CODE apkg_init(char *);
APKG_RET_CODE apkg_unpack(package_ptr );
APKG_RET_CODE apkg_configure(package_ptr );
APKG_RET_CODE apkg_install(char *, package_ptr );
APKG_RET_CODE apkg_fields(package_ptr );
APKG_RET_CODE apkg_files_of_package(char *, package_ptr);
APKG_RET_CODE apkg_remove(char *, package_ptr );
APKG_RET_CODE apkg_print_packages();
APKG_RET_CODE apkg_status_of(char *, package_ptr );
APKG_RET_CODE apkg_install_all_unpacked();
void apkg_settings(int, int, int, char *);
void apkg_list_packages(char *, apkg_callback_function);
APKG_RET_CODE apkg_print_dep_tree(char *cached_directory, char *package);
APKG_RET_CODE apkg_print_dep_tree_of_dir(char *cached_directory);
