/*
 * depends.c
 *
 *  Created on: Feb 7, 2012
 *      Author: root
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <search.h>
#include "libapkg_defs.h"
#include "status.h"
#include "depends.h"

extern package_ptr cached_packages; // head of list of packages in cached directory
dep_node_ptr full_dependency_list; // list of dependencies in setup directory
dep_node_ptr dependency_list_of_package; // sub dependencies of a package
dep_node_ptr current;
void *status_cached;

package_ptr new_package() {
	package_ptr p;
	p = (package_ptr) malloc(sizeof(struct package_t));
	memset(p, 0, sizeof(struct package_t));
	p->next = NULL;
	return p;
}

void add_cached_package_to_list(package_ptr *head, package_ptr p) {

	p->next = *head;
	*head = p;
}

void list_cached_packages() {

	package_ptr traverse;
	for (traverse = cached_packages; traverse != NULL;
			traverse = traverse->next)
		PRINT("%s - %s - %s\n",
				traverse->package, traverse->file, traverse->depends);
}

dep_node_ptr alloc_dependency_node(char *depname) {
	dep_node_ptr p;
	p = (dep_node_ptr) malloc(sizeof(struct dep_node));
	strcpy(p->depname, depname);
	p->next = NULL;

	return p;
}

void add_dependency_node_to_list(dep_node_ptr *head, char *depname) {
	dep_node_ptr p = *head, b;
	if (!p) { // no element in list before
		*head = alloc_dependency_node(depname);
		return;
	}
	for (; p != NULL; b = p, p = p->next) // go to end of list
		if (strcmp(p->depname, depname) == 0) // is it added already ?
			return;
	b->next = alloc_dependency_node(depname);
}

void delete_dependency_node_from_list(dep_node_ptr* head, char *depname) {

	dep_node_ptr j, p;
	for (j = NULL, p = *head; p != NULL; j = p, p = p->next) {
		if (!strcmp(p->depname, depname)) {
			if (!j) {
				*head = p->next;
				free(p);
				p = NULL;
			} else {
				j->next = p->next;
				free(p);
				p = NULL;
			}
		}
	}
}

char* apkg_get_next_dependency() {

	dep_node_ptr back;
	char *dependency_name;

	if (!current) // Maybe user can force to get a node from empty list
		return NULL;
	dependency_name = strdup(current->depname);
	back = current;
	current = current->next; // this dependency is given to user skip next
	free(back);

	return dependency_name;
}

// release the list of dependencies in a directory
void apkg_release_dependency_list() {

	dep_node_ptr back;
	while (full_dependency_list) {
		back = full_dependency_list;
		full_dependency_list = full_dependency_list->next;
		free(back);
		back = NULL;
	}
}

// release the list of packages in cached directory
void apkg_release_cached() {
	package_ptr p;
	while (cached_packages) {
		p = cached_packages;
		cached_packages = cached_packages->next;
		free(p);
		p = NULL;
	}
}

char **split_dependency_line(const char *dependsstr) {
	static char *dependsvec[DEPENDSMAX];
	char *p;
	int i = 0;

	dependsvec[0] = 0;

	if (dependsstr != 0) {
		p = strdup(dependsstr);
		while (*p != 0 && *p != '\n') {
			if (*p != ' ') {
				if (*p == ',') {
					*p = 0;
					dependsvec[++i] = 0;
				} else if (dependsvec[i] == 0)
					dependsvec[i] = p;
			} else
				*p = 0;
			p++;
		}
		*p = 0;
	}
	dependsvec[i + 1] = 0;
	return dependsvec;
}

// this function searches all sub-dependencies of a package recursively
// pkg argument is a package struct that contains only name of the package
// whose dependecny tree will be builded.
void find_all_subdependencies_of_package(package_ptr pkg) {
	package_ptr p;
	void *package_in_tree;
	char **dependencies;
	int i, num_of_dependencies = 0;

	package_in_tree = tfind((void *) pkg, &status_cached, package_compare);
	if (package_in_tree == NULL) {
		return;
	}

	p = *(package_ptr *) package_in_tree;
	dependencies = split_dependency_line(p->depends);
	for (i = 0; dependencies[i] != NULL; i++)
		num_of_dependencies++;

	struct package_t pkgs[num_of_dependencies];
	for (i = 0; i < num_of_dependencies; i++)
		strcpy(pkgs[i].package, dependencies[i]);

	for (i = 0; i < num_of_dependencies; i++) {
		if (strcmp(pkgs[i].package, p->package) == 0)
			continue;
		find_all_subdependencies_of_package(&pkgs[i]);
	}
	add_dependency_node_to_list(&dependency_list_of_package, p->package);
}

void merge_dependency_lists() {

	dep_node_ptr tail, full, p;

	if (full_dependency_list == NULL) { // dependency list is empty at first so copy all of them
		full_dependency_list = dependency_list_of_package;
		return;
	}

	for (tail = full_dependency_list; tail->next != NULL; tail = tail->next)
		// go to end of full dependency list
		;
	for (p = dependency_list_of_package; p != NULL; p = p->next) {
		for (full = full_dependency_list; full != NULL; full = full->next)
			if (strcmp(full->depname, p->depname) == 0) // this dependency is added to full list already
				break; // skip the next dependency of package
		if (full == NULL) { // dependency not found, it will be added to full list
			tail->next = alloc_dependency_node(p->depname); // create new dependency node and add it
			tail = tail->next; // go to end
		}
	}

	// free the package dependency list
	dep_node_ptr back;
	while (dependency_list_of_package) {
		back = dependency_list_of_package;
		dependency_list_of_package = dependency_list_of_package->next;
		free(back);
		back = NULL;
	}
}

// used with apkg_init
void apkg_get_dependency_list_of_directory() {

	package_ptr traverse;
	char buffer[256];

	snprintf(buffer, sizeof(buffer), "%s/%s", TMPAPKGDIR, "status");
	status_cached = status_read(buffer, SILENT); // read the database file in setup directory

	apkg_release_dependency_list(); // make the list empty

	// for all packages in cached directory
	// cached_packages list contains all packages in directory db
	for (traverse = cached_packages; traverse != NULL;
			traverse = traverse->next) {

		dependency_list_of_package = NULL; // make it empty  for each package
		find_all_subdependencies_of_package(traverse); // all sub dependencies of current package is found
		merge_dependency_lists(); // merge lists
	}

	apkg_release_cached();
	// set current pointer to head of full dependency list of packages in setup directory
	current = full_dependency_list;
}

void configure_all_unpacked(const void *nodep, const VISIT which,
		const int depth) {
	package_ptr pkg;

	switch (which) {
	case preorder:
		break;
	case postorder:
		pkg = *(package_ptr *) nodep;
		if (pkg->status & STATUS_STATUSUNPACKED) {		// find subdependencies of an unpacked package
			add_cached_package_to_list(&cached_packages, pkg);
		}
		break;
	case endorder:
		break;
	case leaf:
		pkg = *(package_ptr *) nodep;
		if (pkg->status & STATUS_STATUSUNPACKED) {
			add_cached_package_to_list(&cached_packages, pkg);
		}
		break;
	}
}

void apkg_get_dependency_list_of_unpacked_packages() {

	package_ptr traverse;

	status_cached = status_read(NULL, SILENT); 	// read default database file
	apkg_release_dependency_list(); 			// make the list empty

	twalk(status_cached, configure_all_unpacked);	// find all packages in database

	// set current pointer to head of full dependency list of packages in setup directory
	apkg_release_dependency_list(); // make the list empty

	// for all packages in cached directory
	// cached_packages list contains all packages in directory db
	for (traverse = cached_packages; traverse != NULL;
			traverse = traverse->next) {

		dependency_list_of_package = NULL; // make it empty  for each package
		find_all_subdependencies_of_package(traverse); // all sub dependencies of current package is found
		merge_dependency_lists(); // merge lists
	}

	apkg_release_cached();
	// set current pointer to head of full dependency list of packages in setup directory
	current = full_dependency_list;
}

void apkg_get_dependency_list_of_package(char *package) {

	package_ptr traverse;
	char buffer[256];

	snprintf(buffer, sizeof(buffer), "%s/%s", TMPAPKGDIR, "status");
	status_cached = status_read(buffer, SILENT); // read the database file in setup directory

	apkg_release_dependency_list(); // make the list empty
	dependency_list_of_package = NULL; // make it empty  for each package

	for (traverse = cached_packages; traverse != NULL;
				traverse = traverse->next) {
		if( strcmp(traverse->package, package) == 0 )
			break;
	}
	if(!traverse)	// package is not in cached directory
		return;
	find_all_subdependencies_of_package(traverse); // all sub dependencies of package
	merge_dependency_lists(); // merge lists

	apkg_release_cached();
	// set current pointer to head of full dependency list of packages in setup directory
	current = full_dependency_list;
}


