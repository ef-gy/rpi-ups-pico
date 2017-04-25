/**\file
 * \brief UPS PIco control daemon.
 *
 * The PIco UPS for the Raspberry Pi by pimodules.com requires some userspace
 * help to function correctly. The vendor provides a Python script -
 * picofssd.py - to do so, but it seems strange to force users to use Python
 * for an embedded device's UPS.
 *
 * This programme replaces the aforementioned Python script. It's a lot smaller
 * than the original, nicely compiled and adds the ability to spawn into a
 * daemon proper, so you don't have to screw around with nohup and & in your
 * rc.local script.
 *
 * \copyright
 * This programme is released as open source, under the terms of an MIT/X style
 * licence. See the accompanying LICENSE file for details.
 *
 * \see Source Code Repository: https://github.com/ef-gy/rpi-ups-pico
 * \see Documentation: https://ef.gy/documentation/rpi-ups-pico
 * \see Hardware: http://pimodules.com/_pdf/_pico/UPS_PIco_BL_FSSD_V1.0.pdf
 * \see Hardware Vendor: http://pimodules.com/
 * \see Licence Terms: https://github.com/ef-gy/rpi-ups-pico/blob/master/LICENSE
 */

/* for open() */
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

/* for usleep(), read(), write(), getopt(), daemon() */
#include <unistd.h>

/* for snprintf() */
#include <stdio.h>

/* for system() */
#include <stdlib.h>

/* for errno */
#include <errno.h>

/**\brief Maximum length of GPIO file name.
 *
 * This is the maximum length of a GPIO file that we're willing to support.
 */
#define MAX_GPIO_FN 256

/**\brief Internal buffer size.
 *
 * Size of internal buffers used throughout the code.
 */
#define MAX_BUFFER 32

/**\brief Daemon version
 *
 * The version number of this daemon. Will be increased around release time.
 */
static const int version = 3;

/**\brief Maximum number of retries.
 *
 * Used during GPIO pin setup, to prevent random setup delays from causing
 * initialisation to fail.
 */
static const int maxRetries = 8;

/**\brief Export GPIO pin
 *
 * Linux's sysfs interface for GPIO pins requires setting up the pins that you
 * intend to use by first "exporting" them, which creates the pin's control
 * files in sysfs. This function tells the kernel to do so.
 *
 * \param[in] gpio The pin to export.
 *
 * \returns 0 on success, negative numbers on (partial) failures.
 */
static int export(int gpio) {
  int fd;
  char bf[MAX_BUFFER];
  int rv = 0;
  int blen = 0;

  blen = snprintf(bf, MAX_BUFFER, "%i", gpio);
  if (blen < 0) {
    return -1;
  }

  fd = open("/sys/class/gpio/export", O_WRONLY);
  if (fd < 0) {
    return -2;
  }

  if (write(fd, bf, blen) < blen) {
    rv = -3;
  }

  do {
    rv = close(fd);
  } while ((rv < 0) && (errno == EINTR));

  return rv;
}

/**\brief Set a GPIO pin's I/O direction.
 *
 * Set the I/O direction of a GPIO pin that has previously been exported.
 *
 * \param[in] gpio   The pin to set up.
 * \param[in] output Nonzero for output, 0 for input.
 *
 * \returns 0 on success, negative numbers on (partial) failures.
 */
static int direction(int gpio, char output) {
  int fd;
  char fn[MAX_GPIO_FN];
  int rv = 0;

  if (snprintf(fn, MAX_GPIO_FN, "/sys/class/gpio/gpio%i/direction", gpio) < 0) {
    return -1;
  }

  fd = open(fn, O_WRONLY);
  if (fd < 0) {
    return -2;
  }

  if (output) {
    if (write(fd, "out\n", 4) < 4) {
      rv = -3;
    }
  } else {
    if (write(fd, "in\n", 3) < 3) {
      rv = -3;
    }
  }

  do {
    rv = close(fd);
  } while ((rv < 0) && (errno == EINTR));

  return rv;
}

/**\brief Export GPIO pin and set I/O direction.
 *
 * Calls export() and then direction() to set up a GPIO pin.
 *
 * \param[in] gpio   The pin to set up.
 * \param[in] output Nonzero for output, 0 for input.
 *
 * \returns 0 on success, negative numbers on (partial) failures.
 */
static int setup(int gpio, char output) {
  int rv = 0;
  int retries = 0;

  rv = export(gpio);
  if (rv < 0) {
    return rv;
  }

  do {
    if (retries > 0) {
      /* setting the pin IO direction may fail for a few milliseconds after
       * exporting the pin, so retry this step a few times. Each time we try
       * this, we wait a bit longer. */
      usleep(retries * retries * 1000);
    }
    rv = direction(gpio, output);
  } while ((rv < 0) && (retries++ < maxRetries));

  if (rv < 0) {
    return rv;
  }

  return 0;
}

/**\brief Set a GPIO pin's state
 *
 * Pins can either be LOW or HIGH. This function sets a pin to the given target
 * state. The pin must previously have been set up as an output pin.
 *
 * \param[in] gpio  The pin to set.
 * \param[in] state The state to set the pin to.
 *
 * \returns 0 on success, negative numbers on (partial) failures.
 */
static int set(int gpio, char state) {
  char fn[MAX_GPIO_FN];
  int rv = 0;

  if (snprintf(fn, MAX_GPIO_FN, "/sys/class/gpio/gpio%i/value", gpio) < 0) {
    return -1;
  } else {
    int fd = open(fn, O_WRONLY);

    if (fd) {
      (void)write(fd, state ? "1\n" : "0\n", 2);
      /* we ignore the return value of that write because we use this in a loop
         anyway, and it's quite unlikely to fail. */

      do {
        rv = close(fd);
      } while ((rv < 0) && (errno == EINTR));
    }
  }

  return rv;
}

/**\brief Get the value of a GPIO pin.
 *
 * Queries a GPIO pin's value. The pin must have been set up to be an input pin
 * beforehand.
 *
 * \param[in] gpio The pin to query.
 *
 * \returns 1 if the pin is HIGH, 0 if the pin is LOW, negative numbers on
 *          (partial) failures.
 */
static int get(int gpio) {
  char fn[MAX_GPIO_FN];
  int rv = 0;

  if (snprintf(fn, MAX_GPIO_FN, "/sys/class/gpio/gpio%i/value", gpio) < 0) {
    return -1;
  } else {
    int fd = open(fn, O_RDONLY);

    if (fd) {
      char buf[MAX_BUFFER];
      int r = read(fd, buf, MAX_BUFFER);

      if (r < 1) {
        rv = -2;
      } else {
        rv = (buf[0] == '1');
      }

      do {
        r = close(fd);
      } while ((r < 0) && (errno == EINTR));
    }
  }

  return rv;
}

/**\brief Create a pulse on a GPIO pin.
 *
 * This function creates a pulse on a GPIO pin that has previously been set up
 * to be an output pin. The pin will be set to HIGH for the given duration, then
 * set to LOW for the remainder of the period.
 *
 * \note The duration must be smaller than the period.
 *
 * \param[in] gpio     The pin to send the pulse to.
 * \param[in] period   The amount of time for the full pulse; in usec.
 * \param[in] duration The amount of time to set the pin to HIGH; in usec.
 *
 * \returns 0 on success, negative numbers on (partial) failures.
 */
static int pulse(int gpio, unsigned int period, unsigned int duration) {
  if (set(gpio, 1) != 0) {
    return -1;
  }

  (void)usleep(duration);
  /* we ignore usleep()'s return value, because the only error would be to be
   * interrupted by a signal, which is OK as the PIco does not seem to be that
   * particular about the exact shape of the pulse train. */

  if (set(gpio, 0) != 0) {
    return -2;
  }

  (void)usleep(period - duration);

  return 0;
}

/**\brief picod's main function.
 *
 * Parses some command line options and then creates a pulse train on pin #22,
 * which is what the PIco UPS requires for it to figure out that the Raspberry
 * Pi it's connected to is running.
 *
 * The only exit condition for the daemon is if pin #27 is set to LOW, which
 * will trigger what the hardware vendor dubbed a "File Safe Shut Down." I.e. a
 * good old "shutdown -h now."
 *
 * Because of this, and the whole thing about the GPIO pins, this daemon needs
 * to be run as root and can't drop privileges. If you were to set things up
 * such that the GPIO pin #22 is available to ordinary users, and you used -n to
 * disable the FSSD function, you could run it as non-root.
 *
 * * -n disables the FSSD test, if you don't care about this feature.
 * * -d launches the programme as a daemon. Pin setup is performed before the
 *   daemon() call, which allows error reporting for that.
 * * -v prints the version of the daemon and then exits.
 *
 * \param[in] argc Argument count.
 * \param[in] argv Argument vecotr.
 *
 * \returns 0 on success, negative numbers for programme setup errors.
 */
int main(int argc, char **argv) {
  char daemonise = 0;
  char fssd = 1;
  char initialPulse = 1;
  char fssdWasHigh = 0;
  int opt;

  while ((opt = getopt(argc, argv, "dnv")) != -1) {
    switch (opt) {
    case 'd':
      daemonise = 1;
      break;
    case 'n':
      fssd = 0;
      break;
    case 'v':
      printf("picod/%i\n", version);
      return 0;
    default:
      printf("Usage: %s [-d] [-n] [-v]\n", argv[0]);
      return -3;
    }
  }

  if (setup(22, 1) != 0) {
    printf("Could not set up pin #22 as an output pin for the pulse train.\n");

    return -1;
  }

  if (fssd == 1) {
    if (setup(27, 0) != 0) {
      printf("Could not set up pin #27 as input for the FSSD feature.\n");

      return -4;
    }
  }

  if (daemonise == 1) {
    if (daemon(0, 0) < 0) {
      printf("Failed to daemonise properly; ERRNO=%d.\n", errno);

      return -2;
    }
  }

  /* create a pulse train with the same modulation as the PIco's FSSD script. */
  while (1) {
    int fssdSignal = (fssd == 1) ? get(27) : 1;
    /* if processing the FSSD signal is disabled, assume it's HIGH so as not to
     * trigger a shutdown, ever. */

    fssdWasHigh = (fssdSignal == 1) ? 1 : fssdWasHigh;
    /* keep track of whether we've ever seen the FSSD signal in a HIGH state; if
     * we haven't, then we assume the PIco has not been installed. */

    if ((initialPulse == 1) || (fssdWasHigh == 1)) {
      /* only send the pulse train if the FSSD signal scanned HIGH recently;
       * this means that the pulse train is not sent if shutdown has been
       * initiated due to a low battery state, or if the PIco has not been
       * installed. */

      (void)pulse(22, 500000, 250000);
      /* note how we don't use the return value here, because we'd really just
       * send another pulse. */

      initialPulse = 0;
      /* we send one initial pulse at boot up, just in case the PIco firmware
       * would only set pin #27 to HIGH upon receiving the initial pulse; not
       * sure if this is needed, but it shouldn't hurt, either. */
    }

    if ((fssdWasHigh == 1) && (fssdSignal == 0)) {
      /* we ignore the error condition on the get() because the only thing to do
       * in that case is to re-issue that, and we'll do that in 500ms. */

      (void)system("shutdown -h now");
      /* there's nothing else to do here - regardless of whether the call fails.
       * so we bail after this. */

      fssdWasHigh = 0;
      /* reset the FSSD HIGH sensing; the daemon will keep running and reinstate
       * the pulse train if power is restored, though we can't cancel the
       * shutdown so something external would have to do that. */
    }
  }

  return 0;
}
