#ifndef _SHFSMOUNT_H_
#define _SHFSMOUNT_H_

#define BUFFER_MAX	1024

#define MOUNT_VERBOSE 1
#define MOUNT_DEBUG 3

void error(char *msg, ...);

extern int verbose;
#define VERBOSE(x...) do { if (verbose >= MOUNT_VERBOSE) fprintf(stderr, x); } while (0)

extern int verbose;
#define DEBUG(x...) do { if (verbose >= MOUNT_DEBUG) fprintf(stderr, x); } while (0)

#endif
