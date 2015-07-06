/*
 * libapkg.c
 *
 *  Created on: Jan 17, 2012
 *      Author: root
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <search.h>
#include <utime.h>
#include <getopt.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/utsname.h>
#include "libapkg_defs.h"
#include "util.h"
#include "status.h"
#include "depends.h"

void *status;
extern char *compression_type;
extern char *decompression_tool;
extern char *offline_root;

extern int verbose;	// externs from util.c
extern int offline;
extern int user;
extern int nodatabase;
package_ptr cached_packages;
apkg_callback_function give_installed_packages_callback; // global pointer to callback function

char *offline_etc_dir;
char *offline_apkg_dir;
char *offline_status_file;
char *offline_info_dir;

// user directories
char *user_apkg_dir;
char *user_status_file;
char *user_info_dir;

static int packet_count;

package_ptr get_package_file_info(char *file_name)
{
	package_ptr pkg;
	int r;
	char *cwd = 0, *p;
	char buf[1024], buf2[1024];
	FILE *f;

	pkg = (package_ptr) malloc(sizeof(struct package_t));
	memset(pkg, 0, sizeof(struct package_t));
	pkg->next = NULL;
	strcpy(pkg->file, file_name);

	p = strrchr(pkg->file, '/'); // paketin ismini al
	if (p)
		p++;
	else
		p = pkg->file;
	p = strcpy(buf2, p);

	while (*p != 0 && *p != '_' && *p != '.')
		p++; // paket ismini ayıkla _ . 0 (.uzantı) atıldı
	*p = 0;
	p = pkg->package;
	strcpy(pkg->package, buf2);

	cwd = getcwd(0, 0);

	if (mkdir(TMPAPKGDIR, S_IRWXU)) { // create temp directory to extract files
		if(errno != EEXIST) {
			ERROR( "Creating directory %s failed: %s\n", TMPAPKGDIR, strerror(errno));
			return NULL;
		}
		apkg_rmdir_content(TMPAPKGDIR);	// delete the content of tmp dir
	}

	if (mkdir(APKGCIDIR, S_IRWXU)) { // create temp directory to extract files
		if(errno != EEXIST) {
			ERROR( "Creating directory %s failed: %s\n", APKGCIDIR, strerror(errno));
			return NULL;
		}
		printf("already created %s\n", APKGCIDIR);
	}

	if (identify_compression_type(pkg) != SUCCESS) // check the compression type of package
		return NULL;

	snprintf(buf, sizeof(buf), "%s%s", APKGCIDIR, buf2);

	if (mkdir(buf, S_IRWXU) != 0) { // geçici dizinde paket isminde bir alt dizin aç
		ERROR( "Creating directory of %s failed: %s\n", buf, strerror(errno));
		return NULL;
	}
	if (chdir(buf) != 0) // /tmp/apkg/tmp.ci/<package>/ dizini altında control.tar ı aç
	{
		ERROR( "Change directory to %s failed: %s\n", buf, strerror(errno));
		return NULL;
	}
	if (strcmp(compression_type, "lzo") == 0)
		snprintf(buf, sizeof(buf), "ar -p %s control.tar.lzo | lzop -d | tar xf -", pkg->file);
	else
		snprintf(buf, sizeof(buf), "ar -p %s control.tar.gz | tar xzf -", pkg->file);

	if ((r = system(buf)) != 0) {
		ERROR( "%s exited with status %d\n", buf, r);
		return NULL;
	}
	if ((f = fopen("control", "r")) == NULL) // control dosyasını oku, paket bilgilerini yükle
	{
		ERROR( "Opening control file failed: %s\n", strerror(errno));
		return NULL;
	}
	control_read(f, pkg);

	r = chdir(cwd); // chdir( /paketin/bulundugu/dizin )
	free(cwd);

	return pkg;
}

APKG_RET_CODE package_info(char *file) {

	printf("Package file: %s\n", file);
	package_ptr p = get_package_file_info(file);
	if(p) {
		printf("%-12s: %s\n", "package", p->package);
		printf("%-12s: %s\n", "description", p->description);
		printf("%-12s: %s\n", "depends", p->depends);
		printf("%-12s: %s\n", "version", p->version);
		printf("%-12s: %s\n", "maintainer", p->maintainer);
		printf("%-12s: %s\n", "architecture", p->architecture);
		return SUCCESS;
	}
	return FILE_NOT_EXIST;
}

package_ptr construct_package_content(char *name, char *dir) {
	package_ptr p;
	char buffer[256];

	p = (package_ptr) malloc(sizeof(struct package_t));
	memset(p, 0, sizeof(struct package_t));
	p->next = NULL;

	strcpy(p->package, name);
	snprintf(buffer, sizeof(buffer), "%s/%s.opk", dir, name);
	strcpy(p->file, buffer);

	return p;
}

void list_tree_action(const void *nodep, const VISIT which, const int depth) {
	package_ptr pkg;

	switch (which) {
	case preorder:
		break;
	case postorder:
		pkg = *(package_ptr *) nodep;
		if(pkg->status & STATUS_STATUSINSTALLED)		// only installed packages
			give_installed_packages_callback(pkg);
		break;
	case endorder:
		break;
	case leaf:
		pkg = *(package_ptr *) nodep;
		if(pkg->status & STATUS_STATUSINSTALLED)
			give_installed_packages_callback(pkg);
		break;
	}
}

void travel_package_tree(const void *nodep, const VISIT which, const int depth) {
	package_ptr pkg;

	switch (which) {
	case preorder:
		break;
	case postorder:
		pkg = *(package_ptr *) nodep;
		if(pkg->status & STATUS_STATUSINSTALLED) {
			PRINT("%-8s%-24s%-44s\n", "INS" , pkg->package, pkg->version);
			packet_count++;
		}
		break;
	case endorder:
		break;
	case leaf:
		pkg = *(package_ptr *) nodep;
		if(pkg->status & STATUS_STATUSINSTALLED) {
			PRINT("%-8s%-24s%-44s\n", "INS" , pkg->package, pkg->version);
			packet_count++;
		}
		break;
	}
}

void apkg_list_packages(char *db_name, apkg_callback_function callback) {

	char db_file[256];

	check_dirs_and_files(db_name);			// create necessary files and directories (also for OFFLINE)
	
	if(db_name == NULL)
		status = status_read(db_name, SILENT); // load default database
	else {
		snprintf(db_file, sizeof(db_file), "%s/apkg.%s/status", ETCDIR, db_name);
		status = status_read(db_file, SILENT); // load default database
	}

	if (status) {
		give_installed_packages_callback = callback; // set callback function
		twalk(status, list_tree_action); // travel the tree and call back user function for each package
	}
}

APKG_RET_CODE apkg_doconfigure(package_ptr pkg) {

	char postinst[1024];
	char config[1024];
	char buf[1024];
	int ret;

	if (verbose)
		PRINT("Configuring %s ..\n", pkg->package);

	if (!(pkg->status & STATUS_STATUSUNPACKED)) {
		if (pkg->status & STATUS_STATUSINSTALLED) {
			WARNING("Package %s is configured already.\n", pkg->package);
			return SUCCESS;
		}
		else {
			ERROR("Package %s is not unpacked already, unpack it at first.\n", pkg->package);
			return CONF_FAILED;
		}
	}

	pkg->status &= STATUS_STATUSMASK;

	if(user)
		snprintf(config, sizeof(config), "%s%s.config", user_info_dir, pkg->package);
	else
		snprintf(config, sizeof(config), "%s%s.config", INFODIR, pkg->package);
//	snprintf(config, sizeof(config), "%s%s.config", INFODIR, pkg->package);
	if (is_file(config)) {
		snprintf(buf, sizeof(buf), "exec %s configure", config);
		if ((ret = system(buf)) != 0) {
			ERROR( "Executing of %s exited with status %d\n", config, ret);
			pkg->status |= STATUS_STATUSHALFCONFIGURED;
			return CONF_EXEC_FAILED;
		}
	}

	if(user)
		snprintf(postinst, sizeof(postinst), "%s%s.postinst", user_info_dir, pkg->package);
	else
		snprintf(postinst, sizeof(postinst), "%s%s.postinst", INFODIR, pkg->package);
//	snprintf(postinst, sizeof(postinst), "%s%s.postinst", INFODIR, pkg->package);
	if (is_file(postinst)) {
		snprintf(buf, sizeof(buf), "exec %s configure", postinst);
		if ((ret = system(buf)) != 0) {
			ERROR( "%s's postinst exited with status %d\n",
					pkg->package, ret);
			pkg->status |= STATUS_STATUSHALFCONFIGURED;
			return CONF_EXEC_FAILED;
		}
	}
	pkg->status &= STATUS_STATUSMASK;
	pkg->status |= STATUS_STATUSINSTALLED;

	return SUCCESS;
}

APKG_RET_CODE apkg_dounpack(package_ptr pkg) {

	const char *adminscripts[] = { "prerm", "postrm", "preinst", "postinst",
			"conffiles", "md5sums", "shlibs", "templates", "menutest",
			"isinstallable", "config" };
	char buffer[1024], buffer2[1024];
	int i, ret;
	char *cwd, *p;
	FILE *infp = NULL, *outfp = NULL;

	if (verbose)
		PRINT("Unpacking %s ..\n", pkg->package);
	cwd = getcwd(0, 0);

	// In offline mode, all files be installed in given directory
	// Id user mode is active, files of user applications go to normal file system
	if (offline)
		ret = chdir(offline_root);
	else
		ret = chdir("/");

	if (identify_compression_type(pkg) != SUCCESS) // check the compression type of package
		return UNKNOWN_COMP_TYPE;

	if (strcmp(compression_type, "lzo") == 0) // LZOP un extract parametresi -x
		snprintf(buffer, sizeof(buffer), "ar -p %s data.tar.%s | %s -d | tar -x",
				pkg->file, compression_type, decompression_tool);
	else if (strcmp(compression_type, "gz") == 0)
		snprintf(buffer, sizeof(buffer), "ar -p %s data.tar.%s | %s -c | tar -x",
				pkg->file, compression_type, decompression_tool);
	else {
		ERROR( "Unrecognized compression format: %s\n", compression_type);
		return UNKNOWN_COMP_TYPE;
	}

	if ((ret = system(buffer)) != 0 ) {
		ret = 1;
	} else {
		// copy scripts to info directory
		for (i = 0; i < sizeof(adminscripts) / sizeof(adminscripts[0]); i++) {

			snprintf(buffer, sizeof(buffer), "%s%s/%s", APKGCIDIR, pkg->package,
					adminscripts[i]);
			if(is_file(buffer)) {
				if(offline)		// copy to offline directory
					snprintf(buffer2, sizeof(buffer), "%s%s.%s", offline_info_dir, pkg->package,
						adminscripts[i]);
				else if(user)
					snprintf(buffer2, sizeof(buffer), "%s%s.%s", user_info_dir, pkg->package,
										adminscripts[i]);
				else
					snprintf(buffer2, sizeof(buffer), "%s%s.%s", INFODIR, pkg->package,
										adminscripts[i]);

				ret = apkg_copyfile(buffer, buffer2);
				if(verbose)
					PRINT("%s is copied to %s info dir.\n", buffer, buffer2);
				if (ret < 0) {
					ERROR( "Cannot copy %s to %s: %s\n",
							buffer, buffer2, strerror(errno));
					ret = 1;
					break;
				}
			}
		}

		if (strcmp(compression_type, "lzo") == 0) // comp. tool is lzop
			snprintf(buffer, sizeof(buffer), "ar -p %s data.tar.%s | %s -d | tar -t",
					pkg->file, compression_type, decompression_tool);
		else
			// comp. tool is gzip
			snprintf(buffer, sizeof(buffer), "ar -p %s data.tar.%s | %s -c | tar -t",
					pkg->file, compression_type, decompression_tool);

		if(offline)
			snprintf(buffer2, sizeof(buffer2), "%s%s.list", offline_info_dir, pkg->package);
		else if(user)
			snprintf(buffer2, sizeof(buffer2), "%s%s.list", user_info_dir, pkg->package);
		else
			snprintf(buffer2, sizeof(buffer2), "%s%s.list", INFODIR, pkg->package);

		// Create .list info file for package
		if ((infp = popen(buffer, "r")) == NULL
				|| (outfp = fopen(buffer2, "w")) == NULL) {
			ERROR( "Cannot create pipe : %s\n", buffer2);
			ret = 1;
		} else {
			// write extracted file names to .list file
			while (fgets(buffer, sizeof(buffer), infp) && !feof(infp)) {
				p = buffer;
				if (*p == '.')
					p++;
				if (*p == '/' && *(p + 1) == '\n') {
					*(p + 1) = '.';
					*(p + 2) = '\n';
					*(p + 3) = 0;
				}
				if (p[strlen(p) - 2] == '/') {
					p[strlen(p) - 2] = '\n';
					p[strlen(p) - 1] = 0;
				}

				fputs(p, outfp);
			}
			pkg->status &= STATUS_WANTMASK;
			pkg->status |= STATUS_WANTINSTALL;
			pkg->status &= STATUS_FLAGMASK;
			pkg->status |= STATUS_FLAGOK;
			pkg->status &= STATUS_STATUSMASK;
			pkg->status |= STATUS_STATUSUNPACKED; // mark package as unpacked
		}
		pclose(infp);
		fclose(outfp);
	}
	ret = chdir(cwd);

	if(ret)
		return FAILED;
	return SUCCESS;
}

APKG_RET_CODE apkg_unpackcontrol(package_ptr pkg) {
	int r;
	char *cwd = 0, *p;
	char buf[1024], buf2[1024];
	FILE *f;

	p = strrchr(pkg->file, '/'); // paketin ismini al
	if (p)
		p++;
	else
		p = pkg->file;
	p = strcpy(buf2, p);

	while (*p != 0 && *p != '_' && *p != '.')
		p++; // paket ismini ayıkla _ . 0 (.uzantı) atıldı
	*p = 0;
	p = pkg->package;
	strcpy(pkg->package, buf2);

// 	printf("@@@ p: %s, pkg->package: %s, buf2: %s\n", p, pkg->package, buf2);

	cwd = getcwd(0, 0);

	if (identify_compression_type(pkg) != SUCCESS) // check the compression type of package
		return UNKNOWN_COMP_TYPE;

	snprintf(buf, sizeof(buf), "%s%s", APKGCIDIR, pkg->package);

	if (mkdir(buf, S_IRWXU) != 0) { // geçici dizinde paket isminde bir alt dizin aç
		ERROR( "Creating directory of %s failed: %s\n", buf, strerror(errno));
		return DIR_CREATE_FAILED;
	}
	if (chdir(buf) != 0) // /tmp/apkg/tmp.ci/<package>/ dizini altında control.tar ı aç
			{
		ERROR( "Change directory to %s failed: %s\n", buf, strerror(errno));
		return DIR_CH_FAILED;
	}
	if (strcmp(compression_type, "lzo") == 0)
		snprintf(buf, sizeof(buf),
				"ar -p %s control.tar.lzo | lzop -d | tar xf -", pkg->file);
	else
		snprintf(buf, sizeof(buf), "ar -p %s control.tar.gz | tar xzf -",
				pkg->file);

	if ((r = system(buf)) != 0) {
		ERROR( "%s exited with status %d\n", buf, r);
		return AR_ERROR;
	}
	if ((f = fopen("control", "r")) == NULL) // control dosyasını oku, paket bilgilerini yükle
	{
		ERROR( "Opening control file failed: %s\n", strerror(errno));
		return FILE_READ_FAILED;
	}
	control_read(f, pkg);

	// package ismi ile control dosyasındaki ismi farklı olursa eşitleme yap
	if (strcmp(pkg->package, p) != 0) {
		snprintf(buf, sizeof(buf), "%s%s", APKGCIDIR, p);
		snprintf(buf2, sizeof(buf2), "%s%s", APKGCIDIR, pkg->package);
		if (rename(buf, buf2) != 0) {
			ERROR( "Renaming file %s to %s failed: %s\n", buf, buf2, strerror(errno));
			return FILE_RENAME_FAILED;
		}
	}

	r = chdir(cwd); // chdir( /paketin/bulundugu/dizin )
	free(cwd);
	return SUCCESS;
}

/* Creates a database containing the infos about the packages in cached directory
 * cached directory is given as parameter (path)
 */
APKG_RET_CODE apkg_init(char *path) {

	struct dirent *files;
	char package_file_path[256];
	char buffer[256];
	char *extension;
	void *status;
	FILE *fd;
	DIR *dp;
	package_ptr p;

	if ((dp = opendir(path)) == NULL) {
		ERROR("Directory cannot be opened : %s\n", path);
		return DIR_OPEN_FAILED;
	}

	if (check_dirs_and_files(NULL))
		return DIR_CREATE_FAILED;

	apkg_rmdir_content(TMPAPKGDIR);	// delete the content of tmp dir
	if (mkdir(APKGCIDIR, S_IRWXU)) { // create temp directory to extract files
		ERROR( "Creating directory %s failed: %s\n", APKGCIDIR, strerror(errno));
		return DIR_CREATE_FAILED;
	}

	cached_packages = NULL;
	strcpy(package_file_path, path);
	while ((files = readdir(dp)) != NULL) {

		if (!strcmp(files->d_name, ".") || !strcmp(files->d_name, ".."))
			continue;
		extension = files->d_name;
		extension = extension + strlen(files->d_name) - 4;
		if (strcmp(extension, PACKAGE_EXTENSION)) // is there anything which is not a package in dir ?
			continue;

		p = new_package();
		snprintf(buffer, sizeof(buffer), "%s/%s", package_file_path, files->d_name);
		strcpy(p->file, buffer);

		apkg_unpackcontrol(p); // read control file and fill structure
		add_cached_package_to_list(&cached_packages, p); // create a linked list of packages	}
	}

	// create database file in tmp directory
	snprintf(buffer, sizeof(buffer), "%s/%s", TMPAPKGDIR, "status");

	if ((fd = fopen(buffer, "w")) < 0) { // create empty database file
		ERROR("Creating cached database file %s failed\n", buffer);
		return FILE_CREATE_FAILED;
	}
	fclose(fd);

	status = status_read(buffer, SILENT);
	status_merge(status, buffer, cached_packages);

	if (apkg_rmdir(APKGCIDIR) != 0) 	// remove the content of tmp dir
		return DIR_CLEAN_FAILED;
	return SUCCESS;
}

APKG_RET_CODE apkg_unpack(package_ptr pkgs) {
	package_ptr pkg;
	void *status = status_read(NULL, verbose);

	if (check_dirs_and_files())
		return DIR_NOT_EXIST;

	if (mkdir(APKGCIDIR, S_IRWXU) != 0) {
		ERROR("Creating directory %s failed: %s\n", APKGCIDIR, strerror(errno));
		return DIR_CREATE_FAILED;
	}

	for (pkg = pkgs; pkg != 0; pkg = pkg->next) {
		apkg_unpackcontrol(pkg);
		if (apkg_dounpack(pkg) != SUCCESS) { // skip next package if one of the packages is not unpacked
			if(verbose)
				ERROR("Package %s cannot be unpacked !\n", pkg->package);
			return FAILED;
		}
	}
	status_merge(status, NULL, pkgs);

	if (apkg_rmdir(APKGCIDIR) != 0)
		return DIR_CLEAN_FAILED;

	return SUCCESS;
}

APKG_RET_CODE apkg_configure(package_ptr pkgs) {
	void *found;
	package_ptr pkg;
	APKG_RET_CODE ret;
	void *status = status_read(NULL, verbose);
	for (pkg = pkgs; pkg != 0; pkg = pkg->next) {
		found = tfind(pkg, &status, package_compare);
		if (found == 0) {
			ERROR( "Trying to configure %s, but it is not installed.\n",
					pkg->package);
			ret = CONF_FAILED;
		} else {
			/* configure the package listed in the status file;
			 * not pkg, as we have info only for the latter */
			apkg_doconfigure(*(package_ptr *) found);
		}
	}
	status_merge(status, NULL, NULL);
	return SUCCESS;
}

APKG_RET_CODE apkg_install(char *custom_db, package_ptr pkgs) {
	package_ptr p;
	void *status;

	if (check_dirs_and_files(custom_db))			// create necessary files and directories (also for OFFLINE)
		return DIR_NOT_EXIST;

	apkg_rmdir(APKGCIDIR);				// delete the content of tmp dir
	if (mkdir(APKGCIDIR, S_IRWXU)) { 	// create tmp directories for extracting
		ERROR( "Creating directory %s failed: %s\n", APKGCIDIR, strerror(errno));
		return DIR_CREATE_FAILED;
	}
	if(offline)
		status = status_read(offline_status_file, verbose);	// read offline status file
	else if(user)
		status = status_read(user_status_file, verbose);	// read user status file
	else
		status = status_read(NULL, verbose);	// read initial database

	for (p = pkgs; p != 0; p = p->next) // open control part of each package and fill package structure contents
		if (apkg_unpackcontrol(p) != SUCCESS)
			return FAILED;				// Some packages are failed while unpacking

	for (p = pkgs; p != 0; p = p->next) {
		p->status &= STATUS_WANTMASK;
		p->status |= STATUS_WANTINSTALL;
		p->status &= STATUS_FLAGMASK;
		p->status |= STATUS_FLAGOK;

		//if (verbose)
			PRINT("%-16s%-40s%-20s\n", "INSTALLING" , p->package, p->version);
		apkg_dounpack(p);						// no need to check for return
		if (!offline)					// no configuration for offline, but configure it for user packages
			apkg_doconfigure(p);			// exec scripts (configure)
	}

	if(!nodatabase) {	// will package be inserted to database ?
		if(offline)
			status_merge(status, offline_status_file, pkgs); 	// update offline database
		else if(user)
			status_merge(status, user_status_file, pkgs); 	// update offline database
		else
			status_merge(status, NULL, pkgs); 	// update database
	}

	if (apkg_rmdir(APKGCIDIR) != 0) 	// remove temp directory
		return DIR_CLEAN_FAILED;
	return SUCCESS;
}

APKG_RET_CODE apkg_fields(package_ptr pkg) {
	char command[1024];
	int ret;

	if (pkg == NULL) {
		ERROR( "The -f flag requires an argument.\n");
		return FAILED;
	}
	if (identify_compression_type(pkg) != SUCCESS) // check the compression type of package
		return UNKNOWN_COMP_TYPE;
	if (strcmp(compression_type, "lzo") && strcmp(compression_type, "gz")) {
		ERROR( "Unrecognized compression format: %s\n", compression_type);
		return UNKNOWN_COMP_TYPE;
	}
	snprintf(command, sizeof(command), "ar -p %s data.tar.%s | %s -d | tar -t ",
			pkg->file, compression_type, decompression_tool);
	ret = system(command);
	if(ret)
		return AR_ERROR;
	return SUCCESS;
}

APKG_RET_CODE apkg_files_of_package(char *custom_db, package_ptr pkg) {
	char buffer[256];
	FILE *infp;

	if (check_dirs_and_files(custom_db))			// create necessary files and directories (also for OFFLINE)
		return DIR_NOT_EXIST;

	if(offline)
		snprintf(buffer, sizeof(buffer), "%s/%s.list", offline_info_dir, pkg->package );
	if(user)
		snprintf(buffer, sizeof(buffer), "%s/%s.list", user_info_dir, pkg->package );
	else
		snprintf(buffer, sizeof(buffer), "%s/%s.list", INFODIR, pkg->package );

	if ((infp = fopen(buffer, "r")) == NULL) {
		ERROR( "No package found : %s\n", pkg->package);
		return NO_PACKAGE_FOUND;
	}

	printf("Files in package %s\n---------------------------\n", pkg->package);
	while (fgets(buffer, sizeof(buffer), infp) && !feof(infp)) {
		printf("%s", buffer);
	}

	return SUCCESS;
}

APKG_RET_CODE apkg_remove(char *custom_db, package_ptr pkgs) {

	int ret = SUCCESS;
	package_ptr p, v_pkg, p_tree;
	char buf[1024], buf2[1024];
	FILE *fp;
	void *status;

	if (check_dirs_and_files(custom_db))			// create necessary files and directories (also for OFFLINE)
		return DIR_NOT_EXIST;

	if(offline)
		status = status_read(offline_status_file, verbose);	// read offline status file
	else if(user)
		status = status_read(user_status_file, verbose);	// read user status file
	else
		status = status_read(NULL, verbose);	// read initial database

	for (p = pkgs; p != 0; p = p->next) {

		v_pkg = tfind((void *) p, &status, package_compare);
		if (v_pkg == NULL) {
			ERROR( "No package found with the name of %s\n", p->package);
			p->status |= STATUS_WANTUNKNOWN; // this package is not installed anymore, it is not known
			continue;
		} else { // package was found on tree, means it is worked before

			p_tree = *(package_ptr *) v_pkg;

			if (p_tree->status & STATUS_STATUSNOTINSTALLED) {
				WARNING("Package %s is removed already.\n",
						p_tree->package);
				p->status = p_tree->status; // protect the packages already removed
				continue;
			} else {
				if (offline)
					snprintf(buf, sizeof(buf), "%s%s.list", offline_info_dir, p->package);
				else if (user)
					snprintf(buf, sizeof(buf), "%s%s.list", user_info_dir, p->package);
				else
					snprintf(buf, sizeof(buf), "%s%s.list", INFODIR, p->package);

//				snprintf(buf, sizeof(buf), "%s%s.list", INFODIR, p->package);
				if ((fp = fopen(buf, "r")) == NULL) {
					ERROR( "Cannot read file %s%s.list \n", INFODIR, p->package);
					ret = FILE_READ_FAILED;
				}
				else {
					// run prerm script
					if(offline)
						snprintf(buf, sizeof(buf), "%s%s.prerm", offline_info_dir, p->package);
					else if (user)
						snprintf(buf, sizeof(buf), "%s%s.prerm", user_info_dir, p->package);
					else
						snprintf(buf, sizeof(buf), "%s%s.prerm", INFODIR, p->package);
//					snprintf(buf, sizeof(buf), "%s%s.prerm", INFODIR, p->package);
					if (is_file(buf)) {
						snprintf(buf2, sizeof(buf2), "exec %s configure", buf);
						if ((ret = system(buf2)) != 0) {
							ERROR( "%s's prerm exited with status %d\n",
									p->package, ret);
						}
					}
					// delete files of package
					while (fgets(buf, sizeof(buf), fp) && !feof(fp)) {
						// remove only files, not directories
						buf[strlen(buf) - 1] = 0;
						if (is_file(buf)) {
							snprintf(buf2, sizeof(buf2), "rm -f -- %s", buf);
							if (system(buf2) != 0)
								ret = FILE_REMOVE_FAILED;
							else if (verbose)
								PRINT("%s file deleted\n", buf);
						}
					}
					// run postrm script
					if(offline)
						snprintf(buf, sizeof(buf), "%s%s.postrm", offline_info_dir, p->package);
					else if(user)
						snprintf(buf, sizeof(buf), "%s%s.postrm", user_info_dir, p->package);
					else
						snprintf(buf, sizeof(buf), "%s%s.postrm", INFODIR, p->package);
//					snprintf(buf, sizeof(buf), "%s%s.postrm", INFODIR, p->package);
					if (is_file(buf)) {
						snprintf(buf2, sizeof(buf2), "exec %s configure", buf);
						if ((ret = system(buf2)) != 0) {
							ERROR( "%s's postrm exited with status %d\n",
									p->package, ret);
						}
					}
				}

				if (ret == SUCCESS) { // Dosya sistemi temizlendi /info dizinini de temizle
					if(offline)
						snprintf(buf, sizeof(buf), "rm -f -- %s%s.*", offline_info_dir, p->package);
					else if(user)
						snprintf(buf, sizeof(buf), "rm -f -- %s%s.*", user_info_dir, p->package);
					else
						snprintf(buf, sizeof(buf), "rm -f -- %s%s.*", INFODIR, p->package);
//					snprintf(buf, sizeof(buf), "rm -f -- %s%s.*", INFODIR, p->package);
					if (system(buf) != 0)
						ret = FILE_REMOVE_FAILED;
				}
				p->status &= STATUS_WANTMASK;
				p->status |= STATUS_WANTDEINSTALL;
				p->status &= STATUS_FLAGMASK;
				p->status |= STATUS_FLAGOK;
				p->status &= STATUS_STATUSMASK;
				if (ret == SUCCESS) // paketin silinemeyen dosyası sistemde kaldı mı ?
					p->status |= STATUS_STATUSNOTINSTALLED;
				else
					p->status |= STATUS_STATUSHALFINSTALLED;
			}
		}
	}

	if (offline)
		return status_merge(status, offline_status_file, pkgs); 	// update offline database
	else if (user)
		return status_merge(status, user_status_file, pkgs); 	// update user database
	else
		return status_merge(status, NULL, pkgs); 	// update database
}

APKG_RET_CODE apkg_print_packages(char *custom_db) {
	packet_count = 0;

	if (check_dirs_and_files(custom_db))			// create necessary files and directories (also for OFFLINE)
		return DIR_NOT_EXIST;
	if (user) {
		status = status_read(user_status_file, verbose);
		PRINT("Database file(User database): %s\n", user_status_file);
	}
	else
		status = status_read(NULL, verbose);

	PRINT("%-8s%-24s%-44s\n", "State", "Package Name", "Version Info");
	PRINT("%s-%s-%s\n", "=======", "=======================", "====================================");
	twalk(status, travel_package_tree);
	PRINT("%s-%s-%s\n", "=======", "=======================", "====================================");
	PRINT("Total Number of Installed Packages: %d\n\n", packet_count );
	return SUCCESS;
}

APKG_RET_CODE apkg_status_of(char *custom_db, package_ptr pkg) {

	void *status;

	if (check_dirs_and_files(custom_db))			// create necessary files and directories (also for OFFLINE)
		return DIR_NOT_EXIST;
	if (offline)
		status = status_read(offline_status_file, verbose);
	else if (user)
		status = status_read(user_status_file, verbose);
	else
		status = status_read(NULL, verbose);

	package_ptr p;
	void *v_pkg;

	v_pkg = tfind((void *) pkg, &status, package_compare);
	if (v_pkg == NULL)
		return FAILED;
	else {
		p = *(package_ptr *) v_pkg;
		printf("Status of %s package : %s \n", p->package,
				status_print(p->status));
	}
	return SUCCESS;
}

APKG_RET_CODE apkg_install_all_unpacked() {

	package_ptr p, pack;
	char *dependency_name;
	void *v_pkg;
	void *status = status_read(NULL, verbose);

	pack = (package_ptr)malloc(sizeof(struct package_t));

	apkg_get_dependency_list_of_unpacked_packages();
	while ((dependency_name = apkg_get_next_dependency()) && dependency_name != NULL)
	{
		strcpy(pack->package, dependency_name);
		v_pkg = tfind((void *) pack, &status, package_compare);

		if (v_pkg == NULL)
			return FAILED;
		else {
			p = *(package_ptr *) v_pkg;
			if (p->status & STATUS_STATUSUNPACKED) {
				if(verbose)
					PRINT("Configuring package: %s\n", p->package);
				apkg_doconfigure(p);
			}
		}
	}

	return status_merge(status, NULL, NULL);
}

APKG_RET_CODE apkg_print_dep_tree(char *cached_directory, char *package) {

	// it is working for only cached directory
	char *dependency_name;
	int ret = 0;

	if (cached_directory) {
		if(verbose)
			PRINT("Cached directory: %s\n", cached_directory );
		ret = apkg_init(cached_directory);
	}
	else {
		ERROR("Cached directory is not entered !\n Use 'apkg --cached=<path/to/dir> -d %s'\n", package);
		return FAILED;
	}
	if(ret != SUCCESS)		// init failed
		return FAILED;
	apkg_get_dependency_list_of_package(package);
//	apkg_get_dependency_list_of_directory();

	PRINT( "---------------------------\n");
	PRINT("  Install order of %s in %s\n", package, cached_directory);
	PRINT( "---------------------------\n");
	while ((dependency_name = apkg_get_next_dependency()) && dependency_name) {
		PRINT("\t%s\n", dependency_name);
	}
	PRINT( "---------------------------\n");
	//apkg_release_dependency_list();

	return SUCCESS;
}

APKG_RET_CODE apkg_print_dep_tree_of_dir(char *cached_directory) {

	// it is working for only cached directory
	char *dependency_name;
	int ret = 0;

	if (cached_directory) {
		if(verbose)
			PRINT("Cached directory: %s\n", cached_directory );
		ret = apkg_init(cached_directory);
	}
	else {
		ERROR("Cached directory is not entered !\n Use 'apkg --cached=<path/to/dir>'\n");
		return FAILED;
	}
	if(ret != SUCCESS)		// init failed
		return FAILED;
//	apkg_get_dependency_list_of_package(package);
	apkg_get_dependency_list_of_directory();

	PRINT( "---------------------------\n");
	PRINT("  Install order in %s\n",cached_directory);
	PRINT( "---------------------------\n");
	while ((dependency_name = apkg_get_next_dependency()) && dependency_name) {
		PRINT("\t%s\n", dependency_name);
	}
	PRINT( "---------------------------\n");
	apkg_release_dependency_list();

	return SUCCESS;
}
