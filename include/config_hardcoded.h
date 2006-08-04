
/* Define to the type of arg 1 for `readlink'. */
#define READLINK_TYPE_ARG1 const char *path

/* Define to the type of arg 2 for `readlink'. */
#define READLINK_TYPE_ARG2 char *buf

/* Define to the type of arg 3 for `readlink'. */
#define READLINK_TYPE_ARG3 size_t bufsiz

/* Define to the type of arg 1 for `scandir'. */
#define SCANDIR_TYPE_ARG1 const char *dir

/* Define to the type of arg 2 for `scandir'. */
#define SCANDIR_TYPE_ARG2 struct dirent ***namelist

/* Define to the type of arg 3 for `scandir'. */
#define SCANDIR_TYPE_ARG3 int(*filter)(const struct dirent *)

/* Define to the type of arg 4 for `scandir'. */
#define SCANDIR_TYPE_ARG4 int(*compar)(const void *,const void *)

