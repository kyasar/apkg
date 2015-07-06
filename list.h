/*
 * list.h
 *
 *  Created on: Jan 19, 2012
 *      Author: root
 */

package_ptr alloc_package();
void add_package(package_ptr *,package_ptr);
void delete_package(package_ptr *, char *);
void list_packages(package_ptr);
void release_package_list(package_ptr *head);

