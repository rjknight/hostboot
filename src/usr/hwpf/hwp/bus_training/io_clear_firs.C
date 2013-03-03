/* IBM_PROLOG_BEGIN_TAG                                                   */
/* This is an automatically generated prolog.                             */
/*                                                                        */
/* $Source: src/usr/hwpf/hwp/bus_training/io_clear_firs.C $               */
/*                                                                        */
/* IBM CONFIDENTIAL                                                       */
/*                                                                        */
/* COPYRIGHT International Business Machines Corp. 2013                   */
/*                                                                        */
/* p1                                                                     */
/*                                                                        */
/* Object Code Only (OCO) source materials                                */
/* Licensed Internal Code Source Materials                                */
/* IBM HostBoot Licensed Internal Code                                    */
/*                                                                        */
/* The source code for this program is not published or otherwise         */
/* divested of its trade secrets, irrespective of what has been           */
/* deposited with the U.S. Copyright Office.                              */
/*                                                                        */
/* Origin: 30                                                             */
/*                                                                        */
/* IBM_PROLOG_END_TAG                                                     */
// $Id: io_clear_firs.C,v 1.5 2013/02/20 09:40:22 jaswamin Exp $
// *!***************************************************************************
// *! (C) Copyright International Business Machines Corp. 2012, 2013
// *!           All Rights Reserved -- Property of IBM
// *!                   *** IBM Confidential ***
// *!***************************************************************************
// *! FILENAME             : io_clear_firs.C
// *! TITLE                : 
// *! DESCRIPTION          : To clear summary fir registers
// *! CONTEXT              : 
// *!
// *! OWNER  NAME          : Swaminathan, Janani         Email: jaswamin@in.ibm.com
// *! BACKUP NAME          : Varghese, Varkey            Email: varkey.kv@in.ibm.com
// *!
// *!***************************************************************************
// CHANGE HISTORY:
//------------------------------------------------------------------------------
// Version:|Author: | Date:  | Comment:
// --------|--------|--------|--------------------------------------------------
//   1.3   |varkeykv|02/18/13|  Missing function check in
//   1.2   |jaswamin|02/14/13|  function for reading the fir scom register contents, enums and arrays for doing fir isolation
//   1.1   |jaswamin|02/14/13|  Additions for reading the fir register.
//   1.0   |jaswamin|01/30/13|  Initial check in .
//------------------------------------------------------------------------------

#include <fapi.H>
#include "io_clear_firs.H"
//#include "gcr_funcs.H"
//#include "ei4_regs.h"

extern "C" {


using namespace fapi;

// For clearing the FIR mask , used by io run training 
ReturnCode clear_fir_mask_reg(const Target &i_target,fir_io_interface_t i_chip_interface){
    
    ReturnCode rc;
//@thi - Fixed compiler error. Varkey will fix this in next version
//    uint32_t rc_ecmd=0;
    uint64_t  scom_address64=0;
    ecmdDataBufferBase putscom_data64(64),temp(64);
    //rc_ecmd |=getscom_data64.flushTo0();
    //rc_ecmd |=putscom_data64.flushTo0();
    FAPI_INF("io_run_training:In the Clear FIR MASK register function ");
    //get the 64 bit data
    temp.setDoubleWord(0,fir_clear_mask_reg_addr[i_chip_interface]);
    scom_address64=temp.getDoubleWord(0);
    
    //do the putscom
    rc=fapiPutScom( i_target, scom_address64, putscom_data64);
    
    return(rc);
    
}

// for toggling the rx and tx fir reset.
ReturnCode clear_fir_err_regs(const Target &i_target,io_interface_t i_chip_interface,uint32_t i_group){
    
    ReturnCode rc;
    uint32_t rc_ecmd=0;
    uint16_t bits = 0;
    ecmdDataBufferBase data_buffer;
 
    ecmdDataBufferBase set_bits(16);
    ecmdDataBufferBase clear_bits(16);
    
    FAPI_DBG("In the Clear fir procedure");
    //const Target *target_ptr=&target;
    
    //set the rx_fir_reset bit
    bits=rx_fir_reset;
    rc_ecmd|=set_bits.insert(bits,0,16);
    bits=rx_fir_reset_clear;
    rc_ecmd|=clear_bits.insert(bits,0,16);
    if(rc_ecmd)
    {
        rc.setEcmdError(rc_ecmd);
        return(rc);
    }
    rc=GCR_write(i_target,i_chip_interface,rx_reset_act_pg,i_group,0,set_bits ,clear_bits);if (rc) {return(rc);}
    
    //clear the rx_fir_reset bit
    bits=0x0000;
    rc_ecmd|=set_bits.insert(bits,0,16);
    bits=rx_fir_reset_clear;
    rc_ecmd|=clear_bits.insert(bits,0,16);
    if(rc_ecmd)
    {
        rc.setEcmdError(rc_ecmd);
        return(rc);
    }
    rc=GCR_write(i_target,i_chip_interface,rx_reset_act_pg,i_group,0,set_bits ,clear_bits);if (rc) {return(rc);}
    
    //set the tx_fir_reset bit
    bits=tx_fir_reset;
    rc_ecmd|=set_bits.insert(bits,0,16);
    bits=tx_fir_reset_clear;
    rc_ecmd|=clear_bits.insert(bits,0,16);
    if(rc_ecmd)
    {
        rc.setEcmdError(rc_ecmd);
        return(rc);
    }
    rc=GCR_write(i_target,i_chip_interface,tx_reset_act_pg,i_group,0,set_bits ,clear_bits);if (rc) {return(rc);}
    
    //clear the tx_fir_reset
    bits=0x0000;
    rc_ecmd|=set_bits.insert(bits,0,16);
    bits=tx_fir_reset_clear;
    rc_ecmd|=clear_bits.insert(bits,0,16);
    if(rc_ecmd)
    {
        rc.setEcmdError(rc_ecmd);
        return(rc);
    }
    rc=GCR_write(i_target,i_chip_interface,tx_reset_act_pg,i_group,0,set_bits ,clear_bits);if (rc) {return(rc);}
    
    return(rc);

    
}

ReturnCode clear_fir_reg(const Target &i_target,fir_io_interface_t i_chip_interface){
    
    ReturnCode rc;
//@thi - Fixed compiler error. Varkey will fix this in next version
//    uint32_t rc_ecmd=0;
    uint64_t  scom_address64=0;
    ecmdDataBufferBase putscom_data64(64),temp(64);
    //rc_ecmd |=getscom_data64.flushTo0();
    //rc_ecmd |=putscom_data64.flushTo0();
    
    //get the 64 bit data
    temp.setDoubleWord(0,fir_clear_reg_addr[i_chip_interface]);
    scom_address64=temp.getDoubleWord(0);
    
    //do the putscom
    rc=fapiPutScom( i_target, scom_address64, putscom_data64);
    
    return(rc);
    
}

ReturnCode read_fir_reg(const Target &i_target,fir_io_interface_t i_chip_interface,ecmdDataBufferBase &o_databuf_64bit){
    
    ReturnCode rc;
    uint32_t rc_ecmd=0;
    uint64_t scom_address64=0;
    ecmdDataBufferBase temp(64);
    rc_ecmd |=o_databuf_64bit.flushTo0();
    
    //rc_ecmd |=putscom_data64.flushTo0();
    
    //get the 64 bit scom address.
    temp.setDoubleWord(0,fir_rw_reg_addr[i_chip_interface]);
    scom_address64=temp.getDoubleWord(0);
    
    //read the 64 bit fir register
    rc=fapiGetScom(i_target,scom_address64,o_databuf_64bit);
    
    return(rc);
    
    
}

ReturnCode io_clear_firs(const fapi::Target &i_target){
    
    ReturnCode rc;
    fir_io_interface_t interface;
    io_interface_t gcr_interface; // requires different base address for gcr scoms
    uint32_t group;
    
    //on dmi
    if( (i_target.getType() == fapi::TARGET_TYPE_MCS_CHIPLET )){
          FAPI_DBG("This is a Processor DMI bus using base DMI scom address");
          interface=FIR_CP_IOMC0_P0; // base scom for MC bus
          gcr_interface=CP_IOMC0_P0;
          group=3; // design requires us to swap
          
     }
     else if((i_target.getType() ==  fapi::TARGET_TYPE_MEMBUF_CHIP)){
            FAPI_DBG("This is a Centaur DMI bus using base DMI scom address");
            interface=FIR_CEN_DMI;
            gcr_interface=CEN_DMI;
            group=0;
           
     }
     else if((i_target.getType() == fapi::TARGET_TYPE_XBUS_ENDPOINT)){
            FAPI_DBG("This is a X Bus invocation");
            interface=FIR_CP_FABRIC_X0;
            gcr_interface=CP_FABRIC_X0;
            group=0;
            
     }
     
     else if((i_target.getType() == fapi::TARGET_TYPE_ABUS_ENDPOINT)){
            FAPI_DBG("This is an A Bus invocation");
            interface=FIR_CP_FABRIC_A0;
            gcr_interface=CP_FABRIC_A0;
            group=0;
            
     }
     else{
        FAPI_ERR("Invalid io_clear_firs HWP invocation . Target doesnt belong to DMI/X/A instances");
        FAPI_SET_HWP_ERROR(rc, IO_CLEAR_FIRS_INVALID_INVOCATION_RC);
        return(rc);
     }
     
    rc=clear_fir_err_regs(i_target,gcr_interface,group);
    if(rc){
               return rc;
    }
    rc=clear_fir_reg(i_target,interface);
    return(rc);
}

} //end extern C