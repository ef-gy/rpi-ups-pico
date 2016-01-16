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

static long getKeyA(struct i2c *i2c) { return getByte(i2c, 0x69, 0x09); }

static long getKeyB(struct i2c *i2c) { return getByte(i2c, 0x69, 0x0a); }

static long getKeyF(struct i2c *i2c) { return getByte(i2c, 0x69, 0x0b); }

static long getTemperature1(struct i2c *i2c) {
  return getByte(i2c, 0x69, 0x0c);
}

static long getTemperature2(struct i2c *i2c) {
  return getByte(i2c, 0x69, 0x0d);
}

int main(int argc, char **argv) {
  char *adaptor = "/dev/i2c-1";
  struct i2c i2c = {0, 0};
  char daemonise = 0;
  char status = 0;
  int opt;

  while ((opt = getopt(argc, argv, "a:dsv")) != -1) {
    switch (opt) {
    case 'a':
      adaptor = optarg;
      break;
    case 'd':
      daemonise = 1;
      break;
    case 's':
      status = 1;
      break;
    case 'v':
      printf("pico-i2cd/%i\n", version);
      return 0;
    default:
      printf("Usage: %s [-a <adaptor>] [-d] [-s] [-v]\n", argv[0]);
      return -3;
    }
  }

  i2c.device = open(adaptor, O_RDWR);
  if (i2c.device < 0) {
    fprintf(stderr, "Could not open adaptor: '%s'; ERRNO=%d.\n", adaptor,
            errno);
    return -1;
  }

  if (daemonise == 1) {
    if (daemon(0, 0) < 0) {
      printf("Failed to daemonise properly; ERRNO=%d.\n", errno);

      return -2;
    }
  }

  if (status) {
    printf("pico_firmware_version %ld\n", getVersion(&i2c));
    printf("pico_mode %ld\n", getMode(&i2c));
    printf("pico_battery_volts %f\n", getBatteryVoltage(&i2c));
    printf("pico_host_volts %f\n", getHostVoltage(&i2c));
    printf("pico_key_a %ld\n", getKeyA(&i2c));
    printf("pico_key_b %ld\n", getKeyB(&i2c));
    printf("pico_key_f %ld\n", getKeyF(&i2c));
    printf("pico_temperature_1_celsius_degrees %ld\n", getTemperature1(&i2c));
    printf("pico_temperature_2_celsius_degrees %ld\n", getTemperature2(&i2c));
  }

  (void)close(i2c.device);
  /* ignore this return value, as we're terminating the programme next, which
     also closes the file. */

  return 0;
}
