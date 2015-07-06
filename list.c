/*
 * list.c
 *
 *  Created on: Jan 17, 2012
 *      Author: root
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "libapkg_defs.h"
#include "list.h"

package_ptr alloc_package() {
	package_ptr p;
	p = (package_ptr) malloc(sizeof(struct package_t));
	memset(p, 0, sizeof(struct package_t));
	p->next = NULL;
	return p;
}

void add_package(package_ptr *head, package_ptr p) {
	p->next = *head;
	*head = p ;
}

void release_package_list(package_ptr *head) {
	package_ptr p;
	while(*head) {
		p = *head;
		*head = (*head)->next;
		free(p);
	}
}

void delete_package(package_ptr *head, char *package) {
	package_ptr back, front;
	for(back=NULL,front=*head; front != NULL && strcmp(front->package, package); back=front, front=front->next);
	if(front==NULL)
		return;
	else if(back==NULL) {
		*head = front->next;
		free(front);
		front = NULL;
	}
	else {
		back->next = front->next;
		free(front);
		front = NULL;
	}
}

void list_packages(package_ptr head) {
	package_ptr traverse;
	for(traverse=head; traverse != NULL; traverse=traverse->next)
		PRINT("%s\n", traverse->package);
}

