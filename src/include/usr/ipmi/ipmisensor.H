#include <attributeenums.H>
#include <targeting/common/target.H>
#include <errl/errlentry.H>
#include <targeting/common/targetservice.H>
#ifndef __IPMI_IPMISENSOR_H
#define __IPMI_IPMISENSOR_H

#define BSWAP_UINT16(x) \
        ((((x) >> 8) & 0xff) | (((x) & 0xff) << 8))

namespace IPMI
{


// Setup the operation to let the BMC handle the events for us, not change
// the assert/de-assert bits and write the value we send to the sensor bytes.
#define DEFAULT_OPERATION 0x01 // see IPMI Spec. "Set Sensor Reading
                                          // and Event Status command" for
                                          // details on this byte.

// Base class for all derived sensor objects
    class SensorBase
    {


    public:

        enum  boot_phase
        {
            MEMORY_INITIALIZATION      = 0x01,
            SECONDARY_PROCESSOR_INIT   = 0x03,
            OS_BOOTING                 = 0x13,
            BASE_INITIALIZATION        = 0x14,
        };

        SensorBase(TARGETING::SENSOR_NAME i_name,
                   TARGETING::SENSOR_TYPE i_type,
                   TARGETING::Target * i_target);

        SensorBase( TARGETING::SENSOR_NAME i_name,
                   TARGETING::SENSOR_TYPE i_type,
                   TARGETING::Target * i_target , uint8_t *i_data,
                   uint8_t i_data_length);

        virtual  ~SensorBase();

        virtual errlHndl_t  writeSensorData();

        virtual errlHndl_t readSensorData() ;

        virtual void DebugPrint();


        // used to set-up assertion and de-assertion masks
        uint16_t setMask( uint8_t offset );


    protected:
    // from target I can get the sensor number, entity Type and entity Instance
    // as well as the sensor number.

        TARGETING::SENSOR_NAME iv_name;
        TARGETING::SENSOR_TYPE iv_type;
        TARGETING::Target * iv_target;

        set_sensor_reading_request * iv_request;

    private:
        SensorBase();

        // helper function to get sensor number attribute
        // of passed in target
        uint16_t  getSensorNumber();

    };

// Helper class to handle the Firmware progress
    class FirmwareProgressSensor : public SensorBase
    {
    public:

        enum firmware_progress_offsets
        {
            SYSTEM_FIRMWARE_POST_ERROR  = 0x00,
            SYSTEM_FIRMWARE_HANG        = 0x01,
            SYSTEM_FIRMWARE_PROGRESS    = 0x02,
        };

        enum firmware_progress_phase
        {
            MEMORY_INIT         = 0x01, // isteps 10->14
            SEC_PROCESSOR_INIT  = 0x03, // isteps 15->16
            STARTING_OS         = 0x13, // isteps 17->21
            BASE_INITIALIZATION = 0x14, // isteps  6-> 9

        };

    // uses the system target, requires 2 bytes of data
        FirmwareProgressSensor();
        ~FirmwareProgressSensor();

        errlHndl_t setBootProgressPhase( firmware_progress_phase i_progress );

    };

uint16_t search(uint16_t array[][2], uint8_t first, uint8_t last,
                       uint16_t key);

#if 0

// Helper class to handle the Firmware progress
    class HostStatusSensor : public SensorBase
    {

        enum host_status
        {
            S0_G0_WORKING   = 0x00,  // Host up
            S5_G2_SOFT_OFF  = 0x05,  // Soft power off
            LEGACY_ON_STATE = 0x0B   // Host not up, but started
        };

    public:

    // uses the system target, requires 2 bytes of data
    HostStatusSensor();
    ~HostStatusSensor();

    errlHndl_t setHostStatus( host_status status );

    };

#endif

    typedef uint8_t reboot_count_t;
    // Helper class to handle the reboot counter
    class RebootCountSensor : public SensorBase
    {

        public:
            // uses the system target, requires 2 bytes of data
            RebootCountSensor();
            ~RebootCountSensor();

            errlHndl_t setRebootCount( reboot_count_t count );

    };

}

#if  0
class StatusSensor : public SensorBase
{
    StatusSensor( TARGETING:SENSOR_NAME i_name, TARGETING:Target * i_target )
        :SensorBase( TARGETING::SENSOR_TYPE_STATUS, i_target ), iv_type(i_type)
    {};

    ~StatusSensor();

    virtual errlHndl_t setFunctionalState(bool state ) = 0;
    virtual errlHndl_t setPresenceState(bool state) = 0;

};

// the processor Status class will handle offsets spcific to the particular sensor
class ProcessorStatusSensor: StatusSensor( TARGETING::Target * i_target )
{
         ProcessorStatusSensor( TARGETING:Target i_target)
         :StatusSensor(i_target ){};

         ~ProcessorStatusSensor();

         errlHndl_t setFunctionalState(bool );
         errlHndl_t setPresentState(bool state);
};


class HostStatusSensor : public Sensor
{

    HostStatusSensor( )
    :SensorBase(NULL, HOST_STATUS_SENSOR)

    errlHndl_t

}

class FirmwareProgressSensor : public Sensor
{

enum bootPhase {
    MEMORY_INITIALIZATION= 0x01,
    SECONDARY_PROCESSOR_INIT = 0x03,
    OS_BOOTING= 0x13,
    BASE_INITIALIZATION = 0x14
}

    FirmwareProgressSensor( )
    :Sensor(NULL, FW_BOOT_PROGRESS_SENSOR)

    errlHndl_t setBaseInitializationPhase();                             //  14h -  >steps 6-9
    errlHndl_t setMemoryInitalizationPhase();                       //  01h  ->steps  10-14
    errlHndl_T setSecondaryProcessorInitPhase();              //  03h  ->steps
    errlHndl_t setOSBootPhase();                                                //  13h  ->steps  17-21
    errlHndl_t setBootProgressPhase( enum bootPhase phase );
}
#endif
#endif
