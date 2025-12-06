/*
 * generic_hid.c
 *
 *  Created on: Apr 22, 2011
 *      Author: Jan Axelson
 *
 * Demonstrates communicating with a device designed for use with a generic HID-class USB device.
 * Sends and receives 2-byte reports.
 * Requires: an attached HID-class device that supports 2-byte
 * Input, Output, and Feature reports.
 * The device firmware should respond to a received report by sending a report.
 * Change VENDOR_ID and PRODUCT_ID to match your device's Vendor ID and Product ID.
 * See Lvr.com/winusb.htm for example device firmware.
 * This firmware is adapted from code provided by Xiaofan.
 * Note: libusb error codes are negative numbers.

The application uses the libusb 1.0 API from libusb.org.
Compile the application with the -lusb-1.0 option.
Use the -I option if needed to specify the path to the libusb.h header file. For example:
-I/usr/local/angstrom/arm/arm-angstrom-linux-gnueabi/usr/include/libusb-1.0

 */

// OS Detection
#if defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__)
    #ifndef _WIN32
        #define _WIN32
    #endif
#elif defined(__linux__)
    #ifdef _WIN32
        #undef _WIN32
    #endif
#endif

#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <assert.h>
#include <stdint.h>

#ifndef _WIN32
#include <linux/types.h>
#include <linux/input.h>
#endif

// Values for bmRequestType in the Setup transaction's Data packet.
#include "Types.h"
#include "HexFile.h"
#include "Utils.h"
#include "USB.h"

const int INTERFACE_NUMBER = 0;

int main(int argc, char **argv)
{
	// Change these as needed to match idVendor and idProduct in your device's device descriptor.
	libusb_device ***list_;
	static const int VENDOR_ID = 0x2dbc;
	static const int PRODUCT_ID = 0x0001;

	struct libusb_device_handle *devh = NULL;
	struct libusb_init_option *opts = {0};
	int device_ready = 0;
	int result = 0;
	// path to file
	char _path[250] = {0};

	// condition file path
	if (argc < 2)
	{
		fprintf(stderr, "No path to hex!\n");
		return 0;
	}
	else
	{
		size_t len_s = strlen(argv[1]);

#if DEBUG == 5
		printf("\nEnter the path of the hex file... ");
		fgets(path, sizeof(path), stdin);
#endif

		// remove the carriage return and or newline feed from path if it exists
		if (len_s > 0 && (argv[1][len_s - 1] == '\r' || argv[1][len_s - 1] == '\n'))
		{
			argv[1][len_s - 1] = '\0'; // remove \r | \n
		}
		strcpy(_path, argv[1]);
		// show the path? sanity check.
		printf("\t*** %s ***\n", _path);
	}

	result = libusb_init_context(NULL, NULL, 0);

	if (result >= 0)
	{

		devh = libusb_open_device_with_vid_pid(NULL, VENDOR_ID, PRODUCT_ID);
		fprintf(stderr, "devh:=  VID%x:PID%x\n", VENDOR_ID, PRODUCT_ID);

		if (devh != NULL)
		{
			// The HID has been detected.
			// Detach the hidusb driver from the HID to enable using libusb.
			// Note: This is Linux-specific and not needed on Windows
#ifndef _WIN32
			libusb_detach_kernel_driver(devh, INTERFACE_NUMBER);
#endif
			{
				result = libusb_claim_interface(devh, INTERFACE_NUMBER);
				if (result >= 0)
				{
					device_ready = 1;
				}
				else
				{
					fprintf(stderr, "libusb_claim_interface error %d\n", result);
				}
			}
		}
		else
		{
			fprintf(stderr, "Unable to find the device.\n");
		}
	}
	else
	{
		fprintf(stderr, "Unable to initialize libusb.\n");
	}

	if (device_ready)
	{
		// Send and receive data.
		// exchange_input_and_output_reports_via_interrupt_transfers(devh);
		// exchange_input_and_output_reports_via_control_transfers(devh);
		// exchange_feature_reports_via_control_transfers(devh);
		setupChiptoBoot(devh, _path);
		// Finished using the device.
		libusb_release_interface(devh, 0);
	}
	libusb_close(devh);
	libusb_exit(NULL);
	return 0;
}
