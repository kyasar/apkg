
int package_compare(const void *, const void *);
void control_read(FILE *, package_ptr);
char *status_print(unsigned long);
unsigned long status_parse(const char *);
void *status_read(char *, int);
APKG_RET_CODE status_merge(void *, char *, package_ptr);
