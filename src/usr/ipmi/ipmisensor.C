#include <ipmiif.H>
#include <ipmisensor.H>
#include <errl/errlentry.H>
#include <targeting/common/target.H>
#include <attributetraits.H>


#define SENSOR_TRACE_NAME "SENS"

trace_desc_t* g_sensorTd;
TRAC_INIT(&g_sensorTd, SENSOR_TRACE_NAME, 4*KILOBYTE, TRACE::BUFFER_SLOW);

#define SENS_TRAC(printf_string,args...) \
    TRACFCOMP(g_trac_sensor,"sensor: "printf_string,##args)

/**
 * @file ipmisensor.C
 * @brief IPMI sensor manipulation
 */
namespace IPMI
{

  SensorBase::SensorBase( TARGETING::SENSOR_NAME i_name,
                        TARGETING::SENSOR_TYPE i_type,
                        TARGETING::Target * i_target)
  :iv_name(i_name), iv_type(i_type),iv_target(i_target)
  {
    iv_request = new set_sensor_reading_request;

    memset(iv_request, sizeof(set_sensor_reading_request), 0);

    // $TODO verify we wont be changing this..
    iv_request->operation = DEFAULT_OPERATION;
  };


  SensorBase::~SensorBase()
  {
  };

  void SensorBase::DebugPrint()
  {
    // print the internal sensor values to the console
    TRACFCOMP(g_sensorTd,"set_sensor_reading reqest:")

    TRACFCOMP(g_sensorTd,"iv_request->sensor_number = 0x%x",
                          iv_request->sensor_number );

    TRACFCOMP(g_sensorTd,"iv_request->operation = 0x%x",
                          iv_request->operation );

    TRACFCOMP(g_sensorTd,"iv_request->sensor_reading = 0x%x",
                          iv_request->sensor_reading );

    TRACFCOMP(g_sensorTd,"iv_request->assertion_mask = 0x%x",
                          iv_request->assertion_mask);

    TRACFCOMP(g_sensorTd,"iv_request->deassertion_mask = 0x%x",
                          iv_request->deassertion_mask );

    TRACFCOMP(g_sensorTd,"iv_request->event_data[0] = 0x%x",
                          iv_request->event_data[0] );

    TRACFCOMP(g_sensorTd,"iv_request->event_data[1] = 0x%x",
                          iv_request->event_data[1] );

    TRACFCOMP(g_sensorTd,"iv_request->event_data[2] = 0x%x",
                          iv_request->event_data[2]);

  };

// send the data constructed internally to the sensor object
  errlHndl_t SensorBase::writeSensorData()
  {
    iv_request->sensor_number = getSensorNumber();

    completion_code l_rc = IPMI::CC_OK;;

    // dump the message to the console
    DebugPrint();

    errlHndl_t l_err = send_set_sensor_reading_cmd( iv_request,l_rc);

    return l_err;
  };

  // set the byte in the assertion/deassertion mask associated with the
  // desired sensor specific offset
  uint16_t SensorBase::setMask( uint8_t offset )
  {

    uint16_t mask = 0x0001;

    mask <<= offset;

    // byte swap the mask here (see set sensor info in spec)
    BSWAP_UINT16(mask);

    return mask;

  };

  // send the data constructed internally to the sensor
  errlHndl_t SensorBase::readSensorData()
  {
        // send command to get the sensor data
        // will need the SDR to figure out the data format
        return NULL;
  };


#define INVALID_SENSOR 0xFF

  uint16_t SensorBase::getSensorNumber()
  {

    int sensor_index = -1;

    if( iv_target == NULL )
    {
        // use the system target
        TARGETING::targetService().getTopLevelTarget(iv_target);

        // die if there is no system target
        if(iv_target == NULL)
          assert(0,"system target NULL!");
    }

    TARGETING::AttributeTraits<TARGETING::ATTR_IPMI_SENSORS>::Type  l_sensors;

    if(  iv_target->tryGetAttr<TARGETING::ATTR_IPMI_SENSORS>(l_sensors) )
    {

      sensor_index = search( l_sensors, 0, 15, iv_name );

      assert(sensor_index != -1,"Did not find the sensor name %s", iv_name);

    }
    else
    {
      // bug here...
      assert(0,"no IPMI_SENSOR attribute check target..");
    }

    return l_sensors[sensor_index][1];
  }

    // uses the system target, requires 2 bytes of data
    FirmwareProgressSensor::FirmwareProgressSensor( )
    :SensorBase(TARGETING::SENSOR_NAME_FW_BOOT_PROGRESS,
                              TARGETING::SENSOR_TYPE_SYSTEM_FIRMWARE_PROGRESS,
                              NULL)
    {
      // iv_request allocated in SensorBase
      // assert the system firmware progress offset in the mask.
      iv_request->assertion_mask = setMask( SYSTEM_FIRMWARE_PROGRESS );


    }

   FirmwareProgressSensor::~FirmwareProgressSensor( )
   {
   }

    errlHndl_t FirmwareProgressSensor::setBootProgressPhase(
                                          firmware_progress_phase phase )
    {
         iv_request->event_data[1] = phase;

         return writeSensorData();
    };


    RebootCountSensor::RebootCountSensor()
    :SensorBase(TARGETING::SENSOR_NAME_REBOOT_COUNT,
                TARGETING::SENSOR_TYPE_REBOOT_COUNT, NULL)
    {
    }

    RebootCountSensor::~RebootCountSensor(){};

    errlHndl_t RebootCountSensor::setRebootCount( reboot_count_t i_count )
    {

      iv_request->sensor_reading = i_count;

      return writeSensorData();

    }

// binary search on two dimensional array using the first entry as the
// key
uint16_t search(uint16_t array[][2], uint8_t first, uint8_t last,
                       uint16_t key)
{
  int idx = -1;

  if (first > last)
  {
    // code bug
    assert(0, "first > last -- check parms to binary search ");
  }
  else
  {
    int mid = (first + last)/2;

    if ( key == array[mid][0])
    {
        idx = mid;
    }
    else
    {
        if (key < array[mid][0])
        {
          idx = search(array,  first, mid-1, key);
        }
        else
        {
          idx = search(array,  mid+1, last, key);
        }
    }
  } //
  return idx;
}
};

TARGETING::Target * ptr = NULL;

errlHndl_t  test(void)
{
            IPMI::FirmwareProgressSensor l_ProgressSensor;

            // setup the progress sensor to indicate we are in steps 6-9
            errlHndl_t l_err = l_ProgressSensor.setBootProgressPhase(
                                    IPMI::FirmwareProgressSensor::MEMORY_INIT );

            return l_err;
};
