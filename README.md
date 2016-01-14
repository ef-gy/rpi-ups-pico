# rpi-ups-pico
C daemon that replaces the functionality of the Python script that powers the
[Raspberry Pi UPS PIco](http://pimodules.com/_pdf/_pico/UPS_PIco_BL_FSSD_V1.0.pdf)
by [pimodules.com](http://pimodules.com/).

## Description

*picod* creates the pulse train necessary for the Raspberry Pi UPS PIco to work
correctly. This device expects a pulse train on GPIO pin #22 to determine if the
Raspberry Pi it is attached to is booted up. This then influences whether the
firmware will charge the battery - without the pulse train, the device may
appear to work correctly and will even switch over to the battery if power is
lost, but the battery will not be charged, rendering the device quite useless
quite fast.

In addition to this, the daemon monitors pin #27, which is used by the PIco to
indicate that the battery level is critical and the system needs to shut down.
The PIco sets this pin to HIGH during normal operation, and LOW to indicate this
condition. Upon receiving this signal, the daemon calls "shutdown -h now" to try
and shut down gracefully. Power will be cut shortly after, so this is necessary.

## Installation

Check out the sources, then compile manually:

    $ make picod

Copy the resulting binary to a convenient location, e.g. /sbin:

    $ sudo install picod /sbin

Then make sure it gets executed upon boot. To do so, you could add a line like
the following to your /etc/rc.local:

    /sbin/picod -d

The *-d* option forks the programme to be in the background. For more options
and details see the provided manpage in "picod.1".
