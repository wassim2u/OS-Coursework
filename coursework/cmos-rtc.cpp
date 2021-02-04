/*
 * CMOS Real-time Clock
 * SKELETON IMPLEMENTATION -- TO BE FILLED IN FOR TASK (1)
 */

/*
 * STUDENT NUMBER: s1870697
 */
#include <infos/drivers/timer/rtc.h>
#include <arch/x86/pio.h>
#include <infos/util/lock.h>
#include <infos/kernel/log.h>
#include <infos/assert.h>

using namespace infos::drivers;
using namespace infos::drivers::timer;
using namespace infos::util;
using namespace infos::arch::x86;
using namespace infos::kernel;

//Define the necessary offsets, which will be used later to activate a port in order to read from the CMOS registers
enum CMOS_REGISTERS{
seconds_offset = 0x00,
 minutes_offset = 0x02,
 hours_offset = 0x04,
 day_of_month_offset = 0x07,
 month_offset = 0x08,
 year_offset = 0x09,
 register_a_offset = 0x0A,
 register_b_offset = 0x0B
};

/**
* The port numbers to be used in order to access the CMOS memory through I/O space.
* The cmos_address is the port used to select the register we are interested in.
* Once the memory offset for the byte you are interested in is written, then the value is read in port defined in cmos_data
*/
enum PORTS{
	cmos_address= 0x70,
	cmos_data = 0x71
};

/**
* Returns the 7th bit of the status register A in CMOS.
* This update-in-progress flag is set when an update is in progress, and cleared if not.
*/
uint8_t get_update_in_progress_flag(){
	__outb(cmos_address,register_a_offset);
	uint8_t v = __inb(cmos_data);
	uint8_t bit = (v>>7) & 0x01;
	return bit;
}


/**
* The function returns the data stored in a register after it is activated given an offset.
* The memory offset is first written to port 0x70, and then read from port 0x71. 
* @param offset The offset to be written to the cmos_address port
*/
unsigned char read_byte(int offset){
	__outb(cmos_address,offset);
	return __inb(cmos_data);
}

/**
* Converts from BCD to Binary. 
* Code Explanation: 
* (unit & 0x0F) preserves the least (right-most or first) four bits. 
* (unit/16)*10 changes the numbers so they are in binary format when added to the result returned by (unit & 0x0F).
* Note: The following BCD code has been taken from https://wiki.osdev.org/CMOS 
* @param unit The time or date that is in BCD Format, except hours. The conversion for hours is done in a seperate function. BCD_to_Binary_hours
*/
short BCD_to_Binary(short unit){
	return (unit & 0x0F) + ((unit/ 16) * 10);
}

/**
* Converts from BCD to Binary for hours. 
* Code Explaination: 
* Similar to the BCD_to_Binary function, but using the OR operator with (hours & 0x80) preserves the PM bit in case 
* the format is in 12 hour. In addition, hours*0x70 preserves the bits in positions 5,6,7 of a byte.
* Together, this gives us the conversion to binary.
* Note: The following BCD code has been taken from https://wiki.osdev.org/CMOS 
* @param hours The hours in BCD format.
*/
short BCD_to_Binary_hours(short hours){
	return ( (hours & 0x0F) + (((hours & 0x70) / 16) * 10) ) | (hours & 0x80);

}




/**
* Helper function that changes the hour format from 12 to 24 hour format.
* Code explaination: 
* 0x7F is (0111 1111) in binary, and note that when an hour is in pm, the 8th bit is set. 
* So, what the function does is it removes the "pm" bit by the logical operator AND(&) on 0x7F and hours, so every bit 
* that was set will return 1 (won't lose any values except remove "pm"). 
* Then, it will add 12 and modulo that number by 24 (in case it is 0) to return the number in 24 hour format (eg. 1:00 PM will become 13:00 by addition of 12).
* @param hours the hour measurement to be converted to 24 hour format. 
*/
unsigned short change_12_to_24_hr(short hours){
	return ((hours & 0x7F) + 12) % 24;
}



class CMOSRTC : public RTC {
public:
	static const DeviceClass CMOSRTCDeviceClass;

	const DeviceClass& device_class() const override
	{
		return CMOSRTCDeviceClass;
	}



	/**
	 * Interrogates the RTC to read the current date & time.
	 * @param tp Populates the tp structure with the current data & time, as
	 * given by the CMOS RTC device.
	 */
	void read_timepoint(RTCTimePoint& tp) override
	{
		// Before accessing the RTC, we first disable the interrupts in order to be able to read the time and date safely.
		// By safe, we mean that we can read without any process interruption and any violation.
		// In addition, this is to allow the hardware time we store to be accurately reflected when we call /usr/date.
		// There will be no need to handle any errors of resource leaks as the RAII wrapper helps deconstruct the l variable at the end of the lifetime of the function.
		UniqueIRQLock l;

		uint8_t status_bit = get_update_in_progress_flag();
		// If the flag is set, meaning an update is in progress, we will wait until it is cleared first before we consider when to read the registers.
		if (status_bit ==1){
			while (status_bit==1){
				status_bit = get_update_in_progress_flag();
			}
		}
		// Before reading any CMOS registers, we will check if the status bit is 0. (No update cycle is in progress)
		// In order to safely read, we will wait for an update to begin.
		while (status_bit==0){
			status_bit = get_update_in_progress_flag();
		}
		// Once an update is in progress, we will keep waiting till this update cycle is over.
		while (status_bit==1){
			status_bit = get_update_in_progress_flag();
		}
		// Finally, once the status bit is cleared, we will start reading from RTC.
		short seconds = read_byte(seconds_offset);
		short minutes = read_byte(minutes_offset);
		short hours = read_byte(hours_offset);
		short day_of_month = read_byte(day_of_month_offset);
		short month = read_byte(month_offset);
		short year = read_byte(year_offset);
		short registerB = read_byte(register_b_offset);

		// If the 2nd bit in status register B is cleared, then the values are in binary coded decimal (BCD).
		// If that is the case, then we must convert them into binary. 
		if (!(registerB & 0x04)) {
			seconds = BCD_to_Binary(seconds);
			minutes = BCD_to_Binary(minutes);
			hours = BCD_to_Binary_hours(hours);
			day_of_month = BCD_to_Binary(day_of_month);
			month = BCD_to_Binary(month);
			year = BCD_to_Binary(year);
		}

		// If the hour is pm, then 0x80 bit is set on the hour byte(the 8th bit is set). In addition, registerB in the 1st bit will be set.
		// In case both of the conditions above is statisfied, then we will need to change from a 12 hour format to a 24 hour format.
		if (!(registerB & 0x02) && (hours & 0x80)) {
				hours = change_12_to_24_hr(hours);
		}
		
		// Set the tp structure with the current date and time
		tp.seconds = seconds;
		tp.minutes= minutes;
		tp.hours = hours;
		tp.day_of_month = day_of_month;
		tp.month = month;
		tp.year=year;
	
		
	}
};

const DeviceClass CMOSRTC::CMOSRTCDeviceClass(RTC::RTCDeviceClass, "cmos-rtc");

RegisterDevice(CMOSRTC);
