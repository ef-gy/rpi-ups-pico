/**\file
 * \brief UPS PIco I2C input driver.
 *
 * The PIco UPS for the Raspberry Pi by pimodules.com has a few buttons that are
 * not normally available to userspace programmes. They can be scanned through
 * I2C, however, and this daemon is designed to do so and make these button
 * events available to userspace via the uinput kernel subsystem.
 *
 * Assuming you have the uinput kernel module loaded, upon running this daemon
 * you will see a new input device pop up in /dev/input. This will most likely
 * be recognised as a joystick, and it exposes three buttons to anything using
 * the input device: BTN_A, BTN_B and BTN_C, corresponding to KEY_A, KEY_B and
 * KEY_F on the PIco. (There is no BTN_F, and it felt wrong to use keyboard scan
 * codes for this.)
 *
 * \copyright
 * This programme is released as open source, under the terms of an MIT/X style
 * licence. See the accompanying LICENSE file for details.
 *
 * \see Source Code Repository: https://github.com/ef-gy/rpi-ups-pico
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

/* for errno */
#include <errno.h>

/* for ioctl() */
#include <sys/ioctl.h>

/* for I2C /dev interface macros */
/* note that this requires the version of this file from libi2c-dev */
#include <linux/i2c-dev.h>

/* for Linux input device macros */
#include <linux/uinput.h>

/**\brief Daemon version
 *
 * The version number of this daemon. Will be increased around release time.
 */
static const int version = 1;

/**\brief I2C state
 *
 * Contains the current state - as we know it - of the I2C device we have open.
 */
struct i2c {
  /**\brief Device file descriptor
   *
   * The OS file descriptor for the open device file.
   */
  int device;

  /**\brief Current I2C address we dialed to
   *
   * Different PIco commands are using different I2C addresses as well as
   * registers. This is to remember the last address we dialed, so we don't need
   * to re-issue those syscalls to change to a different address.
   */
  int addr;
};

/**\brief Decode PIco floats
 *
 * The PIco encodes floats (e.g. voltages) as fixed-point decimal word, with the
 * higher-value byte representing the part before the decimal point and the
 * lower-value byte representing the part after the decimal point.
 *
 * I'm not entirely sure about the range on the latter part, the manual doesn't
 * seem to say. The assumption I saw in scripts seems to have been that the
 * range is 0-100.
 *
 * \param[in] w The word to convert to a float.
 *
 * \returns A floating point number, positive if successful and negative if not.
 */
static float getFloat(long w) {
  float v1 = w & 0xff;
  float v2 = (w >> 8) & 0xff;

  return v2 + v1 / 100.;
}

/**\brief Select I2C address
 *
 * Sets the I2C address to read data from. If the current address is the same as
 * the one that was dialed last, this function does not issue a syscall to
 * change the address.
 *
 * \param[out] i2c  The I2C state struct.
 * \param[in]  addr The I2C address to select.
 *
 * \returns 0 on success, negative values otherwise.
 */
static int selectAddr(struct i2c *i2c, int addr) {
  if (i2c->addr == addr) {
    return 0;
  }

  if (ioctl(i2c->device, I2C_SLAVE, addr) < 0) {
    return -1;
  }

  i2c->addr = addr;

  return 0;
}

/**\brief Read word from I2C via SMBUS
 *
 * Reads a word from the given I2C address and register via SMBUS.
 *
 * \param[out] i2c  The I2C state struct.
 * \param[in]  addr The I2C address to read from.
 * \param[in]  reg  The register to read.
 *
 * \returns Negative values on failure; the read value otherwise.
 */
static long getWord(struct i2c *i2c, int addr, int reg) {
  if (selectAddr(i2c, addr) < 0) {
    return -1;
  } else {
    long res = i2c_smbus_read_word_data(i2c->device, reg);
    if (res < 0) {
      return -3;
    }
    return res;
  }
}

/**\brief Read byte from I2C via SMBUS
 *
 * Reads a byte from the given I2C address and register via SMBUS.
 *
 * \param[out] i2c  The I2C state struct.
 * \param[in]  addr The I2C address to read from.
 * \param[in]  reg  The register to read.
 *
 * \returns Negative values on failure; the read value otherwise.
 */
static long getByte(struct i2c *i2c, int addr, int reg) {
  if (selectAddr(i2c, addr) < 0) {
    return -1;
  } else {
    long res = i2c_smbus_read_byte_data(i2c->device, reg);
    if (res < 0) {
      return -3;
    }
    return res;
  }
}

/**\brief Store byte to I2C via SMBUS
 *
 * Stores a byte at the given I2C address and register via SMBUS.
 *
 * \param[out] i2c   The I2C state struct.
 * \param[in]  addr  The I2C address to read from.
 * \param[in]  reg   The register to read.
 * \param[in]  value The value to write.
 *
 * \returns Negative values on failure; 0 otherwise.
 */
static long setByte(struct i2c *i2c, int addr, int reg, int value) {
  if (selectAddr(i2c, addr) < 0) {
    return -1;
  } else {
    long res = i2c_smbus_write_byte_data(i2c->device, reg, value);
    if (res < 0) {
      return -3;
    }
    return res;
  }
}

/**\brief Get PIco battery voltage.
 *
 * Read the voltage of battery connected to the PIco. The battery has a nominal
 * voltage of around 3.7 V, and a useful voltage down to about 3.5 V.
 *
 * \param[out] i2c The I2C state struct.
 *
 * \returns A positive float of the voltage, if it could be read. A negative
 *     number on error.
 */
static float getBatteryVoltage(struct i2c *i2c) {
  return getFloat(getWord(i2c, 0x69, 0x01));
}

/**\brief Get Raspberry Pi 5V pin voltage.
 *
 * Read the voltage of the 5V input line, as seen by the PIco.
 *
 * \param[out] i2c The I2C state struct.
 *
 * \returns A positive float of the voltage, if it could be read. A negative
 *     number on error.
 */
static float getHostVoltage(struct i2c *i2c) {
  return getFloat(getWord(i2c, 0x69, 0x03));
}

/**\brief Read out PIco firmware version.
 *
 * This functions reads the firmware version register of the PIco. Note that
 * these are typically written out in hexadecimal, so the readout may look
 * different than what you're expecting if you don't adjust for that.
 *
 * \param[out] i2c The I2C state struct.
 *
 * \returns Negative numbers on errors, or the version number otherwise.
 *     Some version numbers have a special meaning. See the PIco manual for more
 *     info on those.
 */
static long getVersion(struct i2c *i2c) { return getByte(i2c, 0x6b, 0x00); }

/**\brief Read out power mode.
 *
 * Reds the power mode register on the PIco, to find out if the device is
 * currently on battery power or not.
 *
 * \param[out] i2c The I2C state struct.
 *
 * \returns 1 if the device is plugged in, 2 if it's on battery power. Any other
 *     code means that something is wrong.
 */
static long getMode(struct i2c *i2c) { return getByte(i2c, 0x69, 0x00); }

/**\brief Get key status.
 *
 * PIco key presses are sensed via I2C. Once a key is pressed, the corresponding
 * register is set to 1, otherwise it is set to 0.
 *
 * This function is used to read the I2C register for a given key. Possible
 * values are 0 for KEY_A, 1 for KEY_B and 2 for KEY_F.
 *
 * \param[out] i2c The I2C state struct.
 * \param[in]  key The key to read the state of (0, 1 or 2).
 *
 * \returns Negative number on failure, 0 otherwise.
 */
static long getKey(struct i2c *i2c, int key) {
  return getByte(i2c, 0x69, 0x09 + key);
}

/**\brief Set key to 0.
 *
 * PIco key presses are sensed via I2C. Once a key is pressed, the corresponding
 * register is set to 1. It has to manually be set back to 0 after successfully
 * reading it, which is what this function does.
 *
 * \param[out] i2c The I2C state struct.
 * \param[in]  key The key to set to 0 (0, 1 or 2).
 *
 * \returns Negative number on failure, 0 otherwise.
 */
static long resetKey(struct i2c *i2c, int key) {
  return setByte(i2c, 0x69, 0x09 + key, 0);
}

/**\brief Read out temperature sensor.
 *
 * The PIco has up to two temperature sensors, one built in by default and one
 * as part of the fan kit. This function can be used to read either, depending
 * on the value of the 'sensor' parameter: 0 for the built-in sensor and 1 for
 * the external one.
 *
 * \param[out] i2c    The I2C state struct.
 * \param[in]  sensor The sensor to read out (0 or 1).
 *
 * \returns Negative number on failure, or the readout as degrees Celsius.
 */
static long getTemperature(struct i2c *i2c, int sensor) {
  return getByte(i2c, 0x69, 0x0c + sensor);
}

/**\brief PIco I2C driver main function
 *
 * Parses some command line variables and then opens an I2C connection to the
 * PIco module. If the connection attempt succeeds, the code will then dump the
 * current state of the PIco and/or create a virtual input device for the
 * buttons on the PIco.
 *
 * The state is dumped with the '-s' parameter, and the output format is roughly
 * compatible with the text format used by the Prometheus monitoring programme.
 *
 * The virtual input device is created using the uinput kernel driver, which
 * allows a user-space programme to act as an input device. For this mode, the
 * programme will most likely need to be run as root, as /dev/uinput is usually
 * only writable by the root user.
 *
 * * -a <address> selects the I2C device to use. The default is /dev/i2c-1,
 *   which is the typical I2C device file on Raspberry Pi 2 and B+.
 * * -d launches the programme as a daemon. Setup is performed before the
 *   daemon() call, which allows error reporting for that.
 * * -i Do not run the input device loop. The default is to run it.
 * * -s Dump current PIco state. The default is not to do so.
 * * -u <uinput> selects the uinput device file. /dev/uinput seems to be used by
 *   Debian, even though the canonical location is /dev/input/uinput.
 * * -v prints the version of the daemon and then exits.
 *
 * \param[in] argc Argument count.
 * \param[in] argv Argument vecotr.
 *
 * \returns 0 on success, negative numbers for programme setup errors.
 */
int main(int argc, char **argv) {
  char *adaptor = "/dev/i2c-1";
  char *uinput = "/dev/uinput";
  struct i2c i2c = {0, 0};
  char daemonise = 0;
  char status = 0;
  char input_loop = 1;
  int opt;

  while ((opt = getopt(argc, argv, "a:disu:v")) != -1) {
    switch (opt) {
    case 'a':
      adaptor = optarg;
      break;
    case 'd':
      daemonise = 1;
      break;
    case 'i':
      input_loop = 0;
      break;
    case 's':
      status = 1;
      break;
    case 'u':
      uinput = optarg;
      break;
    case 'v':
      printf("pico-i2cd/%i\n", version);
      return 0;
    default:
      printf("Usage: %s [-a <adaptor>] [-d] [-i] [-s] [-u <uinput>] [-v]\n",
             argv[0]);
      return -3;
    }
  }

  i2c.device = open(adaptor, O_RDWR);
  if (i2c.device < 0) {
    fprintf(stderr, "Could not open adaptor: '%s'; ERRNO=%d.\n", adaptor,
            errno);
    return -1;
  }

  if (status) {
    printf("pico_firmware_version %ld\n", getVersion(&i2c));
    printf("pico_mode %ld\n", getMode(&i2c));
    printf("pico_battery_volts %f\n", getBatteryVoltage(&i2c));
    printf("pico_host_volts %f\n", getHostVoltage(&i2c));
    printf("pico_temperature_1_celsius_degrees %ld\n", getTemperature(&i2c, 0));
    printf("pico_temperature_2_celsius_degrees %ld\n", getTemperature(&i2c, 1));
  }

  if (input_loop) {
    int device = open(uinput, O_WRONLY | O_NONBLOCK), i;
    struct uinput_user_dev userdev = {
        "Raspberry Pi PIco UPS", {BUS_I2C, 0x0000, 0x0000, version}, 0};
    struct input_event event = {{0}, EV_KEY, 0};
    struct input_event syn = {{0}, EV_SYN, SYN_REPORT};
    int code[3] = {BTN_A, BTN_B, BTN_C};
    char release[3] = {0, 0, 0};
    char synchronise = 0;

    if (device < 0) {
      fprintf(stderr, "Could not open uinput: '%s'; ERRNO=%d.\n", uinput,
              errno);
      return -2;
    }

    if (daemonise == 1) {
      if (daemon(0, 0) < 0) {
        printf("Failed to daemonise properly; ERRNO=%d.\n", errno);

        return -3;
      }
    }

    if (ioctl(device, UI_SET_EVBIT, EV_KEY) < 0) {
      fprintf(stderr, "Could not set event bits: ERRNO=%d.\n", errno);
      return -5;
    }
    if (ioctl(device, UI_SET_EVBIT, EV_SYN) < 0) {
      fprintf(stderr, "Could not set event bits: ERRNO=%d.\n", errno);
      return -5;
    }

    for (i = 0; i < 3; i++) {
      if (ioctl(device, UI_SET_KEYBIT, code[i]) < 0) {
        fprintf(stderr, "Could not declare key code: ERRNO=%d.\n", errno);
        return -5;
      }
    }

    if (write(device, &userdev, sizeof(userdev)) != sizeof(userdev)) {
      fprintf(stderr, "Could not write device id: ERRNO=%d.\n", errno);
      return -5;
    }

    if (ioctl(device, UI_DEV_CREATE) < 0) {
      fprintf(stderr, "Could not create input device: ERRNO=%d.\n", errno);
      return -5;
    }

    while (1) {
      for (i = 0; i < 3; i++) {
        int scan = getKey(&i2c, i);
        if (release[i]) {
          if (scan == 0) {
            event.code = code[i];
            event.value = 0;
            if (write(device, &event, sizeof(event)) == sizeof(event)) {
              /* event has been sent successfully */
              release[i] = 0;
              synchronise = 1;
            }
          } else {
            /* I supose this could happen if the button is still being pressed?
               let's just reset it to 0 again... */
            resetKey(&i2c, i);
          }
        } else {
          if (scan > 0) {
            event.code = code[i];
            event.value = 1;
            if (write(device, &event, sizeof(event)) == sizeof(event)) {
              /* event has been sent successfully */
              release[i] = 1;
              resetKey(&i2c, i);
              synchronise = 1;
            }
          }
        }
      }

      if (synchronise) {
        (void)write(device, &syn, sizeof(syn));
        /* AFAICT the SYN for this should be optional, we're only sending it
           for completeness' sake. Therefore, if it couldn't be sent right, we
           ought to be able to ignore it. */
        synchronise = 0;
      }

      (void)usleep(100000);
      /* ignore the return status */
    }

    /* we should never reach this part of the code. */

    (void)ioctl(device, UI_DEV_DESTROY);
    /* clean up, but ignore the return status since this should not be
       reachable. */

    (void)close(device);
    /* same here. */
  }

  /* we only ever reach this part of the code IFF we disabled the input loop. */

  (void)close(i2c.device);
  /* ignore this return value, as we're terminating the programme next, which
     also closes the file. */

  return 0;
}
