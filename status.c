#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <search.h>
#include "libapkg_defs.h"
#include "util.h"
#include "list.h"
#include "status.h"

extern int verbose;
extern int offline;
extern int user;
extern char *offline_apkg_dir;
extern char *user_apkg_dir;

static const char *statuswords[][10] = { { (char *) STATUS_WANTSTART, "unknown",
		"install", "hold", "deinstall", "purge", 0 }, {
		(char *) STATUS_FLAGSTART, "ok", "reinstreq", "hold", "hold-reinstreq",
		0 }, { (char *) STATUS_STATUSSTART, "not-installed", "unpacked",
		"half-configured", "installed", "half-installed", "config-files",
		"post-inst-failed", "removal-failed", 0 } };

int package_compare(const void *p1, const void *p2) {
	// insert packages to tree in name order
	return strcmp(((package_ptr) p1)->package, ((package_ptr) p2)->package);
}

void control_read(FILE *f, package_ptr p) {
	char buf[BUFSIZE];
	while (fgets(buf, BUFSIZE, f) && !feof(f)) // read inside of control file or status file and fill the package structure
	{
		buf[strlen(buf) - 1] = 0;
		if (*buf == 0) // empty line means infos about a package is finished and new package info will start
			return;
		else   if (strstr(buf, "Package: ") == buf) {
			strcpy(p->package, buf + 9);
		} else if (strstr(buf, "Status: ") == buf) {
			p->status = status_parse(buf + 8);
		} else if (strstr(buf, "Depends: ") == buf) {
			strcpy(p->depends, buf + 9);
		} else if (strstr(buf, "Architecture: ") == buf) {
			strcpy(p->architecture, buf + 14);
		} else if (strstr(buf, "Description: ") == buf) {
			strcpy(p->description, buf + 13);
		} else if (strstr(buf, "Priority: ") == buf) {
			strcpy(p->priority, buf + 10);
		} else if (strstr(buf, "Section: ") == buf) {
			strcpy(p->section, buf + 9);
		} else if (strstr(buf, "Maintainer: ") == buf) {
			strcpy(p->maintainer, buf + 12);
		} else if (strstr(buf, "Version: ") == buf) {
			strcpy(p->version, buf + 9);
		}
	}
}

char *status_print(unsigned long flags) {
	/* this function returns a static buffer... */
	static char buf[256];
	int i, j;

	buf[0] = 0;
	for (i = 0; i < 3; i++) {
		j = 1;
		while (statuswords[i][j] != 0) {
			if ((flags & (1 << ((long) statuswords[i][0] + j - 1))) != 0) {
				strcat(buf, statuswords[i][j]);
				if (i < 2)
					strcat(buf, " ");
				break;
			}
			j++;
		}
		if (statuswords[i][j] == 0) {
			if (verbose)
				WARNING("Corrupted status flag!!: %lx\n", flags);
			return NULL;
		}
	}
	return buf;
}

unsigned long status_parse(const char *line) {
	char *p;
	int i, j;
	unsigned long l = 0;
	for (i = 0; i < 3; i++) {
		p = strchr(line, ' ');
		if (p)
			*p = 0;
		j = 1;
		while (statuswords[i][j] != 0) {
			if (strcmp(line, statuswords[i][j]) == 0) {
				l |= (1 << ((long) statuswords[i][0] + j - 1)); // covert each status word to corresponding bit
				break;
			}
			j++;
		}
		if (statuswords[i][j] == 0)
			return 0; /* parse error */
		line = p + 1;
	}
	return l;
}

void *status_read(char *status_file, int verbose) {
	FILE *f;
	void *status = 0;
	package_ptr m = 0;

	if (verbose)
		TITLE("(Reading database...)\n");
	if (!status_file) // if database file not set, use default one
		status_file = STATUSFILE;
	if ((f = fopen(status_file, "r")) == NULL) {
		ERROR("Status file %s cannot be opened\n", status_file);
		return 0;
	}
	while (!feof(f)) {
		m = (package_ptr) malloc(sizeof(struct package_t));
		memset(m, 0, sizeof(struct package_t));
		control_read(f, m); // read package attributes
		if (m->package) {
			tdelete(m, &status, package_compare); // if there is a package with same name, delete it first
			tsearch(m, &status, package_compare); // add the package to tree
		} else {
			free(m);
		}
	}
	fclose(f);
	return status;
}

APKG_RET_CODE status_merge(void *status, char *status_file, package_ptr pkgs) {

	FILE *fin, *fout;
	char buf[BUFSIZE], *cwd;
	package_ptr pkg = 0, statpkg = 0;
	struct package_t locpkg;
	int ret = 0;

	if (verbose)
		TITLE("(Updating database...)\n");
	if (!status_file) 				// if database file not set, use default one /etc/apkg/status
		status_file = STATUSFILE;

	// Where database file(status) and back-ups will be stored ?
	cwd = getcwd(0, 0); 			// create bak and new status files in corresponding directory
	if (offline) 										// offline root used
		ret = chdir(offline_apkg_dir);
	else {
		if (user) 											// user database will be used
			ret = chdir(user_apkg_dir);
		else if (strcmp(status_file, STATUSFILE) != 0) 		// external database will be used
			ret = chdir(TMPAPKGDIR);
		else
			ret = chdir(ADMINDIR);							// normal installation
	}

	if ((fin = fopen(status_file, "r")) == NULL) {
		ERROR("Cannot open the status file : %s\n", status_file);
		ret = chdir(cwd);								// restore cwd
		return FILE_READ_FAILED;
	}
	if ((fout = fopen("status.new", "w")) == NULL) {
		ERROR("Cannot create new file : %s/%s \n", getcwd(0, 0), "status.new");
		ret = chdir(cwd);								// restore cwd
		return FILE_CREATE_FAILED;
	}

	while (fgets(buf, BUFSIZE, fin) && !feof(fin)) {
		buf[strlen(buf) - 1] = 0;

		if (strstr(buf, "Package: ") == buf) {
			for (pkg = pkgs; pkg != 0 && strcmp(buf + 9, pkg->package) != 0;
					pkg = pkg->next)
				;
			strcpy(locpkg.package, buf + 9);
			statpkg = tfind(&locpkg, &status, package_compare);

			if (statpkg != 0)
				statpkg = *(package_ptr *) statpkg;
		}
		if (pkg != 0)
			continue;

		if (strstr(buf, "Status: ") == buf && statpkg != 0) {
			snprintf(buf, sizeof(buf), "Status: %s",
					status_print(statpkg->status));
		}
		fputs(buf, fout);
		fputc('\n', fout);
	}

	// write new packages to database
	for (pkg = pkgs; pkg != 0; pkg = pkg->next) {
		if ((pkg->status & STATUS_WANTUNKNOWN)) // this package is not installed before
			continue;
		fprintf(fout, "Package: %s\nStatus: %s\n", pkg->package,
				status_print(pkg->status));

		if (pkg->priority)
			fprintf(fout, "Priority: %s\n", pkg->priority);
		if (pkg->section)
			fprintf(fout, "Section: %s\n", pkg->section);
		if (pkg->architecture)
			fprintf(fout, "Architecture: %s\n", pkg->architecture);
		if (pkg->maintainer)
			fprintf(fout, "Maintainer: %s\n", pkg->maintainer);
		if (pkg->version)
			fprintf(fout, "Version: %s\n", pkg->version);
		if (pkg->depends)
			fprintf(fout, "Depends: %s\n", pkg->depends);
		if (pkg->description)
			fprintf(fout, "Description: %s\n", pkg->description);
		fputc('\n', fout);
	}

	fclose(fin);
	fclose(fout);

	ret = rename(status_file, "status.bak"); // back-up the database
	if (ret == 0) {
		ret = rename("status.new", status_file); // update database file
	}
	ret = chdir(cwd);

	return SUCCESS;
}
