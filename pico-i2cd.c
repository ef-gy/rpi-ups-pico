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

struct i2c {
  int device;
  int addr;
};

static float getFloat(long w) {
  float v1 = w & 0xff;
  float v2 = (w >> 8) & 0xff;

  return v2 + v1 / 100.;
}

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

static float getFloatWord(struct i2c *i2c, int addr, int reg) {
  return getFloat(getWord(i2c, addr, reg));
}

static float getBatteryVoltage(struct i2c *i2c) {
  return getFloatWord(i2c, 0x69, 0x01);
}

static float getHostVoltage(struct i2c *i2c) {
  return getFloatWord(i2c, 0x69, 0x03);
}

static long getVersion(struct i2c *i2c) { return getByte(i2c, 0x6b, 0x00); }

static long getMode(struct i2c *i2c) { return getByte(i2c, 0x69, 0x00); }

static long getKey(struct i2c *i2c, int key) {
  return getByte(i2c, 0x69, 0x09 + key);
}

static long resetKey(struct i2c *i2c, int key) {
  return setByte(i2c, 0x69, 0x09 + key, 0);
}

static long getTemperature1(struct i2c *i2c) {
  return getByte(i2c, 0x69, 0x0c);
}

static long getTemperature2(struct i2c *i2c) {
  return getByte(i2c, 0x69, 0x0d);
}

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
    printf("pico_temperature_1_celsius_degrees %ld\n", getTemperature1(&i2c));
    printf("pico_temperature_2_celsius_degrees %ld\n", getTemperature2(&i2c));
  }

  if (input_loop) {
    int device = open(uinput, O_WRONLY | O_NONBLOCK);
    struct uinput_user_dev userdev = {
        "Raspberry Pi PIco UPS", {BUS_I2C, 0x0000, 0x0000, version}, 0};
    struct input_event event = {{0}, EV_KEY, 0};
    int code[3] = {BTN_A, BTN_B, BTN_C};
    char release[3] = {0, 0, 0};

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

    for (int i = 0; i < 3; i++) {
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
      for (int i = 0; i < 3; i++) {
        int scan = getKey(&i2c, i);
        if (release[i]) {
          if (scan == 0) {
            event.code = code[i];
            event.value = 0;
            if (write(device, &event, sizeof(event)) == sizeof(event)) {
              /* event has been sent successfully */
              release[i] = 0;
            }
          }
        } else {
          if (scan > 0) {
            event.code = code[i];
            event.value = 1;
            if (write(device, &event, sizeof(event)) == sizeof(event)) {
              /* event has been sent successfully */
              release[i] = 1;
              resetKey(&i2c, i);
            }
          }
        }
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

  (void)close(i2c.device);
  /* ignore this return value, as we're terminating the programme next, which
     also closes the file. */

  return 0;
}
