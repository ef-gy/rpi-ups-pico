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
PIDFILE_PICOD=/var/run/$NAME.picod.pid
PIDFILE_PICO_I2CD=/var/run/$NAME.pico-i2cd.pid

DAEMON_PICOD=/sbin/picod
DAEMON_PICO_I2CD=/sbin/pico-i2cd

DESC="Raspberry Pi PIco UPS control daemon."

case "$1" in
  start)
    echo -n "Starting daemon: $NAME"
    start-stop-daemon --start --quiet --background --pidfile $PIDFILE_PICOD --exec $DAEMON_PICOD
    start-stop-daemon --start --quiet --background --pidfile $PIDFILE_PICO_I2CD --exec $DAEMON_PICO_I2CD
    echo "."
    ;;
  stop)
    echo -n "Stopping daemon: $NAME"
    start-stop-daemon --stop --quiet --pidfile $PIDFILE_PICO_I2CD --exec $DAEMON_PICO_I2CD
    start-stop-daemon --stop --quiet --pidfile $PIDFILE_PICOD --exec $DAEMON_PICOD
    echo "."
    ;;
  restart)
    echo -n "Restarting daemon: $NAME"
    start-stop-daemon --stop --quiet --pidfile $PIDFILE_PICO_I2CD --exec $DAEMON_PICO_I2CD
    start-stop-daemon --stop --quiet --pidfile $PIDFILE_PICOD --exec $DAEMON_PICOD
    start-stop-daemon --start --quiet --background --pidfile $PIDFILE_PICOD --exec $DAEMON_PICOD
    start-stop-daemon --start --quiet --background --pidfile $PIDFILE_PICO_I2CD --exec $DAEMON_PICO_I2CD
    echo "."
    ;;
  *)
    echo "Usage: $1 {start|stop|restart}"
    exit 1
esac

exit 0
