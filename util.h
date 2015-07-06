
#define GZ_COMP_TYPE		"gz"
#define LZOP_COMP_TYPE		"lzo"
#define GZ_COMP_TOOL		"gzip"
#define LZOP_COMP_TOOL		"lzop"

void apkg_settings(int v, int f, int nodb, char *off_root);
int is_file(char *);
int is_dir(char *);
int check_dirs_and_files();
int apkg_rmdir(char *);
int apkg_rmdir_content(char *);
int apkg_copyfile(const char *, const char *);
int apkg_mv(char *source, char *destination);
int apkg_cp(char *source, char *destination);
APKG_RET_CODE identify_compression_type(package_ptr);
