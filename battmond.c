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

void oops(char * str)
{
	perror(str);
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

#ifndef DEBUG
	if (daemon(0, 0) < 0) {
		pidfile_remove(pfh);
		oops ("Can't spawn daemon process. Bailing out...");
	}
#endif

	if (pidfile_write(pfh) == -1) {
		syslog(LOG_WARNING, "pidfile_write(): %m");
	}

	pidfile_close(pfh);


	while (1) {
		int total_cap = 0;
		int units;
		int unit = 0;
		int num_discharging = 0, num_charging = 0;

		if ((acpifd = open(acpidev, O_RDONLY)) == -1)
			oops("Unable to open acpi device");

		if (ioctl(acpifd, ACPIIO_BATT_GET_UNITS, &units) == -1) {
			syslog(LOG_WARNING,
					"Unable to retrieve battery count. Defaulting to probing approach...");
			units = 5;
		}
#ifdef DEBUG
		fprintf(stderr, "%d battery unit%s detected\n", units, (units > 1) ? "s" : "");
#endif

		for (unit = 0; unit < units; unit ++) {
			bzero(&battio, sizeof(battio));
			battio.unit = unit;
			if (ioctl(acpifd, ACPIIO_BATT_GET_BATTINFO, &battio) != -1) {
#ifdef DEBUG
				fprintf(stderr, "Battery unit %d: ", unit);
#endif
				if (battio.battinfo.state != ACPI_BATT_STAT_NOT_PRESENT && 
						battio.battinfo.cap != -1) {
#ifdef DEBUG
					fprintf(stderr, "present%s (%d%%)\n", (battio.battinfo.state & ACPI_BATT_STAT_DISCHARG) ? "/discharging" : "", battio.battinfo.cap);
#endif
					total_cap += battio.battinfo.cap;
					if ((battio.battinfo.state & ACPI_BATT_STAT_DISCHARG)) {
						num_discharging++;
					} else {
						num_charging++;
						have_warned = 0;
					}
				} else {
#ifdef DEBUG
					fprintf(stderr, "not present\n");
#endif
					have_warned = 0;
				}
			}
		}
#ifdef DEBUG
		fprintf(stderr, "Total battery capacity: %d%%\n", total_cap);
#endif
		if (num_discharging && !num_charging && total_cap > 0) {
			if (total_cap <= halt) {
				syslog(LOG_EMERG, BATT_HALT);
				close(acpifd);
				execl("/sbin/halt", "halt", "-p", NULL);
				oops("execl");
			}
			else if (total_cap <= warn) {
				if (!have_warned) {
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
