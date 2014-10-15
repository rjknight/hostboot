/* IBM_PROLOG_BEGIN_TAG                                                   */
/* This is an automatically generated prolog.                             */
/*                                                                        */
/* $Source: src/usr/ipmi/ipmirp.C $                                       */
/*                                                                        */
/* OpenPOWER HostBoot Project                                             */
/*                                                                        */
/* Contributors Listed Below - COPYRIGHT 2012,2014                        */
/* [+] International Business Machines Corp.                              */
/*                                                                        */
/*                                                                        */
/* Licensed under the Apache License, Version 2.0 (the "License");        */
/* you may not use this file except in compliance with the License.       */
/* You may obtain a copy of the License at                                */
/*                                                                        */
/*     http://www.apache.org/licenses/LICENSE-2.0                         */
/*                                                                        */
/* Unless required by applicable law or agreed to in writing, software    */
/* distributed under the License is distributed on an "AS IS" BASIS,      */
/* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or        */
/* implied. See the License for the specific language governing           */
/* permissions and limitations under the License.                         */
/*                                                                        */
/* IBM_PROLOG_END_TAG                                                     */
/**
 * @file ipmirp.C
 * @brief IPMI resource provider definition
 */

#include "ipmirp.H"
#include <ipmi/ipmi_reasoncodes.H>
#include <devicefw/driverif.H>
#include <devicefw/userif.H>

#include <config.h>
#include <sys/task.h>
#include <initservice/taskargs.H>
#include <initservice/initserviceif.H>
#include <sys/vfs.h>

#include <targeting/common/commontargeting.H>
#include <kernel/ipc.H>
#include <arch/ppc.H>
#include <errl/errlmanager.H>
#include <sys/time.h>
#include <sys/misc.h>
#include <errno.h>

// Defined in ipmidd.C
extern trace_desc_t * g_trac_ipmi;
#define IPMI_TRAC(printf_string,args...) \
    TRACFCOMP(g_trac_ipmi,"rp: "printf_string,##args)

/**
 * setup _start and handle barrier
 */
TASK_ENTRY_MACRO( IpmiRP::daemonProcess );

/**
 * @brief  Constructor
 */
IpmiRP::IpmiRP(void):
    iv_msgQ(msg_q_create()),
    iv_sendq(),
    iv_timeoutq(),
    iv_respondq(),
    iv_bmc_timeout(1),       // Defaults until we get the interface capabilties
    iv_outstanding_req(0xff),
    iv_xmit_buffer_size(0x40),
    iv_recv_buffer_size(0x40),
    iv_retries(0)
{
    mutex_init(&iv_mutex);
    sync_cond_init(&iv_cv);
}

/**
 * @brief  Destructor
 */
IpmiRP::~IpmiRP(void)
{
    msg_q_destroy(iv_msgQ);
}

void* IpmiRP::start(void* unused)
{
    Singleton<IpmiRP>::instance().execute();
    return NULL;
}

void* IpmiRP::timeout_thread(void* unused)
{
    Singleton<IpmiRP>::instance().timeoutThread();
    return NULL;
}

void* IpmiRP::get_capabilities(void* unused)
{
    Singleton<IpmiRP>::instance().getInterfaceCapabilities();
    return NULL;
}

void IpmiRP::daemonProcess(errlHndl_t& o_errl)
{
    task_create(&IpmiRP::start, NULL);
}

/**
 * @brief Return the maximum data size to allocate
 */
inline size_t IpmiRP::maxBuffer(void)
{
    // TODO RTC 116300: We need to figure out how to handle
    // this if the interface isn't up yet. We don't have the
    // capabilities data from the BMC, and this ends up being
    // static in max_buffer().

    // Given the max buffer information from the get-capabilities
    // command, subtract off the physical layers header size.
    IPMI::Message* msg = IPMI::Message::factory();

    mutex_lock(&iv_mutex);
    size_t mbs = iv_xmit_buffer_size - msg->header_size();
    mutex_unlock(&iv_mutex);
    delete msg;
    return mbs;
}

/**
 * @brief Start routine of the time-out handler
 */
void IpmiRP::timeoutThread(void)
{
    // Mark as an independent daemon so if it crashes we terminate.
    task_detach();

    // If there's something on the queue we want to grab it's timeout time
    // and wait. Note the reaponse queue is "sorted" as we send messages in
    // order. So, the first message on the queue is the one who's timeout
    // is going to come first.
    while (true)
    {
        mutex_lock(&iv_mutex);
        while (iv_timeoutq.size() == 0)
        {
            sync_cond_wait(&iv_cv, &iv_mutex);
        }

        msg_t*& msq_msg = iv_timeoutq.front();
        IPMI::Message* msg = static_cast<IPMI::Message*>(msq_msg->extra_data);

        // The diffence between the timeout of the first message in the
        // queue and the current time is the time we wait for a timeout
        timespec_t tmp_time;
        assert(0 == clock_gettime(CLOCK_MONOTONIC, &tmp_time), "gettime fail?");

        uint64_t now = (NS_PER_SEC * tmp_time.tv_sec) + tmp_time.tv_nsec;
        uint64_t timeout = (NS_PER_SEC * msg->iv_timeout.tv_sec) +
            msg->iv_timeout.tv_nsec;

        if (now >= timeout)
        {
            IPMI_TRAC("timeout: %x:%x", msg->iv_netfun, msg->iv_cmd);

            // This little bugger timed out. Get him off the timeoutq
            iv_timeoutq.pop_front();

            // Get him off the responseq, and reply back to the waiter that
            // there was a timeout
            response(msg, IPMI::CC_TIMEOUT);
            mutex_unlock(&iv_mutex);
        }
        else
        {
            mutex_unlock(&iv_mutex);
            nanosleep( 0, timeout - now );
        }
    }

    return;
}

/**
 * @brief Get the BMC interface capabilities
 */
void IpmiRP::getInterfaceCapabilities(void)
{
    // Mark as an independent daemon so if it crashes we terminate.
    task_detach();

    // Queue up a get-capabilties message.
    // TODO: RTC 116300 the RP shouldn't queue up other messages until the
    // BMC has given us these parameters. Handle max_buffer() case too.

    IPMI::completion_code cc = IPMI::CC_UNKBAD;
    size_t len = 0;
    uint8_t* data = NULL;
    errlHndl_t err = IPMI::sendrecv(IPMI::get_capabilities(), cc, len, data);

    do {
        // If we have a problem, we can't "turn on" the IPMI stack.
        if (err)
        {
            IPMI_TRAC("get_capabilties returned an error");
            errlCommit(err, IPMI_COMP_ID);
            break;
        }

        // If we get back a funky completion code, we'll use the defaults.
        if (cc != IPMI::CC_OK)
        {
            IPMI_TRAC("get_capabilities not ok %d, using defaults", cc);
            break;
        }

        // If we didn't get back what we expected, use the defaults
        if (len != 5)
        {
            IPMI_TRAC("get_capabilities length %d; using defaults", len);
            break;
        }

        // Protect the members as we're on another thread.
        mutex_lock(&iv_mutex);
        iv_outstanding_req = data[0];
        iv_xmit_buffer_size = data[1];
        iv_recv_buffer_size = data[2];
        iv_bmc_timeout = data[3];
        iv_retries = data[4];
        mutex_unlock(&iv_mutex);

        IPMI_TRAC("get_capabilities: requests %d, in buf %d, "
                  "out buf %d, timeout %d, retries %d",
                  iv_outstanding_req, iv_xmit_buffer_size,
                  iv_recv_buffer_size, iv_bmc_timeout, iv_retries);

    } while(false);

    delete[] data;

    return;
}

/**
 * @brief  Entry point of the resource provider
 */
void IpmiRP::execute(void)
{
    // Mark as an independent daemon so if it crashes we terminate.
    task_detach();

    IPMI_TRAC(ENTER_MRK "message loop");

    // TODO: RTC 116300 Mark the daemon as started in the interface.
    // TODO: RTC 116300 Query the BMC for timeouts, other config
    // TODO: RTC 116300 Hold off transmitters until the BMC is ready?

    // Register shutdown events with init service.
    //      Done at the "end" of shutdown processesing.
    //      This will flush out any IPMI messages which were sent as
    //      part of the shutdown processing. We chose MBOX priority
    //      as we don't want to accidentally get this message after
    //      interrupt processing has stopped in case we need intr to
    //      finish flushing the pipe.
    INITSERVICE::registerShutdownEvent(iv_msgQ, IPMI::MSG_STATE_SHUTDOWN,
                                       INITSERVICE::MBOX_PRIORITY);
    do {

        // Start the thread that waits for timeouts
        task_create( &IpmiRP::timeout_thread, NULL );

        // Queue and wait for a message for the interface capabilities
        task_create( &IpmiRP::get_capabilities, NULL);

        while (true)
        {
            msg_t* msg = msg_wait(iv_msgQ);

            switch(static_cast<IPMI::msg_type>(msg->type))
            {
                // Messages we're told to send. If we get a transmission
                // error (EAGAIN counts) then the interface is likely
                // not idle, and so we don't want to bother with idle below
            case IPMI::MSG_STATE_SEND:
                IPMI_TRAC("msg loop: ipmi transmit message");
                // Push the message on the queue, and the idle() at the
                // bottom of this loop will start the transmit process.
                // Be sure to push_back to ensure ordering of transmission.
                iv_sendq.push_back(msg);
                break;

                // State changes from the IPMI hardware. These are async
                // messages so we get rid of them here.
            case IPMI::MSG_STATE_IDLE:
                IPMI_TRAC("msg loop: ipmi idle state");
                msg_free(msg);
                // No-op - we do it at the bottom of the loop.
                break;

            case IPMI::MSG_STATE_RESP:
                IPMI_TRAC("msg loop: ipmi response state");
                msg_free(msg);
                response();
                break;
            case IPMI::MSG_STATE_EVNT:
                IPMI_TRAC("msg loop: ipmi event state");
                msg_free(msg);
                // TODO: RTC 116300 Handle SMS messages
                break;

                // Accept no more messages. Anything in the sendq is doomed.
                // This should be OK - either they were async messages in which
                // case they'd appear to never have been sent or they're sync
                // in which case the higher layers should have handled this case
                // in their shutdown processing.
            case IPMI::MSG_STATE_SHUTDOWN:
                IPMI_TRAC(INFO_MRK "ipmi shuting down");
                // TODO: RTC 116887 Hold off transmitters, drain queues.
                // Patrick suggests handling this like mailboxes.
                msg_respond(iv_msgQ, msg);
                break;
            };

            // There's a good chance the interface will be idle right after
            // the operation we just performed. Since we need to poll for the
            // idle state, calling idle after processing a message may decrease
            // the latency of waiting for idle. The worst case is that we find
            // the interface busy and go back to waiting.
            idle();
        }

    } while (false);

    return;
}

///
/// @brief  Go in to the idle state
///
void IpmiRP::idle(void)
{
    // If the interface is idle, we can write anything we need to write.
    for (IPMI::send_q_t::iterator i = iv_sendq.begin(); i != iv_sendq.end();)
    {
        // If we have a problem transmitting a message, then we just stop
        // here and wait for the next time the interface transitions to idle.
        // Note that there are two failure cases: the first is that there is
        // a problem transmitting. In this case we told the other end of the
        // message queue, and so the life of this message is over. The other
        // case is that the interface turned out to be busy in which case
        // this message can sit on the queue and it'll be next.

        IPMI::Message* msg = static_cast<IPMI::Message*>((*i)->extra_data);

        IPMI_TRAC("transmitting q head %x:%x, queue length %d",
                  msg->iv_netfun, msg->iv_cmd, iv_sendq.size());

        // If there was an i/o error, we do nothing - leave this message on
        // the queue. Don't touch msg after xmit returns. If the message was
        // sent, and it was async, msg has been destroyed.
        if (msg->xmit())
        {
            break;
        }
        i  = iv_sendq.erase(i);
    }
}

///
/// @brief Handle a response to a message we sent
///
void IpmiRP::response(void)
{
    IPMI::Message* rcv_buf = IPMI::Message::factory();

    do
    {
        // Go down to the device and read. Fills in iv_key.
        errlHndl_t err = rcv_buf->recv();

        if (err)
        {
            // Not much we're going to do here, so lets commit the error and
            // the original request will timeout.
            err->collectTrace(IPMI_COMP_NAME);
            errlCommit(err, IPMI_COMP_ID);
            break;
        }

        mutex_lock(&iv_mutex);
        response(rcv_buf);
        mutex_unlock(&iv_mutex);

    } while (false);

    delete rcv_buf;
    return;
}

///
/// @brief Handle a response to a message we want to change
///
void IpmiRP::response(IPMI::Message* i_msg, IPMI::completion_code i_cc)
{
    i_msg->iv_cc = i_cc;
    i_msg->iv_netfun |= 0x04; // update the netfun
    i_msg->iv_len = 0;        // sending no data
    i_msg->iv_data = NULL;
    response(i_msg);
    return;
}

///
/// @brief Handle a response to a message we have
///
void IpmiRP::response(IPMI::Message* i_msg)
{
    do {

        // Look for a message with this seq number waiting for a
        // repsonse. If there isn't a message looking for this response,
        // log that fact and drop this on the floor.
        // TO THINK ABOUT: Could there be a command which generated
        // more than one response packet from the BMC? If this happens,
        // these messages should be handled like SMS messages - sent to
        // the appropriate queue for a waiter to take care of. So if a
        // message can generate more than one packet of response, something
        // needs to register for the overflow. So far we have not seen
        // any such beast ...
        IPMI::respond_q_t::iterator itr = iv_respondq.find(i_msg->iv_key);
        if (itr == iv_respondq.end())
        {
            // Every async message goes through this path. The semantics
            // somewhat contrary to IPMI semantics in that we have the ability
            // to generate "async" messages when the IPMI spec says there's no
            // such thing. We decided to just drop the response on the floor.
            // This is good for "fire and forget" situations where we don't
            // really care if the BMC gets the message or processes it
            // correctly. However, the BMC doesn't know we don't care about the
            // response and sends it. This code path does the dropping of the
            // response.
            IPMI_TRAC("IPMI command response, but no waiter: seq %x %x:%x",
                      i_msg->iv_seq, i_msg->iv_netfun, i_msg->iv_cmd);
            IPMI_TRAC("deleting response data %x:%x, no waiter",
                      i_msg->iv_netfun, i_msg->iv_cmd);
            delete[] i_msg->iv_data;
            break;
        }

        msg_t* original_msg = itr->second;

        // Get us off the response queue, and the timeout queue.
        iv_respondq.erase(itr);
        iv_timeoutq.remove(original_msg);

        // Hand the allocated buffer over to the original message's
        // ipmi_msg_t It will be responsible for de-allocating it
        // when it's dtor is called.
        IPMI::Message* ipmi_msg =
            static_cast<IPMI::Message*>(original_msg->extra_data);

        // Hand ownership of the data to the original requestor
        ipmi_msg->iv_data = i_msg->iv_data;
        ipmi_msg->iv_len  = i_msg->iv_len;

        // Send the response to the original caller of sendrecv()
        int rc = msg_respond(iv_msgQ, original_msg);
        if (rc)
        {
            // Not much we're going to do here, so lets commit an error and
            // the original request will timeout.

            /* @errorlog tag
             * @errortype       ERRL_SEV_UNRECOVERABLE
             * @moduleid        IPMI::MOD_IPMISRV_REPLY
             * @reasoncode      IPMI::RC_INVALID_QRESPONSE
             * @userdata1       rc from msg_respond()
             * @devdesc         msg_respond() failed
             */
            errlHndl_t err = new ERRORLOG::ErrlEntry(
                ERRORLOG::ERRL_SEV_UNRECOVERABLE,
                IPMI::MOD_IPMISRV_REPLY,
                IPMI::RC_INVALID_QRESPONSE,
                rc,
                0,
                true);
            err->collectTrace(IPMI_COMP_NAME);

            IPMI_TRAC("msg_respond() i/o error (response) %d", rc);
            errlCommit(err, IPMI_COMP_ID);

            // Frotz the response data
            IPMI_TRAC("deleting response data %x:%x, msgq error",
                      ipmi_msg->iv_netfun, ipmi_msg->iv_cmd);
            delete[] ipmi_msg->iv_data;

            break;
        }

    } while(false);

    return;
}

///
/// @brief Queue a message on to the response queue
///
void IpmiRP::queueForResponse(IPMI::Message& i_msg)
{
    // Figure out when this fellow's timeout should occur. If we
    // have a problem from clock_gettime we have a bug, not an error.
    assert(0 == clock_gettime(CLOCK_MONOTONIC, &(i_msg.iv_timeout)));

    // Lock before accessing the timeout (and the queues, etc.)
    mutex_lock(&iv_mutex);

    // BMC request-to-response times are always seconds, 1 - 30.
    // And I don't think we care about roll over here.
    i_msg.iv_timeout.tv_sec += iv_bmc_timeout;

    // Put this message on the response queue so we can find it later
    // for a response and on the timeout queue so if it times out
    // we can find it there.

    iv_respondq[i_msg.iv_seq] = i_msg.iv_msg;
    iv_timeoutq.push_back(i_msg.iv_msg);

    // If we put a message in an empty timeout queue (we know this as
    // there's only one message in the queue now) signal the timeout thread
    if (iv_timeoutq.size() == 1)
    {
        sync_cond_signal(&iv_cv);
    }

    mutex_unlock(&iv_mutex);
    return;
}


namespace IPMI
{
    ///
    /// @brief  Synchronus message send
    ///
    errlHndl_t sendrecv(const IPMI::command_t& i_cmd,
                        IPMI::completion_code& o_completion_code,
                        size_t& io_len, uint8_t*& io_data)
    {
        errlHndl_t err;
        static msg_q_t mq = Singleton<IpmiRP>::instance().msgQueue();

        IPMI::Message* ipmi_msg = IPMI::Message::factory(i_cmd, io_len, io_data,
                                                         IPMI::TYPE_SYNC);

        IPMI_TRAC("sync send %x:%x", i_cmd.first, i_cmd.second);

        // I think if the buffer is too large this is a programming error.
        assert(io_len <= max_buffer());

        int rc = msg_sendrecv(mq, ipmi_msg->iv_msg);

        // If the kernel didn't give a hassle about he message, check to see if
        // there was an error reported back from the other end of the queue. If
        // this message made it to the other end of the queue, then our memory
        // (io_data) is in the proper state.
        if (rc == 0) {
            err = ipmi_msg->iv_errl;
        }

        // Otherwise, lets make an errl out of our return code
        else
        {
            /* @errorlog tag
             * @errortype       ERRL_SEV_UNRECOVERABLE
             * @moduleid        IPMI::MOD_IPMISRV_SEND
             * @reasoncode      IPMI::RC_INVALID_SENDRECV
             * @userdata1       rc from msq_sendrecv()
             * @devdesc         msg_sendrecv() failed
             */
            err = new ERRORLOG::ErrlEntry(ERRORLOG::ERRL_SEV_UNRECOVERABLE,
                                          IPMI::MOD_IPMISRV_SEND,
                                          IPMI::RC_INVALID_SENDRECV,
                                          rc,
                                          0,
                                          true);
            err->collectTrace(IPMI_COMP_NAME);

            // ... and clean up the memory for the caller
            IPMI_TRAC("deleting data %x:%x .", i_cmd.first, i_cmd.second);
            delete[] io_data;
        }

        // The length and the data are the response, if there was one. All of
        // there are pretty much invalid if there was an error.
        io_len = ipmi_msg->iv_len;
        io_data = ipmi_msg->iv_data;
        o_completion_code = static_cast<IPMI::completion_code>(ipmi_msg->iv_cc);
        delete ipmi_msg;

        return err;
    }

    ///
    /// @brief  Asynchronus message send
    ///
    errlHndl_t send(const IPMI::command_t& i_cmd,
                    const size_t i_len, uint8_t* i_data)
    {
        static msg_q_t mq = Singleton<IpmiRP>::instance().msgQueue();
        errlHndl_t err = NULL;

        // We don't delete this message, the message will self destruct
        // after it's been transmitted. Note it could be placed on the send
        // queue and we are none the wiser - so we can't delete it.
        IPMI::Message* ipmi_msg = IPMI::Message::factory(i_cmd, i_len, i_data,
                                                         IPMI::TYPE_ASYNC);
        IPMI_TRAC("async send %x:%x", i_cmd.first, i_cmd.second);

        // I think if the buffer is too large this is a programming error.
        assert(i_len <= max_buffer());

        int rc = msg_send(mq, ipmi_msg->iv_msg);

        if (rc)
        {
            /* @errorlog tag
             * @errortype       ERRL_SEV_UNRECOVERABLE
             * @moduleid        IPMI::MOD_IPMISRV_SEND
             * @reasoncode      IPMI::RC_INVALID_SEND
             * @userdata1       rc from msq_send()
             * @devdesc         msg_send() failed
             */
            err = new ERRORLOG::ErrlEntry(ERRORLOG::ERRL_SEV_UNRECOVERABLE,
                                          IPMI::MOD_IPMISRV_SEND,
                                          IPMI::RC_INVALID_SEND,
                                          rc,
                                          0,
                                          true);
            err->collectTrace(IPMI_COMP_NAME);

            // ... and clean up the memory for the caller
            IPMI_TRAC("deleting data %x:%x .", i_cmd.first, i_cmd.second);
            delete[] i_data;
        }

        return err;
    }

    ///
    /// @brief  Maximum buffer for data (max xport - header)
    ///
    inline size_t max_buffer(void)
    {
        static const size_t mbs = Singleton<IpmiRP>::instance().maxBuffer();
        return mbs;
    }

    ///
    /// @brief Synchronously send an event
    ///
    errlHndl_t send_event(const uint8_t i_sensor_type,
                          const uint8_t i_sensor_number,
                          const bool i_assertion,
                          const uint8_t i_type,
                          completion_code& o_completion_code,
                          const size_t i_len,
                          uint8_t* i_data)
    {
        static const size_t event_header = 5;

        // Sanity check
        assert((i_len > 0) && (i_len < 4),
               "event request i_len incorrect %d", i_len);
        assert(i_type < 0x80, "event request i_type out of range %x", i_type);

        size_t len = event_header + i_len;
        uint8_t* data = new uint8_t[len];
        IPMI::completion_code cc = IPMI::CC_OK;

        data[0] = 0x01;     // More or less fixed, see table 5.4
        data[1] = 0x04;     // Fixed in the IPMI spec table 29.5
        data[2] = i_sensor_type;
        data[3] = i_sensor_number;
        data[4] = (i_assertion ? 0x80 : 0x00) + i_type;
        for (size_t i = 0; i < i_len; i++)
        {
            data[event_header + i] = i_data[i];
        }

        // We're done with i_data, but the caller deletes it.

        errlHndl_t err = sendrecv(IPMI::platform_event(), cc, len, data);

        o_completion_code = cc;
        delete[] data;
        return err;
    }

    ///
    /// @brief Synchronously send a set sensor reading command
    ///
    errlHndl_t send_set_sensor_reading_cmd(
                          set_sensor_reading_request * i_cmd,
                          completion_code& o_completion_code )
    {
        static const size_t cmd_header = 3;  // $TODO verify this size

        size_t len = cmd_header + sizeof( set_sensor_reading_request );

        uint8_t* data = new uint8_t[len];

        IPMI::completion_code cc = IPMI::CC_OK;

        // copy the sensor reading to the data buffer
        memcpy( data + cmd_header, i_cmd, sizeof(set_sensor_reading_request));

        // We're done with i_data, but the caller deletes it.
        errlHndl_t err = sendrecv(IPMI::set_sensor_reading(), cc, len, data);

        o_completion_code = cc;

        delete[] data;

        return err;
    }

};
