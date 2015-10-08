/*  (The MIT License)
 *
 * Copyright (c) 2014 Arran Cudbard-Bell <a.cudbardb@freeradius.org>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the 'Software'), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED 'AS IS', WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

/*
 * Link with libusb >= 1.0.0 (which uses the newer style api)
 *
 * gcc -Wall button.c -lusb-1.0.0 -I /usr/include/libusb-1.0/
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include <unistd.h>
#include <signal.h>

#include <sys/stat.h>
#include <sys/types.h>

#include <dirent.h>

#include <libusb.h>

static bool debug = false;

#define DEBUG(_fmt, ...) if (debug) fprintf(stdout, "brb: " _fmt "\n", ## __VA_ARGS__)
#define INFO(_fmt, ...) fprintf(stdout, "brb: "_fmt "\n", ## __VA_ARGS__)
#define ERROR(_fmt, ...) fprintf(stderr, "brb: " _fmt "\n", ## __VA_ARGS__)

typedef enum button_state {
	BUTTON_STATE_ERROR = -1,	//!< Error occurred getting button state
	BUTTON_STATE_UNKNOWN = 0,
	BUTTON_STATE_LID_CLOSED = 0x15,	//!< Button lid is closed
	BUTTON_STATE_PRESSED = 0x16,	//!< Button is currently depressed (and lid is open)
	BUTTON_STATE_LID_OPEN = 0x17	//!< Button lid is open
} button_states_t;

#define BRB_VID 0x1d34			//!< Big Red Button Vendor ID
#define BRB_PID 0x000d			//!< Big Red Button Product ID
#define BRB_POLL_INTERVAL 20000		//!< How long we wait in between polling the button

static char const *progname;		//!< What the binary's called
static bool should_exit = false;	//!< If true, break out of the poll loop.

static bool kernel_was_attached;

/** Find the device, and detach the kernel driver
 *
 */
static struct libusb_device_handle *get_button_handle(void){
	struct libusb_device_handle *handle = NULL;
	int ret;

	DEBUG("Attempting to open device (vendor 0x%04x, device 0x%04x)", BRB_VID, BRB_PID);
	handle = libusb_open_device_with_vid_pid(NULL, BRB_VID, BRB_PID);
	if (!handle) {
		ERROR("Failed opening device descriptor (you may need to be root)...");
		return NULL;
	}

	/* If the kernel driver is active, we need to detach it */
	if (libusb_kernel_driver_active(handle, 0)) {
		DEBUG("Kernel driver active, attempting to detach...");
	        ret = libusb_detach_kernel_driver(handle, 0);
	        if (ret < 0 ){
			ERROR("Can't detach kernel driver");
	                return NULL;
	        }

	        kernel_was_attached = true;
	 }

	 return handle;
}


static int set_button_control(struct libusb_device_handle *handle)
{
	int ret;
	uint8_t state[8] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02 };

	ret = libusb_control_transfer(handle, 0x21, 0x09, 0x00, 0x00, state, 8, 0);
	if (ret < 0) {
		ERROR("Error reading response %i", ret);
		return -1;
	}
	if (ret == 0) {
		ERROR("Device didn't send enough data");
		return -1;
	}

	return 0;
}

/** Returns the current button state
 *
 */
static button_states_t read_button_state(struct libusb_device_handle *handle) {

	uint8_t data[8] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
	int dev[8];
	int ret;

	ret = set_button_control(handle);
	if (ret < 0) {
		return -1;
	}

	/* Send 0x81 to the EP to retrieve the state */
	ret = libusb_interrupt_transfer(handle, 0x81, data, 8, dev, 200);
	if (ret < 0) {
		ERROR("Error getting interrupt data");
		return -1;
	}

	return data[0];
}

void run_command(char const *cmd) {
	int ret;

	DEBUG("Running command: %s", cmd);
	ret = system(cmd);
	DEBUG("Command returned %i", ret);
}

/** Cleanup gracefully
 *
 */
void exit_handler(int sig_num);	/* Prototype for signal */
void exit_handler(int sig_num)
{
	signal(SIGINT, exit_handler);
	should_exit = true;
}

static void usage(int status)
{
	FILE *output = status ? stderr : stdout;

	fprintf(output, "Usage: %s [options]\n", progname);
	fprintf(output, "  -P <microsends>   Polling interval\n");
	fprintf(output, "  -o <command>      Command to execute when button lid is open.\n");
	fprintf(output, "  -c <command>      Command to execute when button lid is closed.\n");
	fprintf(output, "  -p <command>      Command to execute when button is pressed.\n");
	fprintf(output, "  -r <command>      Command to execute when button is released.\n");
	fprintf(output, "  -h                This help text.\n");
	fprintf(output, "  -v                Turn on verbose output.\n");

	exit(status);
}

void take_picture(){
	INFO("Preparing directory");
	char const * dir = "/tmp/picture";
	int result = mkdir(dir, 0777);
	chdir(dir);

	// take picture
	system("chdkptp -ec -e\"rec\" -e\"rs\"");

	// look for the jpg
	DIR *hdir;
	struct dirent *ent;
	if ((hdir = opendir(dir)) != NULL) {
  		/* print all the files and directories within directory */
  		while ((ent = readdir (hdir)) != NULL) {
    			printf ("%s\n", ent->d_name);
  		}	
  		closedir (hdir);
	} 
	INFO("Finished with picture");
}

/** Main program
 *
 */
int main (int argc, char *argv[]) {
	char c;

	char const *cmd_lid_open = NULL;
	char const *cmd_lid_closed = NULL;
	char const *cmd_pressed = NULL;
	char const *cmd_released = NULL;

	int interval = BRB_POLL_INTERVAL;

	struct libusb_device_handle *handle = NULL;
	button_states_t then = BUTTON_STATE_UNKNOWN, now;

	progname = argv[0];

	while ((c = getopt(argc, argv, "P:o:c:p:r:hv")) != EOF) {
		switch (c) {
		case 'P':
			interval = atoi(optarg);
			break;

		case 'o':
			cmd_lid_open = optarg;
			break;

		case 'c':
			cmd_lid_closed = optarg;
			break;

		case 'p':
			cmd_released = optarg;
			break;

		case 'r':
			cmd_pressed = optarg;
			break;

		case 'h':
			usage(0);
			break;

		case 'v':
			debug = true;
			break;
		}
	}

	/* Setup a signal handler, so we can cleanup gracefully */
	signal(SIGINT, exit_handler);

	/* Initialise libusb (with the default context) */
	libusb_init(NULL);

#if defined(LIBUSB_LOG_LEVEL_DEBUG) && defined(LIBUSB_LOG_LEVEL_ERROR)
	/* All the debugging messages !*/
	if (debug) {
		libusb_set_debug(NULL, LIBUSB_LOG_LEVEL_DEBUG);
	/* We still want to know about errors (helps with debugging) */
	} else {
		libusb_set_debug(NULL, LIBUSB_LOG_LEVEL_ERROR);
	}
#endif

	/* If we can't get the handle, exit... */
	handle = get_button_handle();
	if (!handle) exit(1);

	if (read_button_state(handle) == BUTTON_STATE_ERROR) goto finish;

	/* Loop, polling the device to get it's status */
	while (should_exit != true) {
		now = read_button_state(handle);
		if (now == BUTTON_STATE_ERROR) goto skip;
		if (then == now) goto skip;

		if (then == BUTTON_STATE_PRESSED) {
			/* Run the released action */
			INFO("RELEASED");
			take_picture();
			if (cmd_released) run_command(cmd_released);

			/* Weird but I guess it could happen... */
			if (now == BUTTON_STATE_LID_CLOSED) goto closed;
			goto next;
		}

		switch (now) {
		case BUTTON_STATE_PRESSED:
			/* Run the pressed action */
			INFO("PRESSED");
			if (cmd_pressed) run_command(cmd_pressed);
			break;

		case BUTTON_STATE_LID_OPEN:
			/* Run the lid open action */
			INFO("LID_OPEN");
			if (cmd_lid_open) run_command(cmd_lid_open);
			break;

		case BUTTON_STATE_LID_CLOSED:
		closed:
			/* Run the closed action */
			INFO("LID_CLOSED");
			if (cmd_lid_closed) run_command(cmd_lid_closed);
			break;

		default:
			goto skip;
		}

	next:
		then = now;
	skip:
		usleep(interval);
	}

finish:
	DEBUG("Exiting...");

	fflush(stdout);
	fflush(stderr);

//	if (kernel_was_attached) {
//		libusb_attach_kernel_handle(ghandle, 0);
//	}

	libusb_close(handle);

	exit(0);
}
