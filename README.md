# rpi-ups-pico
A C daemon that replaces the functionality of the Python script that powers the
[Raspberry Pi UPS PIco](http://pimodules.com/_pdf/_pico/UPS_PIco_BL_FSSD_V1.0.pdf)
by [pimodules.com](http://pimodules.com/), and another C daemon that creates a
virtual input device for the buttons on it.

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

*pico-i2cd*, on the other hand, monitors the I2C interface of the PIco and
simulates an input device for the physical hardware buttons on it. These can
then be bound to arbitrary actions using something like triggerhappyd, which is
installed on Pis by default. It needs the *uinput* module to do so.

It can also dump the current status of the PIco to stdout, and will do so in a
format that is roughly compatible with the metrics format used by Prometheus.

## Installation

Make sure you have the full build environment on your platform, and the correct
i2c headers. In particular, if building on a Debian-ish/Raspbian system you'll
need `make`, a C compiler and the `libi2c-dev` package.

Check out the sources, then compile manually:

    $ make

And install the resulting binaries:

    $ sudo make install

Then make sure things run upon boot. To do so, you could add lines like the
following to your /etc/rc.local:

    /sbin/picod -d
    /sbin/pico-i2cd -d

The *-d* option forks the programmes to be in the background. For more options
and details see the provided manpages.

## Reading PIco status

*pico-i2cd* can read the PIco status registers - battery mode, voltages, etc. To
do so, invoke it like this:

    # pico-i2cd -s -i

This will write status information to stdout. The format is the same as the
plaintext /metrics format used by Prometheus, so you could create a cron job to
read that out and put it somewhere you can monitor through Prometheus... maybe.
