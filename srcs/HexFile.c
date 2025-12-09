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
#include <stdlib.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>

#ifndef _WIN32
#include <linux/types.h>
#include <linux/input.h>
#endif

#include "HexFile.h"
#include "Types.h"
#include "Utils.h"

// 1 = file size |
// 2 = address info |
// 3 = supply the path other than argument |
// 4 = Report hex file size, Memory address allocation, transfer file size
// 6 = print out hex address to ensure iteration is line for line ignoring report type 02 & 04, hex file byte totals
#define DEBUG 0

// Set to 1 to enable debug printf statements (disables progress bar)
// Set to 0 to enable progress bar (disables debug printf)
#define DEBUG_PRINT 0

// boot loader 1st line - jumps back to bootloader at 0xBD1F4000 / 0xBD0F4000
const uint8_t boot_line[][16] = {{0x1F, 0xBD, 0x1E, 0x3C, 0x00, 0x40, 0xDE, 0x37, 0x08, 0x00, 0xC0, 0x03, 0x00, 0x00, 0x00, 0x70},
                                 {0x0F, 0xBD, 0x1E, 0x3C, 0x00, 0x40, 0xDE, 0x37, 0x08, 0x00, 0xC0, 0x03, 0x00, 0x00, 0x00, 0x70}};

const uint32_t _PIC32Mn_STARTFLASH = 0x1D000000;
const uint32_t _PIC32Mn_STARTCONF = 0x1FC00000;
const uint32_t vector[] = {_PIC32Mn_STARTFLASH, _PIC32Mn_STARTFLASH, _PIC32Mn_STARTCONF};

// memory to hold flash data and maintain initial pointers addresses
uint8_t *prg_ptr = 0;
uint8_t *prg_ptr_start = 0;
uint8_t *conf_ptr = 0;
uint8_t *conf_ptr_start = 0;

// Save first instruction from program flash before it gets overwritten
uint8_t first_instruction[4] = {0};

// write program memor variable
uint32_t bootaddress_space = 0;

// keep track of how many bytes have been extracted form each line
uint32_t prg_mem_count = 0;
uint32_t conf_mem_count = 0;

// Progress tracking
static uint32_t total_bytes_to_write = 0;
static uint32_t bytes_written = 0;

// iterate the vector array in state machine
int vector_index = 0;

void overwrite_bootflash_program(void);
uint32_t page_iteration_calc(uint16_t row_page_size, uint32_t mem_quantity);

// Progress bar function
void print_progress_bar(const char *label, uint32_t current, uint32_t total)
{
    const int bar_width = 40;
    float percentage = (float)current / total;
    int filled = (int)(percentage * bar_width);
    
    printf("\r%s: [", label);
    for (int i = 0; i < bar_width; i++)
    {
        if (i < filled)
            printf("=");
        else
            printf(" ");
    }
    printf("] %3.0f%%", percentage * 100.0f);
    fflush(stdout);
    
    if (current >= total)
        printf("\n");
}

/*
 * To get chip into bootloader mode to usb needs to interrupt transfer a sequence of packets
 * Packet A : send [STX][cmdSYNC]
 * Packet B : send [STX][cmdINFO]
 * Packet C : send [STX][cmdBOOT]
 * Packet D : send [STX][cmdSYNC]
 * Find the file to send
 */

/***************************************************
 * Open the hex file extract each line and iterate
 * over the data from each line, the data is ASCII,
 * conver each byte to its binary equivilant,
 * Get the address MSW and LSW then use the address
 * to place the data bytes at the index in the
 * ram buffer, this way the file is only iterated
 * through once.
 * 2 buffers are used
 *  1) program data,
 *  2) configuration data
 ***************************************************/
uint32_t condition_hexfile_data(char *path, TBootInfo *bootinfo)
{
    uint32_t count = 0;
    uint32_t address = 0;
    uint32_t root_address = 0;
    uint32_t prg_min_addr = 0xFFFFFFFF;
    uint32_t prg_max_addr = 0;
    uint32_t conf_min_addr = 0xFFFFFFFF;
    uint32_t conf_max_addr = 0;

    int c_ = 0;
    // temp buffers
    uint8_t line[64] = {0};

    // temp struct of type hex descriptors
    _HEX_ hex = {0};

    // get file size to allocate memory
    FILE *fp = NULL;
    if (fp == NULL)
    {
        fp = fopen(path, "r");
        if (fp == NULL)
        {
            fprintf(stderr, "Could not find or open a file!!\n");
            return 0;
        }
    }

    // need the size ofthe file to allocate memory for linear buffer
    uint32_t size = file_byte_count(fp);

#if DEBUG == 4
    printf("fc = %u\n", size);
#endif

    // allocate memory to prg_ptr to the size of chars in the file,
    // this isn't quite correct as it will over allocate by the
    // size of 12 bytes "[1][4][2][4]....[1]" I may want to save
    // space later by using this value
    prg_ptr = (uint8_t *)malloc(bootinfo->ulMcuSize.fValue);
    memset(prg_ptr, 0xff, bootinfo->ulMcuSize.fValue);
    prg_ptr_start = prg_ptr;

    // allocate memory for configuration data, use size for now,
    // once I know how many bytes are allocated to configuration
    // I can reduce this size.
    conf_ptr = (uint8_t *)malloc(0xffff); // bootinfo->uiWriteBlock.fValue.intVal + 1);
    memset(conf_ptr, 0xff, 0xffff);
    conf_ptr_start = conf_ptr;

    // make sure file starts from begining
    fseek(fp, 0, SEEK_SET);

    // rest the counters if they hold values?
    prg_mem_count = conf_mem_count = 0;

    // iterate through file line by line
    while (c_ != EOF)
    {
        file_extract_line(fp, line, c_);

        // extract byte count and address and report type
        memcpy((uint8_t *)&hex, &line, sizeof(_HEX_));

        hex.report.add_lsw = swap_wordbytes(hex.report.add_lsw);

        // intel hex report type 02 and 04 are Address data types
        if (hex.report.report == 0x02 | hex.report.report == 0x04)
        {
            hex.add_msw = swap_wordbytes(hex.add_msw);
            root_address = transform_2words_long(hex.add_msw, hex.report.add_lsw);
        }
        else if (hex.report.report == 00)
        {
            address = root_address + hex.report.add_lsw;

#if DEBUG == 6 // 6 to output memory address read from hex file
            printf("%08x\n", address);
#endif
            if (address >= _PIC32Mn_STARTFLASH && address < _PIC32Mn_STARTCONF)
            {
                uint32_t temp_prg_add = (address - _PIC32Mn_STARTFLASH);
                uint32_t data_quant = (uint32_t)hex.report.data_quant;
                
                // Write data at exact offset from hex file
                for (uint32_t k = 0; k < data_quant; k++)
                {
                    *(prg_ptr + temp_prg_add + k) = line[k + sizeof(_HEX_REPORT_)];
                }
                
                // Track the full address range: minimum and maximum addresses
                if (temp_prg_add < prg_min_addr)
                    prg_min_addr = temp_prg_add;
                
                uint32_t end_of_record = temp_prg_add + data_quant;
                if (end_of_record > prg_max_addr)
                    prg_max_addr = end_of_record;
            }
            else if (address >= _PIC32Mn_STARTCONF)
            {
                uint32_t temp_add = address - _PIC32Mn_STARTCONF;
                uint32_t data_quant = (uint32_t)hex.report.data_quant;
                
                // Write data at exact offset from hex file
                for (uint32_t k = 0; k < data_quant; k++)
                {
                    *(conf_ptr + temp_add + k) = line[k + sizeof(_HEX_REPORT_)];
                }
                
                // Track the full address range: minimum and maximum addresses
                if (temp_add < conf_min_addr)
                    conf_min_addr = temp_add;
                
                uint32_t end_of_record = temp_add + data_quant;
                if (end_of_record > conf_max_addr)
                    conf_max_addr = end_of_record;
            }
        }

        if (hex.report.report == 0x01)
            break;
    }

    // Calculate the full memory span from minimum to maximum address
    // This includes all gaps filled with 0xFF
    // The buffer was already initialized with 0xFF, and data was written at exact offsets
    // We write from address 0 up to the highest address written
    if (prg_max_addr > 0)
    {
        prg_mem_count = prg_max_addr;  // Total bytes from 0 to highest address
        // Do NOT adjust prg_ptr_start - keep it at the base
    }
    
    if (conf_max_addr > 0)
    {
        conf_mem_count = conf_max_addr;  // Total bytes from 0 to highest address
        // Do NOT adjust conf_ptr_start - keep it at the base
    }

#if DEBUG_PRINT == 1
    // Debug: check if data at 0x0600 was parsed correctly
    printf("Program memory range: 0x%x to 0x%x, total = %u bytes (0x%x)\n", prg_min_addr, prg_max_addr, prg_mem_count, prg_mem_count);
    printf("Config memory range: 0x%x to 0x%x, total = %u bytes (0x%x)\n", conf_min_addr, conf_max_addr, conf_mem_count, conf_mem_count);
    printf("Data at offset 0x0600: %02x %02x %02x %02x %02x %02x %02x %02x\n",
           *(prg_ptr + 0x600), *(prg_ptr + 0x601), *(prg_ptr + 0x602), *(prg_ptr + 0x603),
           *(prg_ptr + 0x604), *(prg_ptr + 0x605), *(prg_ptr + 0x606), *(prg_ptr + 0x607));
#endif

    return size;
}

/*
 * Work engine of bootloader
 *
 * Args: usb_device_handle = from libusb device attach
 *       path = the folder/file path of the hexfile to be loaded
 *
 * return: nothing
 */
void setupChiptoBoot(struct libusb_device_handle *devh, char *path)
{

    // utils
    static int8_t trigger = 0;
    int16_t result = 0;
    uint8_t _out_only = 0;

    // Reset progress counters
    bytes_written = 0;
    total_bytes_to_write = 0;

    // flash size
    uint32_t size = 0;
    uint32_t _temp_flash_erase_ = 0;
    uint32_t _boot_flash_start = 0;
    uint32_t _pages_to_flash = 0;

    uint16_t _blocks_to_flash_ = 0, modulo = 0; //(uint32_t)(size / bootinfo_t.ulMcuSize.fValue);
    double _blocks_temp = 0.0, fractional = 0.0, integer = 0.0, _write_row_error = 0.0;
    TCmd tcmd_t = cmdINFO;
    TBootInfo bootinfo_t = {0};

    // hex loading
    uint32_t load_calc_result = 0;
    uint16_t hex_load_limit = 0;
    uint16_t hex_load_tracking = 0;
    uint16_t hex_load_modulo = 0;
    uint32_t hex_load_page_tracking = 0;

    // file handling
    FILE *fp = NULL;

    // usb specific data
    static char data_in[MAX_INTERRUPT_IN_TRANSFER_SIZE];
    static char data_out[MAX_INTERRUPT_OUT_TRANSFER_SIZE];

    while (tcmd_t != cmdDONE)
    {
        /*
         * main state mc to handle the sequence need by
         * MikroC bootloader firmware, I believe this conforms
         * closely to the UHB standard.
         */
        {
            switch (tcmd_t)
            {

            case cmdSYNC:
            {
                _out_only = 0;
                data_out[0] = 0x0f;
                data_out[1] = (char)cmdSYNC;
                for (int i = 9; i < MAX_INTERRUPT_OUT_TRANSFER_SIZE; i++)
                {
                    data_out[i] = 0x0;
                }
            }
            break;
            case cmdINFO:
            {
                _out_only = 0;
                data_out[0] = 0x0f;
                data_out[1] = (char)cmdINFO;
                for (int i = 2; i < MAX_INTERRUPT_OUT_TRANSFER_SIZE; i++)
                {
                    data_out[i] = 0x0;
                }
            }
            break;
            case cmdBOOT:
            {
                _out_only = 0;
                bootInfo_buffer(&bootinfo_t, data_in);
                data_out[0] = 0x0f;
                data_out[1] = (char)cmdBOOT;
                for (int i = 2; i < MAX_INTERRUPT_OUT_TRANSFER_SIZE; i++)
                {
                    data_out[i] = 0x0;
                }
                // start at address space 1d00
                vector_index = 0;
            }
            break;
            case cmdNON: // A wait state between commands
            {
                // expect a data response back from device
                _out_only = 0;

                // handle address space from vector array, 1st 1d00 then 1fc0
                if (vector_index == 1) // boot startup page
                {
                    prg_ptr = prg_ptr_start; // reset place holder

                    // pre-condition the hex file for bootloading
                    // This fills buffer with 0xFF then places boot vector at end (offset 0x3FF0)
                    overwrite_bootflash_program();

                    prg_ptr = prg_ptr_start;                      // reset place holder
                    size = bootinfo_t.uiEraseBlock.fValue.intVal; // 0x4000

                    // Calculate boot vector location: MCU_SIZE - 0x10000
                    // For MZ1024 (0x100000): 0x1D000000 + 0xF0000 = 0x1D0F0000
                    _boot_flash_start = _PIC32Mn_STARTFLASH + (bootinfo_t.ulMcuSize.fValue - 0x10000);

                    // erase a whole page 0x4000 for boot vector
                    hex_load_limit = (bootinfo_t.uiEraseBlock.fValue.intVal / MAX_INTERRUPT_OUT_TRANSFER_SIZE) - 1;

                    _temp_flash_erase_ = (_boot_flash_start);

#if DEBUG == 4
                    printf("%08x : %08x : %08x\n", vector[vector_index], _boot_flash_start, _temp_flash_erase_);
#endif
                    // pages to flash
                    _blocks_to_flash_ = 1;
                    // write hex data from address
                    bootaddress_space = _boot_flash_start;
                }
                else if (vector_index == 2) // config data
                {
                    // reset place holders to load from the begining
                    prg_ptr = prg_ptr_start;
                    conf_ptr = conf_ptr_start;

                    // Copy ONLY the first instruction (4 bytes) from saved copy (not corrupted buffer)
                    // The PIC32MZ boots from config flash at reset
#if DEBUG_PRINT == 1
                    printf("Using first_instruction: %02x %02x %02x %02x\n", 
                           first_instruction[0], first_instruction[1], first_instruction[2], first_instruction[3]);
#endif
                    memcpy(conf_ptr, first_instruction, 4);
                    
                    // Fill the rest of the first 64 bytes with nop instructions
                    uint32_t nop = 0x70000000;  // nop instruction (little-endian: 0x00 0x00 0x00 0x70)
                    for (int i = 1; i < 16; i++)
                    {
                        memcpy(conf_ptr + (i * 4), &nop, 4);
                    }
                    
                    // After first 64 bytes, add the boot vector (jumps to bootloader at BD0F4000)
                    uint8_t boot_vector[16] = {
                        0x0F, 0xBD, 0x1E, 0x3C,  // lui $30, 0xBD0F
                        0x00, 0x40, 0xDE, 0x37,  // ori $30, $30, 0x4000
                        0x08, 0x00, 0xC0, 0x03,  // jr $30
                        0x00, 0x00, 0x00, 0x70   // nop (delay slot)
                    };
                    memcpy(conf_ptr + 64, boot_vector, 16);
                    
                    // For config flash, we'll use conf_ptr directly in load_hex_buffer
                    // Don't copy to prg_ptr - that would overwrite program flash data!

                    // Config flash write is 0x1800 (6144 bytes) = 3 write blocks = 96 packets
                    // hex_load_limit = (0xffff + 1) / MAX_INTERRUPT_OUT_TRANSFER_SIZE;
                    hex_load_limit = ((bootinfo_t.uiWriteBlock.fValue.intVal * 3) / MAX_INTERRUPT_OUT_TRANSFER_SIZE) - 1;

                    // set the start address to flash erase
                    _temp_flash_erase_ = (vector[vector_index]);

                    // set erase block to multiple pages of data [1page = 0x4000 for mz]
                    //_blocks_to_flash_ = (0xffff + 1) / 0x4000;
                    _blocks_to_flash_ = 1;

                    // set the write hex data address space
                    bootaddress_space = vector[vector_index];
                }
                else // program flash region
                {
                    // open hexx file read it line for line and extract the data according
                    //  to the address, buffer offset is indexed by address
                    size = condition_hexfile_data(path, &bootinfo_t);

                    // Save first instruction before it gets overwritten by boot vector processing
                    memcpy(first_instruction, prg_ptr_start, 4);
                    
#if DEBUG_PRINT == 1
                    printf("Saved first_instruction: %02x %02x %02x %02x\n", 
                           first_instruction[0], first_instruction[1], first_instruction[2], first_instruction[3]);
#endif

                    // reset place holder
                    prg_ptr = prg_ptr_start;

                    // hex page tracking works out how many pages will be loaded into PFM 1 page at a time
                    // bootload firmware has 16bit int so can't load more than 0x8000 bytes at a time
                    // calculate size of erasing preperation
                    _pages_to_flash = page_iteration_calc(bootinfo_t.uiEraseBlock.fValue.intVal, prg_mem_count);

                    // set how many iterations of 64byte packets to send over wire with row boundry
                    //_write_row_error = (double)prg_mem_count / (double)bootinfo_t.uiWriteBlock.fValue.intVal;
                    // fractional = modf(_write_row_error, &integer);
                    // load_calc_result = (fractional > 0.0) ? 1 : 0;
                    // load_calc_result += (uint32_t)integer;
                    if (_pages_to_flash == 1)
                    {
                        load_calc_result = page_iteration_calc(bootinfo_t.uiWriteBlock.fValue.intVal, prg_mem_count);
                        prg_mem_count = bootinfo_t.uiWriteBlock.fValue.intVal * load_calc_result;

                        load_calc_result = (prg_mem_count / MAX_INTERRUPT_OUT_TRANSFER_SIZE); // + load_calc_result;
                        hex_load_limit = load_calc_result - 1;                                // size / MAX_INTERRUPT_OUT_TRANSFER_SIZE;
#if DEBUG == 2
                        printf("[%u] : [%02f] [%02f] [%u] [%u]\n", _pages_to_flash, _write_row_error, integer, load_calc_result, prg_mem_count);
#endif
                    }
                    else
                    {
                        // load the full page into the chip
                        hex_load_limit = (bootinfo_t.uiEraseBlock.fValue.intVal - MAX_INTERRUPT_OUT_TRANSFER_SIZE) / MAX_INTERRUPT_OUT_TRANSFER_SIZE;
                    }

                    printf("%u : %u : %u : %d\n", _pages_to_flash, prg_mem_count, load_calc_result, _blocks_to_flash_);

                    // erase at least 1 page if there are zero blocks to flash.
                    _blocks_to_flash_ = _pages_to_flash;
                    if (_blocks_to_flash_ == 0)
                        _blocks_to_flash_ = 1;

                    bootaddress_space = vector[vector_index];

                    _temp_flash_erase_ = (vector[vector_index]); // Start address for erase, not end
                }

#if DEBUG == 4
                printf("trnsfer size:= %d\n", size);
#endif

                if (size > 0)
                {
                    trigger = 1;
                    // reset flash pointer to start
                    prg_ptr = prg_ptr_start;
                }
                else
                {
                    // no point in continuing if the file is empty
                    exit(EXIT_FAILURE);
                }

#if DEBUG == 3
                printf("vector indexed at [%02x]\n", vector_index);
#elif DEBUG == 4
                printf("bootaddress_space [%08x]\tflash erase start [%08x]\tblock to flash [%04x]\n", bootaddress_space, _temp_flash_erase_, _blocks_to_flash_);
#endif
            }
            break;
            case cmdERASE:
            {
                // expect a data response back from device
                _out_only = 0;
                // bootloader needs startaddress "page boundry" and quantity of pages to to erase
                // erase for MikroC starts high and subracts from quantity after each page has
                // been erased and quantity == 0
                data_out[0] = 0x0f;
                data_out[1] = (char)cmdERASE;
                memcpy(data_out + 2, &_temp_flash_erase_, sizeof(uint32_t));
                memcpy(data_out + 6, &_blocks_to_flash_, sizeof(int16_t));
                for (int i = 9; i < MAX_INTERRUPT_OUT_TRANSFER_SIZE; i++)
                {
                    data_out[i] = 0x0;
                }
            }
            break;
            case cmdWRITE:
            {
                // expect no data back continously stream data.
                _out_only = 1;

                if (vector_index == 2)
                {
                    size = bootinfo_t.uiWriteBlock.fValue.intVal * 3; // 0x1800 (6144 bytes) - three write blocks for config
                }
                else if (vector_index == 1)
                {
                    size = bootinfo_t.uiEraseBlock.fValue.intVal; // 0x4000 - full boot vector page
                }
                else // Program flash - send total size for ALL data, not per-page
                {
                    size = prg_mem_count; // Total program flash data size
                }

                hex_load_tracking = 0;
                data_out[0] = 0x0f;
                data_out[1] = (char)cmdWRITE;
                memcpy(data_out + 2, &bootaddress_space, sizeof(uint32_t));
                memcpy(data_out + 6, &size, sizeof(int16_t));
                for (int i = 9; i < MAX_INTERRUPT_OUT_TRANSFER_SIZE; i++)
                {
                    data_out[i] = 0x0;
                }

                // Calculate total packets to send for this region (all pages at once)
                hex_load_limit = (size / MAX_INTERRUPT_OUT_TRANSFER_SIZE) - 1;

                // Accumulate total for progress tracking
                total_bytes_to_write += size;

                // Reset the pointer position
                prg_ptr = prg_ptr_start;
            }
            break;
            case cmdHEX:
            {
                // expect no data back continously stream data.
                _out_only = 1;

                hex_load_tracking++;

                // use the flash buffer to stream 64 byte slices at a time
                if (hex_load_tracking > hex_load_limit)
                {
                    // All data for this region has been sent, move to next region
                    tcmd_t = cmdREBOOT;
                    _out_only = 0;
                }

                load_hex_buffer(data_out, MAX_INTERRUPT_OUT_TRANSFER_SIZE);
            }
            break;
            case cmdREBOOT:
            {
                _out_only = 2;

#if DEBUG == 0
                printf("%u : %u\n", prg_mem_count, conf_mem_count);
#endif

                // free memory created for flas_pointer

                /*
                 * re-boot command will cause the app to exit due to timeout from
                 * usb response, may want to set _out_only to 1 to sto exception.
                 * extra handling of usb may be needed if _out_only set to 1.
                 */

                vector_index++;
                if (vector_index > 2)
                {
                    if (prg_ptr != NULL)
                    {
                        prg_ptr = prg_ptr_start;
                        // free(flash_ptr);
                        // flash_ptr = flash_ptr_start = NULL;
                    }
                    data_out[0] = 0x0f;
                    data_out[1] = (char)cmdREBOOT;
                    for (int i = 2; i < MAX_INTERRUPT_OUT_TRANSFER_SIZE; i++)
                    {
                        // prg_ptr = prg_ptr_start;
                        data_out[i] = 0x0;
                    }
                    // After sending final reboot command, we'll exit the loop
                    // This will be set to cmdDONE after the USB transfer completes
                }
                else
                {
                    // start back at data prep for config vector
                    tcmd_t = cmdNON;
                }
            }
            break;
            default:
                break;
            }
        }
        // Sendin the data via usb
        if (tcmd_t != cmdNON && !(tcmd_t == cmdREBOOT && _out_only == 1))
        {
            if (boot_interrupt_transfers(devh, data_in, data_out, _out_only))
            {
                fprintf(stderr, "Transfered data complete...\n");
                exit(EXIT_FAILURE);
            }
        }

        /*
         * extra state machine to help sequencer? this may be taken away in
         * for refinement.
         */
        {
            switch (tcmd_t)
            {
            case cmdINFO:
                tcmd_t = cmdBOOT;
                break;
            case cmdBOOT:
                tcmd_t = cmdNON;
                break;
            case cmdNON:
                if (trigger == 1)
                {
                    if (vector_index == 0)
                        tcmd_t = cmdSYNC;
                    else
                        tcmd_t = cmdERASE;
                    trigger = 0;
                }
                break;
            case cmdSYNC:
                tcmd_t = cmdERASE;
#if DEBUG_PRINT == 1
                printf("Erase\n");
#endif
                break;
            case cmdERASE:
                tcmd_t = cmdWRITE /*cmdREBOOT*/;
#if DEBUG_PRINT == 1
                printf("Write\n");
#endif
                break;
            case cmdWRITE:
                tcmd_t = cmdHEX;
#if DEBUG_PRINT == 1
                printf("HEX\n");
#endif
                break;
            case cmdHEX:

                break;
            case cmdREBOOT:
                // Only exit loop when vector_index > 2 (final reboot sent)
                // If vector_index <= 2, cmdREBOOT sets tcmd_t = cmdNON to continue
                if (vector_index > 2)
                {
                    tcmd_t = cmdDONE;
                }
                break;
            default:
                break;
            }
        }
    }
}

/*Display the boot info need for erase and write data*/
void bootInfo_buffer(void *boot_info, const void *buffer)
{
    TBootInfo *bootinfo_t = boot_info;
    uint8_t *data;
    bootinfo_t->bSize = *((uint8_t *)buffer + 0);
    memcpy(((uint8_t *)bootinfo_t + 1), ((uint8_t *)buffer + 1), sizeof(TCharField));
    memcpy(((uint8_t *)bootinfo_t + 3), ((uint8_t *)buffer + 4), sizeof(TULongField));
    memcpy(((uint8_t *)bootinfo_t + 11), ((uint8_t *)buffer + 12), sizeof(TUIntField));
    memcpy(((uint8_t *)bootinfo_t + 15), ((uint8_t *)buffer + 16), sizeof(TULongField));
    memcpy(((uint8_t *)bootinfo_t + 19), ((uint8_t *)buffer + 20), sizeof(TULongField));
    memcpy(((uint8_t *)bootinfo_t + 23), ((uint8_t *)buffer + 24), sizeof(TULongField));
    memcpy(((uint8_t *)bootinfo_t + 31), ((uint8_t *)buffer + 32), sizeof(TStringField));

#if DEBUG_PRINT == 1
    printf("\n%02x\n%02x\t%02x\n%02x\t%08x\n%02x\t%04x\n%02x\t%04x\n%02x\t%04x\n%02x\t%08x\n%02x\t%s\n\n", bootinfo_t->bSize, bootinfo_t->bMcuType.fFieldType, bootinfo_t->bMcuType.fValue, bootinfo_t->ulMcuSize.fFieldType, bootinfo_t->ulMcuSize.fValue, bootinfo_t->uiEraseBlock.fFieldType, bootinfo_t->uiEraseBlock.fValue.intVal, bootinfo_t->uiWriteBlock.fFieldType, bootinfo_t->uiWriteBlock.fValue.intVal, bootinfo_t->uiBootRev.fFieldType, bootinfo_t->uiBootRev.fValue.intVal, bootinfo_t->ulBootStart.fFieldType, bootinfo_t->ulBootStart.fValue, bootinfo_t->sDevDsc.fFieldType, bootinfo_t->sDevDsc.fValue);
#endif
}

/*
 * @param uint32_t size
 *
 * Stream the data 64 byte slices using
 *
 * return none
 */
void load_hex_buffer(char *data, uint16_t iterable)
{
    uint32_t i = 0;
    
    // Use conf_ptr for config flash (vector_index == 2), prg_ptr for everything else
    if (vector_index == 2)
    {
        for (i = 0; i < iterable; i++)
        {
            *(data + i) = *(conf_ptr++);
        }
    }
    else
    {
        for (i = 0; i < iterable; i++)
        {
            *(data + i) = *(prg_ptr++);
        }
    }
    
    // Update progress
    bytes_written += iterable;
#if DEBUG_PRINT == 0
    if (total_bytes_to_write > 0)
    {
        print_progress_bar("Programming", bytes_written, total_bytes_to_write);
    }
#endif
}

/*
 * Utils
 */

uint32_t file_byte_count(FILE *fp)
{
    uint32_t size = 0;

    fseek(fp, 0, SEEK_SET);

    for (size = 0; getc(fp) != EOF; size++)
        ;

    return size;
}

void file_extract_line(FILE *fp, char *buf, int fp_result)
{
    int i = 0, j = 0;
    char c;
    uint8_t temp_[3] = {0};

    while (fp_result = fgetc(fp))
    {

        c = (unsigned char)fp_result;

        // make sure we dont capture new line
        if (c == '\n')
        {
            break;
        }

        // start char of a new line in a hex file is always a ':'
        if (c == ':')
        {
            continue;
        }

        // extract each ascii char from hex file line and convert to bin data
        temp_[j++] = transform_char_bin(c);

        if (j > 1)
        {
            *(buf + i) = transform_2chars_1bin(temp_);
            j = 0;
#if DEBUG == 4
            printf("[%02x] ", buf[i]);
#endif
            i++;
        }
    }
}

void overwrite_bootflash_program(void)
{
    int i = 0;
    // Default PIC32 boot vector - jumps to 0xBFC00050 (default boot flash)
    uint8_t default_boot_vector[16] = {
        0xC0, 0xBF, 0x1E, 0x3C,  // lui $30, 0xBFC0
        0x50, 0x00, 0xDE, 0x37,  // ori $30, $30, 0x0050
        0x08, 0x00, 0xC0, 0x03,  // jr $30
        0x00, 0x00, 0x00, 0x70   // nop (delay slot)
    };
    
    // Fill entire boot vector page with 0xFF
    for (i = 0; i < (0x4000 - 16); i++)
    {
        *(prg_ptr++) = 0xff;
    }
    
    // Place default boot vector at end (offset 0x3FF0)
    memcpy(prg_ptr, default_boot_vector, 16);
}

uint32_t page_iteration_calc(uint16_t row_page_size, uint32_t mem_quantity)
{
    double tempA, fractional, integer;
    uint32_t result = 0;
    tempA = (double)mem_quantity / (double)row_page_size;
    fractional = modf(tempA, &integer);
    result = (fractional > 0.0) ? 1 : 0;
    result += (uint32_t)integer;

    return result;
}
