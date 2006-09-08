#!/bin/sh
#
# battmond.sh for rc.d usage
# $FreeBSD$

# PROVIDE: battmond
# REQUIRE: DAEMON
#
# Add the following line to /etc/rc.conf to enable battmond:
#
# battmond_enable="YES"
#

battmond_enable=${battmond_enable-"NO"}
battmond_flags=${battmond_falgs-""}

. /etc/rc.subr

name=battmond
rcvar=`set_rcvar`

command=/usr/local/sbin/${name}
pidfile=/var/run/${name}.pid
sig_stop=-KILL

load_rc_config ${name}
run_rc_command "$1"
