//--------------------------------------------------------------------------------------------------
// System Programming                         I/O Lab                                   Spring 2024
//
/// @file
/// @brief resursively traverse directory tree and list all entries
/// @author Sunwoo Song
/// @studid 2021-15524
//--------------------------------------------------------------------------------------------------

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <errno.h>
#include <unistd.h>
#include <stdarg.h>
#include <assert.h>
#include <grp.h>
#include <pwd.h>

#define MAX_DIR 64            ///< maximum number of supported directories
#define NAME_WID 54
#define USER_WID 8
#define GROUP_WID 8
#define FILSZ_WID 10
#define PERM_WID 9
#define SUMLN_WID 68
#define TOTSZ_WID 14

/// @brief output control flags
#define F_DIRONLY   0x1       ///< turn on direcetory only option
#define F_SUMMARY   0x2       ///< enable summary
#define F_VERBOSE   0x4       ///< turn on verbose mode

/// @brief struct holding the summary
struct summary {
  unsigned int dirs;          ///< number of directories encountered
  unsigned int files;         ///< number of files
  unsigned int links;         ///< number of links
  unsigned int fifos;         ///< number of pipes
  unsigned int socks;         ///< number of sockets

  unsigned long long size;    ///< total size (in bytes)
};


/// @brief abort the program with EXIT_FAILURE and an optional error message
///
/// @param msg optional error message or NULL
void panic(const char *msg)
{
  if (msg) fprintf(stderr, "%s\n", msg);
  exit(EXIT_FAILURE);
}


/// @brief read next directory entry from open directory 'dir'. Ignores '.' and '..' entries
///
/// @param dir open DIR* stream
/// @retval entry on success
/// @retval NULL on error or if there are no more entries
struct dirent *getNext(DIR *dir)
{
  struct dirent *next;
  int ignore;

  do {
    errno = 0;
    next = readdir(dir);
    if (errno != 0) perror(NULL);
    ignore = next && ((strcmp(next->d_name, ".") == 0) || (strcmp(next->d_name, "..") == 0));
  } while (next && ignore);

  return next;
}


/// @brief qsort comparator to sort directory entries. Sorted by name, directories first.
///
/// @param a pointer to first entry
/// @param b pointer to second entry
/// @retval -1 if a<b
/// @retval 0  if a==b
/// @retval 1  if a>b
static int dirent_compare(const void *a, const void *b)
{
  struct dirent *e1 = (struct dirent*)a;
  struct dirent *e2 = (struct dirent*)b;

  // if one of the entries is a directory, it comes first
  if (e1->d_type != e2->d_type) {
    if (e1->d_type == DT_DIR) return -1;
    if (e2->d_type == DT_DIR) return 1;
  }

  // otherwise sorty by name
  return strcmp(e1->d_name, e2->d_name);
}


/// @brief recursively process directory @a dn and print its tree
///
/// @param dn absolute or relative path string
/// @param depth depth in directory tree
/// @param stats pointer to statistics
/// @param flags output control flags (F_*)

void processDir(const char *dn, unsigned int depth, struct summary *stats, unsigned int flags)
{
  // TODO
  // open, enumerate, sort, close directory
  // print elements
  // update statistics
  // if element is directory => call recursively

  // ***OPEN***
  DIR *dir = opendir(dn);
  if (!dir) {
    panic("Failed to open directory.");
  }

  // ***ENUMERATE***
  struct dirent *entry;
  struct dirent entrylist[MAX_DIR];
  int count = 0;

  while ((entry = getNext(dir)) != NULL && count < MAX_DIR) {
    entrylist[count] = *entry;
    count += 1;
  }

  // ***SORT***
  qsort(entrylist, count, sizeof(struct dirent), dirent_compare);

  // ***CLOSE***
  closedir(dir);

  for (int i=0; i<count; i++) {
    struct stat info;

    char *full_path;
    int full_path_len = asprintf(&full_path, "%s/%s", dn, entrylist[i].d_name);
    if (full_path_len == -1) {
      panic("Failed to get full path.");
    }

    if (lstat(full_path, &info) != 0) {
        panic("Failed to get file stats.");
    }

    if (flags & F_DIRONLY) { if (entrylist[i].d_type != DT_DIR) { continue; } }

    // ***UPDATE STATISTICS***
    if      (entrylist[i].d_type == DT_DIR) { stats->dirs += 1; }
    else if (entrylist[i].d_type == DT_REG) { stats->files += 1; }
    else if (entrylist[i].d_type == DT_LNK) { stats->links += 1; }
    else if (entrylist[i].d_type == DT_FIFO) { stats->fifos += 1; }
    else if (entrylist[i].d_type == DT_SOCK) { stats->socks += 1; }

    // ***PRINT LINE***
    char line[99];

    // 1. NAME
    char *name;
    int name_len = asprintf(&name, "%*s%s", depth*2, "", entrylist[i].d_name);
    if (name_len == -1) { panic("Failed to write path & name."); }

    if (!(flags & F_VERBOSE)) {
      strncpy(line, name, name_len);
      line[name_len] = '\0';
    }
    
    else {
      // line[0] ~ line[53], width 54
      if (name_len > NAME_WID) {
        strncpy(line, name, NAME_WID-3);
        line[NAME_WID-3] = '\0';
        strncat(line, "...", 3);
      } else {
        strncpy(line, name, name_len);
        line[name_len] = '\0';
        for (int i=0; i<NAME_WID-name_len; i++) {
          strncat(line, " ", 1);
        }
      }
      free(name);

      strncat(line, "  ", 2);
      
      // 2. USER & GROUP
      struct passwd *user_info = getpwuid(info.st_uid);
      struct group *group_info = getgrgid(info.st_gid);
      if (user_info == NULL || group_info == NULL) {
        panic("Failed to get file information.");
      }
      
      char *user;
      int user_len = asprintf(&user, "%s", user_info->pw_name);
      if (user_len == -1) {
        panic("Failed to write user.");
      }

      char *group;
      int group_len = asprintf(&group, "%s", group_info->gr_name);
      if (group_len == -1) {
        panic("Failed to write group.");
      }

      // line[54] ~ line[70], width 8+1+8 = 17
      // (1) user: line[54] ~ line[61], width 8
      if (user_len > USER_WID) {
        strncat(line, user, USER_WID);
      } else {
        for (int i=0; i<USER_WID-user_len; i++) {
          strncat(line, " ", 1);
        }
        strncat(line, user, user_len);
      }
      // (2) ":": line[62], width 1
      strncat(line, ":", 1);

      // (3) group: line[63] ~ line[70], width 8
      if (group_len > GROUP_WID) {
        strncat(line, group, GROUP_WID);
      } else {
        strncat(line, group, group_len);
        for (int i=0; i<GROUP_WID-group_len; i++) {
          strncat(line, " ", 1);
        }
      }
      free(user);
      free(group);

      strncat(line, "  ", 2);

      // 3. SIZE
      unsigned long long size = (unsigned long long)info.st_size;
      stats->size += size;

      char size_str[FILSZ_WID+1];
      int size_str_len = snprintf(size_str, sizeof(size_str), "%llu", size);

      // line[71] ~ line[80], width 10
      if (size_str_len > FILSZ_WID) {
        strncat(line, size_str, FILSZ_WID);
      } else {
        for (int i=0; i<FILSZ_WID-size_str_len; i++) {
          strncat(line, " ", 1);
        }
        strncat(line, size_str, size_str_len);
      }

      strncat(line, " ", 1);

      // 4. PERMISSION
      char perms[10] = "---------";
      if (filestat.st_mode & S_IRUSR) perms[0] = 'r';
      if (filestat.st_mode & S_IWUSR) perms[1] = 'w';
      if (filestat.st_mode & S_IXUSR) perms[2] = 'x';
      if (filestat.st_mode & S_IRGRP) perms[3] = 'r';
      if (filestat.st_mode & S_IWGRP) perms[4] = 'w';
      if (filestat.st_mode & S_IXGRP) perms[5] = 'x';
      if (filestat.st_mode & S_IROTH) perms[6] = 'r';
      if (filestat.st_mode & S_IWOTH) perms[7] = 'w';
      if (filestat.st_mode & S_IXOTH) perms[8] = 'x';
      strncat(line, perms, PERM_WID);

      strncat(line, "  ", 1);

      // 5. TYPE
      char type = '?';
      if (S_ISDIR(filestat.st_mode)) type = 'd';
      else if (S_ISREG(filestat.st_mode)) type = 'f';
      else if (S_ISLNK(filestat.st_mode)) type = 'l';
      else if (S_ISSOCK(filestat.st_mode)) type = 's';
      else if (S_ISFIFO(filestat.st_mode)) type = 'p';
      strncat(line, type, 1);

    }

    // final. PRINT
    printf("%s\n", line);

    // ***RECURSIVE CALL***
    if (entrylist[i].d_type == DT_DIR) {
      processDir(full_path, depth+1, stats, flags);
    }
    free(full_path);

  }
  
}


/// @brief print program syntax and an optional error message. Aborts the program with EXIT_FAILURE
///
/// @param argv0 command line argument 0 (executable)
/// @param error optional error (format) string (printf format) or NULL
/// @param ... parameter to the error format string
void syntax(const char *argv0, const char *error, ...)
{
  if (error) {
    va_list ap;

    va_start(ap, error);
    vfprintf(stderr, error, ap);
    va_end(ap);

    printf("\n\n");
  }

  assert(argv0 != NULL);

  fprintf(stderr, "Usage %s [-d] [-s] [-v] [-h] [path...]\n"
                  "Gather information about directory trees. If no path is given, the current directory\n"
                  "is analyzed.\n"
                  "\n"
                  "Options:\n"
                  " -d        print directories only\n"
                  " -s        print summary of directories (total number of files, total file size, etc)\n"
                  " -v        print detailed information for each file. Turns on tree view.\n"
                  " -h        print this help\n"
                  " path...   list of space-separated paths (max %d). Default is the current directory.\n",
                  basename(argv0), MAX_DIR);

  exit(EXIT_FAILURE);
}


/// @brief program entry point
int main(int argc, char *argv[])
{
  //
  // default directory is the current directory (".")
  //
  const char CURDIR[] = ".";
  const char *directories[MAX_DIR];
  int   ndir = 0;

  struct summary tstat;
  unsigned int flags = 0;

  //
  // parse arguments
  //
  for (int i = 1; i < argc; i++) {
    if (argv[i][0] == '-') {
      // format: "-<flag>"
      if      (!strcmp(argv[i], "-d")) flags |= F_DIRONLY;
      else if (!strcmp(argv[i], "-s")) flags |= F_SUMMARY;
      else if (!strcmp(argv[i], "-v")) flags |= F_VERBOSE;
      else if (!strcmp(argv[i], "-h")) syntax(argv[0], NULL);
      else syntax(argv[0], "Unrecognized option '%s'.", argv[i]);
    } else {
      // anything else is recognized as a directory
      if (ndir < MAX_DIR) {
        directories[ndir++] = argv[i];
      } else {
        printf("Warning: maximum number of directories exceeded, ignoring '%s'.\n", argv[i]);
      }
    }
  }

  // if no directory was specified, use the current directory
  if (ndir == 0) directories[ndir++] = CURDIR;


  //
  // process each directory
  //
  // TODO
  //
  // Pseudo-code
  // - reset statistics (tstat)
  // - loop over all entries in 'directories' (number of entires stored in 'ndir')
  //   - reset statistics (dstat)
  //   - if F_SUMMARY flag set: print header
  //   - print directory name
  //   - call processDir() for the directory
  //   - if F_SUMMARY flag set: print summary & update statistics
  memset(&tstat, 0, sizeof(tstat));
  //...

  for (int i = 0; i < ndir; i++) {
    struct summary dstat;
    memset(&dstat, 0, sizeof(dstat));

    if (flags & F_SUMMARY) {
      if (flags & F_VERBOSE) {
        printf("Name                                                        User:Group           Size     Perms Type\n");
      } else {
        printf("Name\n");
      }

      printf("----------------------------------------------------------------------------------------------------\n");
    }

    printf("%s\n", directories[i]);
    processDir(directories[i], 1, &dstat, flags);

    if (flags & F_SUMMARY) {
      printf("----------------------------------------------------------------------------------------------------\n");
      if (flags & F_DIRONLY) {
        char *summary_line;
        int summary_line_len = asprintf(&summary_line, "%d director%s", dstat.dirs, (dstat.dirs != 1) ? "ies" : "y");
        if (summary_line_len == -1) {
          panic("Failed to print summary line.");
        }

        if (!(flags & F_VERBOSE)) {
          printf("%s", summary_line);
        } else {
          char summary_line_limited[SUMLN_WID+1];
          int summary_line_overflow = (summary_line_len > SUMLN_WID) ? SUMLN_WID : summary_line_len;
          strncpy(summary_line_limited, summary_line, summary_line_overflow);
          summary_line_limited[summary_line_overflow] = '\0';
          printf("%-*s", SUMLN_WID, summary_line);

        }
        free(summary_line);

      } else {
        char *summary_line;
        int summary_line_len = asprintf(&summary_line,
          "%d file%s, %d director%s, %d link%s, %d pipe%s, and %d socket%s",
          dstat.files, (dstat.files != 1) ? "s" : "",
          dstat.dirs, (dstat.dirs != 1) ? "ies" : "y",
          dstat.links, (dstat.links != 1) ? "s" : "",
          dstat.fifos, (dstat.fifos != 1) ? "s" : "",
          dstat.socks, (dstat.socks != 1) ? "s" : "");
        if (summary_line_len == -1) {
          panic("Failed to print summary line.");
        }

        if (!(flags & F_VERBOSE)) {
          printf("%s", summary_line);
        } else {
          char summary_line_limited[SUMLN_WID+1];
          int summary_line_overflow = (summary_line_len > SUMLN_WID) ? SUMLN_WID : summary_line_len;
          strncpy(summary_line_limited, summary_line, summary_line_overflow);
          summary_line_limited[summary_line_overflow] = '\0';
          printf("%-*s", SUMLN_WID, summary_line);

          printf("   ");
          printf("%*lld", TOTSZ_WID, dstat.size);

        }
        free(summary_line);
        
      }
      printf("\n");
    }

    tstat.dirs += dstat.dirs;
    tstat.fifos += dstat.fifos;
    tstat.files += dstat.files;
    tstat.links += dstat.links;
    tstat.size += dstat.size;
    tstat.socks += dstat.socks;

  }

  //
  // print grand total
  //
  if ((flags & F_SUMMARY) && (ndir > 1)) {
    printf("Analyzed %d directories:\n"    
           "  total # of files:        %16d\n"
           "  total # of directories:  %16d\n"
           "  total # of links:        %16d\n"
           "  total # of pipes:        %16d\n"
           "  total # of sockets:      %16d\n",
           ndir, tstat.files, tstat.dirs, tstat.links, tstat.fifos, tstat.socks);

    if (flags & F_VERBOSE) {
      printf("  total file size:         %16llu\n", tstat.size);
    }

  }

  //
  // that's all, folks!
  //
  return EXIT_SUCCESS;
}
