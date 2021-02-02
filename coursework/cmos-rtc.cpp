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

//The port numbers to be used in order to access the CMOS memory through I/O space.
//The cmos_address is the port used to select the register we are interested in.
//Once the memory offset for the byte you are interested in is written, then the value is read in port defined in cmos_data
enum PORTS{
	cmos_address= 0x70,
	cmos_data = 0x71
};

//Returns the 7th bit of the status register A in CMOS.
//This update-in-progress flag is set when an update is in progress, and cleared if not.
uint8_t get_update_in_progress_flag(){
	__outb(cmos_address,register_a_offset);
	uint8_t v = __inb(cmos_data);
	uint8_t bit = (v>>7) & 0x01;
	return bit;
}


//The function returns the data stored in a register after it is activated given an offset.
//The memory offset is first written to port 0x70, and then read from port 0x71. 
unsigned char read_byte(int offset){
	__outb(cmos_address,offset);
	return __inb(cmos_data);
}

//Helper function that changes the hour format from 12 to 24 hour format.
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
		//Before accessing the RTC, we first disable the interrupts in order to be able to read the time and date safely.
		//By safe, we mean that we can read without any process interruption and any violation.
		//In addition, this is to allow the hardware time we store to be accurately reflected when we call /usr/date.
		UniqueIRQLock var;
		syslog.messagef(LogLevel::DEBUG, "Interrupts Disabled");

		uint8_t status_bit = get_update_in_progress_flag();
		//If the flag is set, meaning an update is in progress, we will wait until it is cleared first before we consider when to read the registers.
		if (status_bit ==1){
			while (status_bit==1){
				status_bit = get_update_in_progress_flag();
			}
		}
		//Before reading any CMOS registers, we will check if the status bit is 0. (No update cycle is in progress)
		//In order to safely read, we will wait for an update to begin.
		while (status_bit==0){
			status_bit = get_update_in_progress_flag();
		}
		//Once an update is in progress, we will keep waiting till this update cycle is over.
		while (status_bit==1){
			status_bit = get_update_in_progress_flag();
		}
		//Finally, once the status bit is cleared, we will start reading from RTC. This is done
		//in order to assure that our reading is not interfered with an update, which might give us inconsistent results or invalid ones, 
		//such as giving us the time 8:60.
		short seconds = read_byte(seconds_offset);
		short minutes = read_byte(minutes_offset);
		short hours = read_byte(hours_offset);
		short day_of_month = read_byte(day_of_month_offset);
		short month = read_byte(month_offset);
		short year = read_byte(year_offset);
		short registerB = read_byte(register_b_offset);

		//If the 2nd bit in status register B is cleared, then the values are in binary coded decimal (BCD).
		//If that is the case, then we must convert it into binary.
		//Note: The following BCD code has been taken from https://wiki.osdev.org/CMOS 
		if (!(registerB & 0x04)) {
			seconds = (seconds & 0x0F) + ((seconds / 16) * 10);
			minutes = (minutes & 0x0F) + ((minutes/ 16) * 10);
			hours = ( (hours & 0x0F) + (((hours & 0x70) / 16) * 10) ) | (hours & 0x80);
			day_of_month = (day_of_month & 0x0F) + ((day_of_month / 16) * 10);
			month = (month & 0x0F) + ((month / 16) * 10);
			year = (year & 0x0F) + ((year / 16) * 10);
		}

		//If the hour is pm, then the 0x80 bit is set on the hour byte. In addition, registerB in the 1st bit will be set.
		//In case both of the conditions above is statisfied, then we will need to change from a 12 hour format to a 24 hour format.
		if (!(registerB & 0x02) && (hours & 0x80)) {
				hours = change_12_to_24_hr(hours);
		}
		
		//Set the tp structure with the current date and time
		tp.seconds = seconds;
		tp.minutes= minutes;
		tp.hours = hours;
		tp.day_of_month = day_of_month;
		tp.month = month;
		tp.year=year;
		syslog.messagef(LogLevel::DEBUG, "TP Structure Populated Successfully");
	
		

		syslog.messagef(LogLevel::DEBUG, "Interrupts Enabled");
	}
};

const DeviceClass CMOSRTC::CMOSRTCDeviceClass(RTC::RTCDeviceClass, "cmos-rtc");

RegisterDevice(CMOSRTC);
