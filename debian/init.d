#!/bin/sh
# kFreeBSD do not accept scripts as interpreters, using #!/bin/sh and sourcing.
if [ true != "$INIT_D_SCRIPT_SOURCED" ] ; then
    set "$0" "$@"; INIT_D_SCRIPT_SOURCED=true . /lib/init/init-d-script
fi
### BEGIN INIT INFO
# Provides:          rpi-ups-pico
# Required-Start:    $syslog
# Required-Stop:     $syslog
# Default-Start:     2 3 4 5
# Default-Stop:      0 1 6
# Short-Description: PIco UPS control daemon.
# Description:       PIco UPS control daemon.
### END INIT INFO

# Author: Magnus Deininger <magnus@ef.gy>

NAME="rpi-ups-pico"
DAEMON=/sbin/picod
DAEMONOPTS="-d"

DESC="Raspberry Pi PIco UPS control daemon."
