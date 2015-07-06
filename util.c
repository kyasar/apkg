/*
 * util.c
 *
 *  Created on: Feb 7, 2012
 *      Author: root
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <utime.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/sendfile.h>

#include "libapkg_defs.h"
#include "util.h"

char *compression_type;
char *decompression_tool;
char *offline_root;
int verbose;
int offline;
int user;
int nodatabase;

extern char *offline_apkg_dir;
extern char *offline_status_file;
extern char *offline_info_dir;
extern char *offline_etc_dir;

extern char *user_apkg_dir;
extern char *user_status_file;
extern char *user_info_dir;

void apkg_settings(int v, int f, int nodb, char *off_root) {
	verbose = v;
	offline = f;
	offline_root = off_root;
	nodatabase = nodb;
}

int is_file(char *fn) {
	struct stat statbuf;

	if (stat(fn, &statbuf) < 0)
		return 0;

	return S_ISREG(statbuf.st_mode);
}

int is_dir(char *fn) {
	struct stat statbuf;

	if (stat(fn, &statbuf) < 0)
		return 0;

	return S_ISDIR(statbuf.st_mode);
}

int check_dirs_and_files(char *custom_db) {

	FILE* fd = 0;
	char buffer [256];

	// If user mode is activated, use custom database
	if(custom_db) {
		user = 1;	// for library use, set user mode when custom db is given
		snprintf(buffer, sizeof(buffer), "%s.%s/", ADMINDIR, custom_db);
		user_apkg_dir = strdup(buffer);

		snprintf(buffer, sizeof(buffer), "%s/info/", user_apkg_dir);
		user_info_dir = strdup(buffer);

		snprintf(buffer, sizeof(buffer), "%s/status", user_apkg_dir);
		user_status_file = strdup(buffer);

		if (!is_dir(user_apkg_dir) ) {
			if (verbose)
				WARNING("%s directory not found.. It will be created.\n",
						user_apkg_dir);
			if (mkdir(user_apkg_dir, S_IRWXU)) {
				ERROR( "Creating directory %s failed.\n", user_apkg_dir);
				return 1;
			}
		}
		if (!is_dir(user_info_dir)) {
			if (verbose)
				WARNING("%s directory not found.. It will be created.\n", user_info_dir);
			if (mkdir(user_info_dir, S_IRWXU)) {
				ERROR("Creating directory %s failed.\n", user_info_dir);
				return 1;
			}
		}
		if (!is_file(user_status_file)) {
			if (verbose)
				WARNING("%s file not found.. It will be created.\n", user_status_file);
			if ((fd = fopen(user_status_file, "a+")) < 0) {
				ERROR("Creating status file %s failed.\n", user_status_file);
				fclose(fd);
				return 1;
			}
		}
	}

	// OFFLINE ROOT DIRECTORIES AND FILES
	else if(offline) {
		snprintf(buffer, sizeof(buffer), "%s/%s", offline_root, "/etc" );
		offline_etc_dir = strdup(buffer);

		snprintf(buffer, sizeof(buffer), "%s/%s", offline_root, ADMINDIR );
		offline_apkg_dir = strdup(buffer);

		snprintf(buffer, sizeof(buffer), "%s/%s", offline_root, INFODIR );
		offline_info_dir = strdup(buffer);

		snprintf(buffer, sizeof(buffer), "%s/%s", offline_root, STATUSFILE );
		offline_status_file = strdup(buffer);

		if (!is_dir(offline_etc_dir) ) {
			if (verbose)
				WARNING("%s directory not found.. It will be created.\n",
						offline_etc_dir);
			if (mkdir(offline_etc_dir, S_IRWXU)) {
				ERROR( "Creating directory %s failed.\n", offline_etc_dir);
				return 1;
			}
		}

		if (!is_dir(offline_apkg_dir) ) {
			if (verbose)
				WARNING("%s directory not found.. It will be created.\n",
						offline_apkg_dir);
			if (mkdir(offline_apkg_dir, S_IRWXU)) {
				ERROR( "Creating directory %s failed.\n", offline_apkg_dir);
				return 1;
			}
		}
		if (!is_dir(offline_info_dir)) {
			if (verbose)
				WARNING("%s directory not found.. It will be created.\n", offline_info_dir);
			if (mkdir(offline_info_dir, S_IRWXU)) {
				ERROR("Creating directory %s failed.\n", offline_info_dir);
				return 1;
			}
		}
		if (!is_file(offline_status_file)) {
			if (verbose)
				WARNING("%s file not found.. It will be created.\n", offline_status_file);
			if ((fd = fopen(offline_status_file, "a+")) < 0) {
				ERROR("Creating status file %s failed.\n", offline_status_file);
				fclose(fd);
				return 1;
			}
		}
	}
	// NO OFFLINE
	else {
		if (!is_dir((char*) ADMINDIR)) {
			if (verbose)
				WARNING("%s directory not found.. It will be created.\n",
						ADMINDIR);
			if (mkdir(ADMINDIR, S_IRWXU)) {
				ERROR( "Creating directory %s failed.\n", ADMINDIR);
				return 1;
			}
		}
		if (!is_dir((char*) INFODIR)) {
			if (verbose)
				WARNING("%s directory not found.. It will be created.\n", INFODIR);
			if (mkdir(INFODIR, S_IRWXU)) {
				ERROR("Creating directory %s failed.\n", INFODIR);
				return 1;
			}
		}
		if (!is_file((char*) STATUSFILE)) {
			if (verbose)
				WARNING("%s file not found.. It will be created.\n", STATUSFILE);
			if ((fd = fopen(STATUSFILE, "a+")) < 0) {
				ERROR("Creating status file %s failed.\n", STATUSFILE);
				fclose(fd);
				return 1;
			}
		}
	}

	// /tmp is working directory so it must be created for everyone
	if (!is_dir((char*) TMPAPKGDIR)) {
		if (verbose)
			WARNING("%s directory not found.. It will be created.\n",
					TMPAPKGDIR);
		if (mkdir(TMPAPKGDIR, S_IRWXU)) {
			ERROR("Creating directory %s failed.\n", TMPAPKGDIR);
			return 1;
		}
	}

	return 0;
}

int apkg_rmdir(char *path) {
	DIR *d = opendir(path);
	size_t path_len = strlen(path);
	int r = -1;

	if (d) {
		struct dirent *p;
		r = 0;
		while (!r && (p = readdir(d))) {
			int r2 = -1;
			char *buf;
			size_t len;

			/* Skip the names "." and ".." as we don't want to recurse on them. */
			if (!strcmp(p->d_name, ".") || !strcmp(p->d_name, ".."))
				continue;

			len = path_len + strlen(p->d_name) + 2;
			buf = malloc(len);

			if (buf) {
				struct stat statbuf;
				snprintf(buf, len, "%s/%s", path, p->d_name);
				if (!stat(buf, &statbuf)) {
					if (S_ISDIR(statbuf.st_mode))
						r2 = apkg_rmdir(buf);
					else
						r2 = unlink(buf);
				}
				free(buf);
			}
			r = r2;
		}
		closedir(d);
	}
	if (!r)
		r = rmdir(path);
	return r;
}

int apkg_rmdir_content(char *path) {
   DIR *d = opendir(path);
   size_t path_len = strlen(path);
   int r = -1;

   if (d)
   {
      struct dirent *p;
      r = 0;
      while (!r && (p=readdir(d)))
      {
          int r2 = -1;
          char *buf;
          size_t len;

          if (!strcmp(p->d_name, ".") || !strcmp(p->d_name, ".."))
             continue;

          len = path_len + strlen(p->d_name) + 2;
          buf = malloc(len);

          if (buf)
          {
             struct stat statbuf;
             snprintf(buf, len, "%s/%s", path, p->d_name);
             if (!stat(buf, &statbuf)) {
                if (S_ISDIR(statbuf.st_mode))
                   r2 = apkg_rmdir(buf);
                else {
                   r2 = unlink(buf);
                }
             }
             free(buf);
          }
          r = r2;
      }
      closedir(d);
   }
   return r;
}

int apkg_copyfile(const char *src, const char *dest) {

	char buf[8192];
	int infd, outfd;
	int r;
	struct stat srcStat;
	struct utimbuf times;

	if (stat(src, &srcStat) < 0) {
		if (errno == ENOENT)
			return 0;
		else
			return -1;
	}
	if ((infd = open(src, O_RDONLY)) < 0)
		return -1;
	if ((outfd = open(dest, O_WRONLY | O_CREAT | O_TRUNC, srcStat.st_mode)) < 0)
		return -1;
	while ((r = read(infd, buf, sizeof(buf))) > 0) {
		if (write(outfd, buf, r) < 0)
			return -1;
	}
	close(outfd);
	close(infd);
	if (r < 0)
		return -1;
	times.actime = srcStat.st_atime;
	times.modtime = srcStat.st_mtime;
	if (utime(dest, &times) < 0)
		return -1;
	return 1;
}

APKG_RET_CODE identify_compression_type(package_ptr pkg) {

	FILE *infp = NULL;
	char buf[1024];

	snprintf(buf, sizeof(buf), "ar -t %s 2>/dev/null", pkg->file);
	if ((infp = popen(buf, "r")) == NULL) {
		ERROR("Cannot retrieve archive members of %s: %s\n",
				pkg->file, strerror(errno));
		return FAILED;
	}

	while (fgets(buf, sizeof(buf), infp)) {
		if (strncmp(buf, "data.tar.", 9) == 0) {
			compression_type = buf + 9;
			break;
		}
	}
	pclose(infp);

	if (compression_type == NULL) {
		ERROR("No data member found in %s\n", pkg->file);
		return FAILED;
	}
	//GREEN("comp. type of %s : %s", pkg->package, compression_type);
	if (strncmp(compression_type, LZOP_COMP_TYPE, 3) == 0) {
		compression_type = LZOP_COMP_TYPE;
		decompression_tool = LZOP_COMP_TOOL;
	} else if (strncmp(compression_type, GZ_COMP_TYPE, 2) == 0) {
		compression_type = GZ_COMP_TYPE;
		decompression_tool = GZ_COMP_TOOL;
	} else {
		ERROR( "Invalid compression type for data member of %s\n",
				pkg->file);
		return UNKNOWN_COMP_TYPE;
	}

	return SUCCESS;
}

int apkg_mv(char *source, char *destination) {

	int src, dest;
	struct stat st;
	off_t offset = 0;

	if( (src = open(source, O_RDONLY)) < 0 )
		return -1;
	fstat(src, &st);
	if( (dest = open(destination, O_WRONLY|O_CREAT, st.st_mode)) < 0 )     // if necessary, create new file with same mode
		return -2;

	sendfile(dest, src, &offset, st.st_size);
	close(dest);	// close descriptors
	close(src);

	return unlink(source);	// delete source
}

int apkg_cp(char *source, char *destination) {

	int src, dest;
	struct stat st;
	off_t offset = 0;

	if( (src = open(source, O_RDONLY)) < 0 )
		return -1;
	fstat(src, &st);
	if( (dest = open(destination, O_WRONLY|O_CREAT, st.st_mode)) < 0 )     // if necessary, create new file with same mode
		return -2;

	sendfile(dest, src, &offset, st.st_size);
	close(dest);	// close descriptors
	close(src);

	return 0;
}
