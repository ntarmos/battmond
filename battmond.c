/*
 * Battmond: A battery monitoring daemon.
 *
 * Author: Nikos Ntarmos <ntarmos@ceid.upatras.gr>
 * URL: http://ntarmos.dyndns.org/
 *
 * $FreeBSD$
 * $Id: battmond.c 54 2006-09-08 13:12:05Z ntarmos $
 */

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <grp.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <syslog.h>
#include <sys/ioctl.h>
#include <sys/param.h>
#include <unistd.h>
#include <libutil.h>

#include <dev/acpica/acpiio.h>

#define ACPIDEV		"/dev/acpi"
char * acpidev = NULL;

#define BATT_WARN "Your battery power is running low. Please connect the \
power cord or save any unsaved work and halt the system.\n"
#define BATT_HALT "Your battery power is in critical level. \
Your system will now halt to preserve any unsaved work.\n"

static int have_warned = 0;

const char *pid_file = "/var/run/battmond.pid";

void oops(char * str, ...)
{
  va_list ap;

  char * err_str = NULL;
  static const char * perr_glue = ": ";
  static const char * perr_str = "%m";
  int have_str = 0, have_glue = 0;

  int err_str_len = 0;
  
  if (str != NULL && str[0] != 0) {
    err_str_len += strlen(str);
    have_str = 1;
  }

  if (errno != 0) {
    if (have_str == 1) {
      err_str_len += strlen(perr_glue);
      have_glue = 1;
    }
    err_str_len += strlen(perr_str);
  }

  err_str = (char*)malloc(err_str_len + 1);

  if (have_str == 1)
    strncpy(err_str, str, err_str_len);
  if (have_glue)
    strncat(err_str, perr_glue, err_str_len);
  if (errno != 0)
    strncat(err_str, perr_str, err_str_len);

  va_start(ap, str);
  vsyslog(LOG_ERR, str, ap); 
  va_end(ap);

  if (acpidev != NULL)
    free(acpidev);
  exit(errno);
}

void _usage(char * argv0)
{
  errno = 0;
  fprintf(stderr, "Usage: %s [-diWHh]\n", argv0);
  exit(0);
}

#define usage() _usage(argv[0])

int main(int argc, char ** argv)
{
	union acpi_battery_ioctl_arg battio;
  int acpifd, ch;
  int interval = 10; // Check every 10 seconds.
  int warn = 10; // Percentage of charge to emit a warning.
  int halt = 5; // Percentage of charge to halt the system.
  pid_t otherpid;
  struct pidfh *pfh = NULL;

  while ((ch = getopt(argc, argv, "p:d:i:W:H:h")) != -1) {
    switch (ch) {
      case 'p':
        pid_file = optarg;
        break;
      case 'd':
        if (acpidev != NULL)
          free(acpidev);
        acpidev = strdup(optarg);
        break;
      case 'i':
        interval = atoi(optarg);
        if (interval <= 0)
          oops("Error in interval value");
        break;
      case 'W':
        warn = atoi(optarg);
        if (warn <= 0)
          oops("Error in warning threshold value");
        break;
      case 'H':
        halt = atoi(optarg);
        if (halt <= 0)
          oops("Error in halt threshold value");
        break;
      case 'h':
      default:
        usage();
    }
  }
  if (acpidev == NULL)
    acpidev = strdup(ACPIDEV);
  if (warn <= halt)
    oops ("Warning threshold is lower or equal to the halt threshold");

  pfh = pidfile_open(pid_file, 0600, &otherpid);
  if (pfh == NULL) {
    if (errno == EEXIST) {
      syslog(LOG_ERR, "%s already running, pid: %d",
          getprogname(), otherpid);
      exit(EX_OSERR);
    }
    syslog(LOG_WARNING, "pidfile_open() failed: %m");
  }

  if( daemon(0, 0) < 0) {
    pidfile_remove(pfh);
    oops ("Can't spawn daemon process. Bailing out...");
  }

  if (pidfile_write(pfh) == -1) {
    syslog(LOG_WARNING, "pidfile_write(): %m");
  }

  pidfile_close(pfh);

  int units = 5;
  if (ioctl(acpifd, ACPIIO_BATT_GET_UNITS, &units) == -1) {
    syslog(LOG_WARNING,
        "Unable to retrieve battery count. Defaulting to probing approach...");
  }

  while (1) {
    errno = 0;
    if ((acpifd = open(acpidev, O_RDONLY)) == -1)
      oops("Unable to open acpi device %s", acpidev);

    int total_cap = 0;
    int unit = 0;

    for (unit = 0; unit < units; unit ++) {

      bzero(&battio, sizeof(battio));
      battio.unit = unit;
      if (ioctl(acpifd, ACPIIO_BATT_GET_BATTINFO, &battio) != -1) {
        if (battio.battinfo.state != ACPI_BATT_STAT_NOT_PRESENT && battio.battinfo.state != ACPI_BATT_STAT_CHARGING &&
            battio.battinfo.cap != -1) {
          total_cap += battio.battinfo.cap;
        } else {
          have_warned = 0;
        }
      } else {
        break;
      }
    }
    if (total_cap > 0) {
      if (total_cap <= halt) {
        syslog(LOG_EMERG, BATT_HALT);
        execl("/sbin/halt", "halt", "-p", NULL);
        oops("execl");
      }
      else if (total_cap <= warn) {
        if (have_warned == 0) {
          syslog(LOG_ALERT, BATT_WARN);
          have_warned = 1;
        }
      } else {
        have_warned = 0;
      }
    }

    close(acpifd);
    sleep(interval);
  }

  return 0;
}
