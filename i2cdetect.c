#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>

#if 0
#include <i2c/busses.h>
#include <i2c/smbus.h>
#endif 

#define MODE_AUTO	0
#define MODE_QUICK	1
#define MODE_READ	2
#define MODE_FUNC	3

#define VERSION "4.0.0+dev"

static void help(void)
{
	fprintf(stderr,
		"Usage: i2cdetect [-y] [-a] [-q|-r] I2CBUS [FIRST LAST]\n"
		"       i2cdetect -F I2CBUS\n"
		"       i2cdetect -l\n"
		"  I2CBUS is an integer or an I2C bus name\n"
		"  If provided, FIRST and LAST limit the probing range.\n");
}

int main(int argc, char *argv[])
{
	char *end;
	int i2cbus, file, res;
	char filename[20];
	unsigned long funcs;
	int mode = MODE_AUTO;
	int first = 0x03, last = 0x77;
	int flags = 0;
	int yes = 0, version = 0, list = 0;
	/* handle (optional) flags first */
	while (1+flags < argc && argv[1+flags][0] == '-') {
		switch(argv[1+flags][1]) {
		case 'V': version = 1; break;
		case 'y': yes = 1; break;
		case 'l': list = 1; break;
		case 'F':
			if (mode != MODE_AUTO && mode != MODE_FUNC) {
				fprintf(stderr, "Error: Different modes "
					"specified!\n");
				exit(1);
			}
			mode = MODE_FUNC;
			break;
		case 'r':
			if (mode == MODE_QUICK) {
				fprintf(stderr, "Error: Different modes "
					"specified!\n");
				exit(1);
			}
			mode = MODE_READ;
			break;
		case 'q':
			if (mode == MODE_READ) {
				fprintf(stderr, "Error: Different modes "
					"specified!\n");
				exit(1);
			}
			mode = MODE_QUICK;
			break;
		case 'a':
			first = 0x00;
			last = 0x7F;
			break;
		default:
			fprintf(stderr, "Error: Unsupported option "
				"\"%s\"!\n", argv[1+flags]);
			 help();
			 exit(1);
		}
		flags++;
	}	
	return 0;	
}
