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
#include <linux/hidraw.h>
#endif

#include "USB.h"
#include "Types.h"
#include "HexFile.h"

// 1 = print out info relating to usb transfers
#define DEBUG 1

// Set to 1 to enable debug printf statements
// Set to 0 to hide USB packet dumps (file logging still works if DEBUG == 1)
#define DEBUG_PRINT 0

static FILE *packet_log = NULL;
static int packet_counter = 0;

static const int CONTROL_REQUEST_TYPE_IN = LIBUSB_ENDPOINT_IN | LIBUSB_REQUEST_TYPE_CLASS | LIBUSB_RECIPIENT_INTERFACE;
static const int CONTROL_REQUEST_TYPE_OUT = LIBUSB_ENDPOINT_OUT | LIBUSB_REQUEST_TYPE_CLASS | LIBUSB_RECIPIENT_INTERFACE;

// From the HID spec:

static const int HID_GET_REPORT = 0x01;
static const int HID_SET_REPORT = 0x09;
static const int HID_REPORT_TYPE_INPUT = 0x01;
static const int HID_REPORT_TYPE_OUTPUT = 0x02;
static const int HID_REPORT_TYPE_FEATURE = 0x03;

// With firmware support, transfers can be > the endpoint's max packet size.

static const int TIMEOUT_MS = 5000;

// Use interrupt transfers to to write data to the device and receive data from the device.
// Returns - zero on success, libusb error code on failure.
int boot_interrupt_transfers(libusb_device_handle *devh, char *data_in, char *data_out, uint8_t out_only)
{
    // Assumes interrupt endpoint 2 IN and OUT:
    static const int INTERRUPT_IN_ENDPOINT = 0x81;
    static const int INTERRUPT_OUT_ENDPOINT = 0x01;

    // With firmware support, transfers can be > the endpoint's max packet size.
    int bytes_transferred;
    int i = 0;
    int result = 0;

    // Write data to the device.

    result = libusb_interrupt_transfer(
        devh,
        INTERRUPT_OUT_ENDPOINT,
        data_out,
        MAX_INTERRUPT_OUT_TRANSFER_SIZE,
        &bytes_transferred,
        TIMEOUT_MS);

    if (result >= 0 | out_only == 1)
    {
#if DEBUG == 1
        // Open log file on first packet
        if (packet_log == NULL)
        {
            packet_log = fopen("our_packets.txt", "w");
        }
        
        // Log packet to file (same format as dissect file - 128 hex chars per line)
        if (packet_log != NULL)
        {
            // Only log 64 bytes (128 hex chars) to match PCAP format
            int bytes_to_log = (bytes_transferred > 64) ? 64 : bytes_transferred;
            for (i = 0; i < bytes_to_log; i++)
            {
                fprintf(packet_log, "%02x", data_out[i] & 0xff);
            }
            fprintf(packet_log, "\n");
            fflush(packet_log);
        }
#endif
        
#if DEBUG == 2
        //  printf("Data sent via interrupt transfer:\n");
        for (i = 0; i < bytes_transferred; i++)
        {
            printf("%02x ", data_out[i] & 0xff);
        }
        printf("\n");
#endif

        if (out_only > 0)
            return result;

        // Read data from the device.

        result = libusb_interrupt_transfer(
            devh,
            INTERRUPT_IN_ENDPOINT,
            data_in,
            MAX_INTERRUPT_OUT_TRANSFER_SIZE,
            &bytes_transferred,
            TIMEOUT_MS);

        if (result >= 0)
        {
            if (bytes_transferred > 0)
            {
#if DEBUG == 1 && DEBUG_PRINT == 1
                // printf("Data received via interrupt transfer:\n");
                for (i = 0; i < bytes_transferred; i++)
                {
                    printf("%02x ", data_in[i] & 0xff);
                }
                printf("\n");
#endif
            }
            else
            {
                fprintf(stderr, "No data received in interrupt transfer (%d)\n", result);
                return -1;
            }
        }
        else
        {
            fprintf(stderr, "mcu rebooted! %d\n", result); //"Error receiving data via interrupt transfer %d\n", result);
            return result;
        }
    }
    else
    {
        if (out_only != 2)
            fprintf(stderr, "Error sending data via interrupt transfer %d\n", result);
        else
            fprintf(stderr, "Device has been re-booted! %d\n", result);

        return result;
    }
    return 0;
}
