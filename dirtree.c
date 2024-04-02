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

  DIR *dir = opendir(dn);
  if (!dir) {
    panic("Failed to open directory.");
  }

  struct dirent *entry;
  struct dirent entrylist[MAX_DIR];
  int count = 0;

  while ((entry = getNext(dir)) != NULL && count < MAX_DIR) {
    entrylist[count] = *entry;
    count += 1;
  }
  closedir(dir);

  qsort(entrylist, count, sizeof(struct dirent), dirent_compare);

  for (int i=0; i<count; i++) {
    // define path and name
    char *name;
    int name_len = asprintf(&name, "%*s%s", depth*2, "", entrylist[i].d_name);
    if (name_len == -1) {
      panic("Failed to write path & name.");
    }

    char name_limited[NAME_WID+1];
    if (name_len > NAME_WID) {
      strncpy(name_limited, name, NAME_WID-3);
      name_limited[NAME_WID - 3] = '\0';
      strcat(name_limited, "...");
    } else {
      strncpy(name_limited, name, name_len);
      name_limited[name_len] = '\0';
    }
    free(name);

    // define user & group
    struct stat info;
    struct passwd *user_info = getpwuid(info.st_uid);
    struct group *group_info = getgrgid(info.st_gid);
    if (user_info == NULL || group_info == NULL) {
      panic("Failed to get file information.");
    }
    
    char *user;
    char *group;
    int user_len = asprintf(&user, "%s", user_info->pw_name);
    int group_len = asprintf(&group, "%s", group_info->gr_name);
    if (user_len == -1) {
      panic("Failed to write user.");
    }
    if (group_len == -1) {
      panic("Failed to write group.");
    }

    char user_limited[USER_WID+1];
    char group_limited[GROUP_WID+1];
    int user_overflow = (user_len > USER_WID) ? USER_WID : user_len;
    int group_overflow = (group_len > GROUP_WID) ? GROUP_WID : group_len;
    strncpy(user_limited, user, user_overflow);
    user_limited[user_overflow] = '\0';
    strncpy(group_limited, group, group_overflow);
    group_limited[group_overflow] = '\0';
    free(user);
    free(group);

    if (entrylist[i].d_type == DT_DIR) {
      stats->dirs += 1;

      // print path and name
      printf("%-*s", NAME_WID, name_limited);

      if (flags & F_SUMMARY) {  
        // print user & group
        printf("  ");
        printf("%*s:%-*s", USER_WID, user_limited, GROUP_WID, group_limited);

      }

      printf("\n");
      
      // call recursively
      char fullPath[1024];
      snprintf(fullPath, sizeof(fullPath), "%s/%s", dn, entrylist[i].d_name);
      processDir(fullPath, depth+1, stats, flags);

    } else {

      if      (entrylist[i].d_type == DT_REG) { stats->files += 1; }
      else if (entrylist[i].d_type == DT_LNK) { stats->links += 1; }
      else if (entrylist[i].d_type == DT_FIFO) { stats->fifos += 1; }
      else if (entrylist[i].d_type == DT_SOCK) { stats->socks += 1; }

      if (flags & F_DIRONLY) {
        continue;
      }

      // print path and name
      printf("%-*s", NAME_WID, name_limited);

      if (flags & F_SUMMARY) {  
        // print user & group
        printf("  ");
        printf("%*s:%-*s", USER_WID, user_limited, GROUP_WID, group_limited);

      }

      printf("\n");
    }
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
      printf("Name                                                        User:Group           Size     Perms Type\n");
      printf("----------------------------------------------------------------------------------------------------\n");
    }

    printf("%s\n", directories[i]);
    processDir(directories[i], 1, &dstat, flags);

    if (flags & F_SUMMARY) {
      printf("----------------------------------------------------------------------------------------------------\n");
      if (flags & F_DIRONLY) {
        printf("%d director%s\n", dstat.dirs, (dstat.dirs != 1) ? "ies" : "y");
      } else {
        int total_size = 0; // should be changed
        char *summary_line;
        int summary_line_len = asprintf(&summary_line,
          "%d file%s, %d director%s, %d link%s, %d pipe%s, and %d socket%s\n",
          dstat.files, (dstat.files != 1) ? "s" : "",
          dstat.dirs, (dstat.dirs != 1) ? "ies" : "y",
          dstat.links, (dstat.links != 1) ? "s" : "",
          dstat.fifos, (dstat.fifos != 1) ? "s" : "",
          dstat.socks, (dstat.socks != 1) ? "s" : "");
        if (summary_line_len == -1) {
          panic("Failed to print summary line.");
        }
        printf("%-68s%14d", summary_line, total_size);
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
