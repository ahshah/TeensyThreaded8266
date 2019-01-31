/**
 * @file ESP8266.cpp
 * @brief The implementation of class ESP8266. 
 * @author Wu Pengfei<pengfei.wu@itead.cc> 
 * @date 2015.02
 * 
 * @par Copyright:
 * Copyright (c) 2015 ITEAD Intelligent Systems Co., Ltd. \n\n
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version. \n\n
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
#include "ESP8266.h"
#include "ESP8266_private.h"
#include <string.h>


int PIN_RTS = 19;
uint8_t tmpBufIndex;
char  tmpBuf[32];
connection_t GConnects[5];

#define LOG_OUTPUT_DEBUG            (0)
#define LOG_OUTPUT_DEBUG_PREFIX     (0)

#define logDebug(arg)\
    do {\
        if (LOG_OUTPUT_DEBUG)\
        {\
            if (LOG_OUTPUT_DEBUG_PREFIX)\
            {\
                Console.print("[LOG Debug: ");\
                Console.print((const char*)__FILE__);\
                Console.print(",");\
                Console.print((unsigned int)__LINE__);\
                Console.print(",");\
                Console.print((const char*)__FUNCTION__);\
                Console.print("] ");\
            }\
            Console.print(arg);\
        }\
    } while(0)

#ifdef ESP8266_USE_SOFTWARE_SERIAL
ESP8266::ESP8266(SoftwareSerial &uart, uint32_t baud): m_puart(&uart)
{
    m_puart->begin(baud);
    rx_empty();
}
#else
ESP8266::ESP8266(HardwareSerial &uart, uint32_t baud): m_puart(&uart)
{
    m_puart->begin(baud);
    rx_empty();
    memset(&ctx_tx,  0, sizeof(ctx_tx));
}
#endif

bool ESP8266::kick(void)
{
    return eAT();
}

bool ESP8266::restart(void)
{
    pinMode(PIN_RTS, OUTPUT);
    Serial2.attachRts(PIN_RTS);

    for (int i =0; i < 5; i++) {
        memset(&(GConnects[i]),  0, sizeof(connection_t));
    }
    unsigned long start;
    if (eATRST()) {
        delay(2000);
        start = millis();
        while (millis() - start < 3000) {
            if (eAT()) {
                delay(1500); /* Waiting for stable */
                return true;
            }
            delay(100);
        }
    }
    return false;
}

String ESP8266::getVersion(void)
{
    String version;
    eATGMR(version);
    return version;
}

bool ESP8266::setOprToStation(void)
{
    uint8_t mode;
    if (!qATCWMODE(&mode)) {
        return false;
    }
    if (mode == 1) {
        return true;
    } else {
        if (sATCWMODE(1) && restart()) {
            return true;
        } else {
            return false;
        }
    }
}

bool ESP8266::setOprToSoftAP(void)
{
    uint8_t mode;
    if (!qATCWMODE(&mode)) {
        return false;
    }
    if (mode == 2) {
        return true;
    } else {
        if (sATCWMODE(2) && restart()) {
            return true;
        } else {
            return false;
        }
    }
}

bool ESP8266::setOprToStationSoftAP(void)
{
    uint8_t mode;
    if (!qATCWMODE(&mode)) {
        return false;
    }
    if (mode == 3) {
        return true;
    } else {
        if (sATCWMODE(3) && restart()) {
            return true;
        } else {
            return false;
        }
    }
}

String ESP8266::getAPList(void)
{
    String list;
    eATCWLAP(list);
    return list;
}

bool ESP8266::joinAP(String ssid, String pwd)
{
    return sATCWJAP(ssid, pwd);
}

bool ESP8266::enableClientDHCP(uint8_t mode, boolean enabled)
{
    return sATCWDHCP(mode, enabled);
}

bool ESP8266::leaveAP(void)
{
    return eATCWQAP();
}

bool ESP8266::setSoftAPParam(String ssid, String pwd, uint8_t chl, uint8_t ecn)
{
    return sATCWSAP(ssid, pwd, chl, ecn);
}

String ESP8266::getJoinedDeviceIP(void)
{
    String list;
    eATCWLIF(list);
    return list;
}

String ESP8266::getIPStatus(void)
{
    String list;
    eATCIPSTATUS(list);
    return list;
}

String ESP8266::getLocalIP(void)
{
    String list;
    eATCIFSR(list);
    return list;
}

String ESP8266::getAccessPointIP(void)
{
    String list;
    eAT_CIPAP(list);
    return list;
}

bool ESP8266::enableMUX(void)
{
    return sATCIPMUX(1);
}

bool ESP8266::disableMUX(void)
{
    return sATCIPMUX(0);
}

bool ESP8266::createTCP(String addr, uint32_t port)
{
    return sATCIPSTARTSingle("TCP", addr, port);
}

bool ESP8266::releaseTCP(void)
{
    return eATCIPCLOSESingle();
}

bool ESP8266::registerUDP(String addr, uint32_t port)
{
    return sATCIPSTARTSingle("UDP", addr, port);
}

bool ESP8266::unregisterUDP(void)
{
    return eATCIPCLOSESingle();
}

bool ESP8266::createTCP(uint8_t mux_id, String addr, uint32_t port)
{
    return sATCIPSTARTMultiple(mux_id, "TCP", addr, port);
}

bool ESP8266::releaseTCP(uint8_t mux_id)
{
    Threads::Scope m(_lock);
    bool ret = sATCIPCLOSEMulitple(mux_id);
    if (ret) {
//        Console.printf("RX closing muxid %d success\r\n", mux_id);
    }
    else {
 //       Console.printf("RX closing muxid %d fail\r\n", mux_id);
    }
    return ret;
}

bool ESP8266::registerUDP(uint8_t mux_id, String addr, uint32_t port)
{
    return sATCIPSTARTMultiple(mux_id, "UDP", addr, port);
}

bool ESP8266::unregisterUDP(uint8_t mux_id)
{
    return sATCIPCLOSEMulitple(mux_id);
}

bool ESP8266::setTCPServerTimeout(uint32_t timeout)
{
    return sATCIPSTO(timeout);
}

bool ESP8266::startTCPServer(uint32_t port)
{
    if (sATCIPSERVER(1, port)) {
        return true;
    }
    return false;
}

bool ESP8266::stopTCPServer(void)
{
    sATCIPSERVER(0);
    restart();
    return false;
}

bool ESP8266::startServer(uint32_t port)
{
    return startTCPServer(port);
}

bool ESP8266::stopServer(void)
{
    return stopTCPServer();
}

bool ESP8266::send(const uint8_t *buffer, uint32_t len)
{
    return sATCIPSENDSingle(buffer, len);
}

bool ESP8266::send(uint8_t mux_id, const uint8_t *buffer, uint32_t len)
{
    Threads::Scope m(_lock);
    Console.printf("Sending len {%d}\r\n", len);
    return sATCIPSENDMultiple(mux_id, buffer, len);
}


bool ESP8266::queue(uint8_t mux_id, const uint8_t *buffer, uint32_t len)
{
    connection_t* cn = &GConnects[mux_id];
    Threads::Scope m(cn->tx_lock);

/*    if ((sizeof(cn->tx_data)-cn->tx_len) < len) {
        return false;
    }
    memcpy(cn->tx_data+cn->tx_len, buffer, len);
    */

    if (cn->tx_len != 0) {
        return false;
    }

    cn->tx_data = (char*) buffer;
    cn->tx_len=len;
    return true;
}

bool ESP8266::queueAvail(uint8_t mux_id) {
    connection_t* cn = &GConnects[mux_id];
    Threads::Scope m(cn->tx_lock);

    if (cn->tx_len != 0) {
        return false;
    }
    return true;
}

void ESP8266::rx_empty(void)
{
    while(m_puart->available() > 0) {
        m_puart->read();
    }
}

String ESP8266::recvString(String target, uint32_t timeout)
{
    String data;
    char a;
    unsigned long start = millis();
    bool found = false;
    while (millis() - start < timeout) {
        while(m_puart->available() > 0) {
            a = m_puart->read();
            if(a == '\0') continue;
            data += a;
            if (data.indexOf(target) != -1) {
                found = true;
                break;
            }
        }
        if (found) { break; }
    }
    return data;
}

String ESP8266::recvString(String target1, String target2, uint32_t timeout)
{
    String data;
    char a;
    unsigned long start = millis();
    while (millis() - start < timeout) {
        while(m_puart->available() > 0) {
            a = m_puart->read();
            if(a == '\0')  {
                continue; 
            }
            data += a;
        }
        if (data.indexOf(target1) != -1) {
            break;
        } else if (data.indexOf(target2) != -1) {
            break;
        }
    }
    return data;
}

String ESP8266::recvString(String target1, String target2, String target3, uint32_t timeout)
{
    String data;
    char a;
    unsigned long start = millis();
    while (millis() - start < timeout) {
        while(m_puart->available() > 0) {
            a = m_puart->read();
            if(a == '\0') { 
                continue; 
            }
            data += a;
        }
        if (data.indexOf(target1) != -1) {
            break;
        } else if (data.indexOf(target2) != -1) {
            break;
        } else if (data.indexOf(target3) != -1) {
            break;
        }
    }
    return data;
}

bool ESP8266::recvFind(String target, uint32_t timeout)
{
    String data_tmp;
    data_tmp = recvString(target, timeout);
    if (data_tmp.indexOf(target) != -1) {
        return true;
    }
    return false;
}

bool ESP8266::recvFindAndFilter(String target, String begin, String end, String &data, uint32_t timeout)
{
    String data_tmp;
    data_tmp = recvString(target, timeout);
    if (data_tmp.indexOf(target) != -1) {
        int32_t index1 = data_tmp.indexOf(begin);
        int32_t index2 = data_tmp.indexOf(end);
        if (index1 != -1 && index2 != -1) {
            index1 += begin.length();
            data = data_tmp.substring(index1, index2);
            return true;
        }
    }
    data = "";
    return false;
}

bool ESP8266::recvFindAndFilter_dbg(String target, String begin, String end, String &data, uint32_t timeout)
{
    String data_tmp;
    data_tmp = recvString(target, timeout);
    Console.printf("Rxed: %s\r\n", data_tmp.c_str());
    if (data_tmp.indexOf(target) != -1) {
        int32_t index1 = data_tmp.indexOf(begin);
        int32_t index2 = data_tmp.indexOf(end);
        Console.printf("Rxed index 1 vs index 2: %d %d\r\n", index1, index2);
        if (index1 != -1 && index2 != -1) {
            index1 += begin.length();
            data = data_tmp.substring(index1, index2);
            return true;
        }
    }
    data = "";
    return false;
}

bool ESP8266::eAT(void)
{
    rx_empty();
    m_puart->println("AT");
    return recvFind("OK");
}

bool ESP8266::eATRST(void) 
{
    rx_empty();
    m_puart->println("AT+RST");
    return recvFind("OK");
}

bool ESP8266::eATGMR(String &version)
{
    rx_empty();
    m_puart->println("AT+GMR");
    return recvFindAndFilter("OK", "\r\r\n", "\r\n\r\nOK", version); 
//    return recvFindAndFilter_dbg("OK", "\r\r\n", "\r\n\r\nOK", version); 
}

bool ESP8266::qATCWMODE(uint8_t *mode) 
{
    String str_mode;
    bool ret;
    if (!mode) {
        return false;
    }
    rx_empty();
    m_puart->println("AT+CWMODE?");
    ret = recvFindAndFilter("OK", "+CWMODE:", "\r\n\r\nOK", str_mode); 
    if (ret) {
        *mode = (uint8_t)str_mode.toInt();
        return true;
    } else {
        return false;
    }
}

bool ESP8266::sATCWMODE(uint8_t mode)
{
    String data;
    rx_empty();
    m_puart->print("AT+CWMODE=");
    m_puart->println(mode);
    
    data = recvString("OK", "no change");
    if (data.indexOf("OK") != -1 || data.indexOf("no change") != -1) {
        return true;
    }
    return false;
}

bool ESP8266::sATCWJAP(String ssid, String pwd)
{
    String data;
    rx_empty();
    m_puart->print("AT+CWJAP=\"");
    m_puart->print(ssid);
    m_puart->print("\",\"");
    m_puart->print(pwd);
    m_puart->println("\"");
    
    data = recvString("OK", "FAIL", 10000);
    if (data.indexOf("OK") != -1) {
        return true;
    }
    return false;
}

bool ESP8266::sATCWDHCP(uint8_t mode, boolean enabled)
{
    String strEn = "0";
    if (enabled) {
        strEn = "1";
    }

    String data;
    rx_empty();
    m_puart->print("AT+CWDHCP=");
    m_puart->print(strEn);
    m_puart->print(",");
    m_puart->println(mode);
    
    data = recvString("OK", "FAIL", 10000);
    if (data.indexOf("OK") != -1) {
        return true;
    }
    return false;
}

bool ESP8266::eATCWLAP(String &list)
{
    String data;
    rx_empty();
    m_puart->println("AT+CWLAP");
    return recvFindAndFilter("OK", "\r\r\n", "\r\n\r\nOK", list, 10000);
}

bool ESP8266::eATCWQAP(void)
{
    String data;
    rx_empty();
    m_puart->println("AT+CWQAP");
    return recvFind("OK");
}

bool ESP8266::sATCWSAP(String ssid, String pwd, uint8_t chl, uint8_t ecn)
{
    String data;
    rx_empty();
    m_puart->print("AT+CWSAP=\"");
    m_puart->print(ssid);
    m_puart->print("\",\"");
    m_puart->print(pwd);
    m_puart->print("\",");
    m_puart->print(chl);
    m_puart->print(",");
    m_puart->println(ecn);
    
    data = recvString("OK", "ERROR", 5000);
    if (data.indexOf("OK") != -1) {
        return true;
    }
    return false;
}

bool ESP8266::eATCWLIF(String &list)
{
    String data;
    rx_empty();
    m_puart->println("AT+CWLIF");
    return recvFindAndFilter("OK", "\r\r\n", "\r\n\r\nOK", list);
}
bool ESP8266::eATCIPSTATUS(String &list)
{
    String data;
    delay(100);
    rx_empty();
    m_puart->println("AT+CIPSTATUS");
    return recvFindAndFilter("OK", "\r\r\n", "\r\n\r\nOK", list);
}
bool ESP8266::sATCIPSTARTSingle(String type, String addr, uint32_t port)
{
    String data;
    rx_empty();
    m_puart->print("AT+CIPSTART=\"");
    m_puart->print(type);
    m_puart->print("\",\"");
    m_puart->print(addr);
    m_puart->print("\",");
    m_puart->println(port);
    
    data = recvString("OK", "ERROR", "ALREADY CONNECT", 10000);
    if (data.indexOf("OK") != -1 || data.indexOf("ALREADY CONNECT") != -1) {
        return true;
    }
    return false;
}
bool ESP8266::sATCIPSTARTMultiple(uint8_t mux_id, String type, String addr, uint32_t port)
{
    String data;
    rx_empty();
    m_puart->print("AT+CIPSTART=");
    m_puart->print(mux_id);
    m_puart->print(",\"");
    m_puart->print(type);
    m_puart->print("\",\"");
    m_puart->print(addr);
    m_puart->print("\",");
    m_puart->println(port);
    
    data = recvString("OK", "ERROR", "ALREADY CONNECT", 10000);
    if (data.indexOf("OK") != -1 || data.indexOf("ALREADY CONNECT") != -1) {
        return true;
    }
    return false;
}
bool ESP8266::sATCIPSENDSingle(const uint8_t *buffer, uint32_t len)
{
    rx_empty();
    m_puart->print("AT+CIPSEND=");
    m_puart->println(len);
    if (recvFind(">", 5000)) {
        rx_empty();
        for (uint32_t i = 0; i < len; i++) {
            m_puart->write(buffer[i]);
        }
        return recvFind("SEND OK", 10000);
    }
    return false;
}

bool ESP8266::setupTransmission(uint8_t mux_id, uint32_t len) {
    m_puart->print("AT+CIPSEND=");
    m_puart->print(mux_id);
    m_puart->print(",");
    m_puart->println(len);
    return true;
}

bool ESP8266::transmit(const char *buffer, uint32_t len) {
    for (uint32_t i = 0; i < len; i++) {
        m_puart->write(buffer[i]);
    }
    return true;
}

bool ESP8266::sATCIPSENDMultiple(uint8_t mux_id, const uint8_t *buffer, uint32_t len)
{
    rx_empty();
    m_puart->print("AT+CIPSEND=");
    m_puart->print(mux_id);
    m_puart->print(",");
    m_puart->println(len);

    bool printed = false;
    char c = 0;
    int count = 0;
    tmpBufIndex = 0;
    while(c != '>') {
        count++;
        if (!m_puart->available() && count < 10000) { 
            continue;
        }

        if (count > 10000) {
            tmpBuf[sizeof(tmpBuf)-1]='\0';
            Console.printf("Giving up TX attempt resp [%s] \r\n", tmpBuf);
            return false;
        }

        // FIXME this is a problem, can't read other mux's data
        if (m_puart->available() ) {
            c = m_puart->read();
            if (tmpBufIndex < sizeof(tmpBuf)-2) {
                tmpBuf[tmpBufIndex++] = c;
            }
            else {
                tmpBuf[sizeof(tmpBuf)-1]='\0';
                tmpBufIndex=0;
                Console.printf("TX resp [%s] \r\n", tmpBuf);
            }
        }
        //Console.printf("READ DATA BEFORE SEND {%c}\r\n", c);
    }

    tmpBuf[sizeof(tmpBuf)-1]='\0';
    tmpBufIndex=0;
    Console.printf("TX resp [%s] \r\n", tmpBuf);
//    
//    FIXME: THIS is dubious.. ((char*)buffer)[len] = '\0';
//     Console.printf("TX DATA LENGTH {%d}\r\n", len);
//     Console.printf("TX DATA        {%s}\r\n", buffer);

    for (uint32_t i = 0; i < len; i++) {
        //char c = (buffer[i] < ' ' || buffer[i] > '~') ? '?' : buffer[i];
        //          Console.printf("TX DATA {%c} {%x}\r\n", c, buffer[i]); 
        m_puart->write(buffer[i]);
    }
    return true;
}

bool ESP8266::sATCIPCLOSEMulitple(uint8_t mux_id)
{
    String data;
    rx_empty();
    m_puart->print("AT+CIPCLOSE=");
    m_puart->println(mux_id);

    return false;
}
bool ESP8266::eATCIPCLOSESingle(void)
{
    rx_empty();
    m_puart->println("AT+CIPCLOSE");
    return recvFind("OK", 5000);
}

bool ESP8266::eATCIFSR(String &list)
{
    rx_empty();
    m_puart->println("AT+CIFSR");
    return recvFindAndFilter("OK", "\r\r\n", "\r\n\r\nOK", list);
}

bool ESP8266::eAT_CIPAP(String &list)
{
    rx_empty();
    m_puart->println("AT+CIPAP_CUR?");
    return recvFindAndFilter_dbg("OK", "ip:\"", "\"\r\n", list);
}

bool ESP8266::sATCIPMUX(uint8_t mode)
{
    String data;
    rx_empty();
    m_puart->print("AT+CIPMUX=");
    m_puart->println(mode);

    data = recvString("OK", "Link is builded");
    if (data.indexOf("OK") != -1) {
        return true;
    }
    return false;
}

bool ESP8266::SpecialBaud()
{
    String data;
    rx_empty();
    m_puart->print("AT+UART_CUR=115200,8,1,0,3");
    m_puart->println("");

    data = recvString("OK", "Link is builded");
    if (data.indexOf("OK") != -1) {
        return true;
    }
    return false;
}

bool ESP8266::sATCIPSERVER(uint8_t mode, uint32_t port)
{
    String data;
    if (mode) {
        rx_empty();
        m_puart->print("AT+CIPSERVER=1,");
        m_puart->println(port);

        data = recvString("OK", "no change");
        if (data.indexOf("OK") != -1 || data.indexOf("no change") != -1) {
            return true;
        }
        return false;
    } else {
        rx_empty();
        m_puart->println("AT+CIPSERVER=0");
        return recvFind("\r\r\n");
    }
}

bool ESP8266::sATCIPSTO(uint32_t timeout)
{
    rx_empty();
    m_puart->print("AT+CIPSTO=");
    m_puart->println(timeout);
    return recvFind("OK");
}


#define IPD_HEADER "+IPD,"
#define IPD_HEADER_LEN 5


int recv_state = 0;
int tx_state = 0;
int rx_iter = 0;
int tx_iter = 0;

void reset_rx_ctx() {
    ctx.state = NEW_CMD;
    ctx.iter = 0;
    ctx.ipd_length = 0;
    ctx.ipd_mux    = 0;
    ctx.ipd_mux_term_index = 0;
    ctx.ipd_header_length = 0;
}

void reset_tx_ctx() {
    ctx_tx.state = READY;
    ctx_tx.requested_tx_len = 0;
    ctx_tx.mux_id = 0;
    ctx_tx.failed = false;
    ctx_tx.reason[0] = '\0';
    //memset(ctx_tx.reason, 0, sizeof(ctx_tx.reason));
}

void ESP8266::softReset() {
    reset_tx_ctx();
    reset_rx_ctx();
    for (int i =0; i < 5; i++) {
        memset(&(GConnects[i]),  0, sizeof(connection_t));
    }
}

void ESP8266::super_recv(void) {
    //Console.printf("ENTER super recv\r\n");
    connection_t* cn = NULL;
    char c = 0;
    char* parser = NULL;
    Threads::Scope m(_lock);
    //Console.printf("    locked wifi\r\n");

    rx_iter = (rx_iter + 1) % 16;
    if (m_puart->available() <= 0) {
        goto done;
    }
    //Console.printf("Console available \r\n", ctx.msg.data);

    /* Overflow */
    if (ctx.iter >= sizeof(ctx.buf)-1) {
    Console.printf("    ctx reset?\r\n");
        reset_rx_ctx();
    }

    c = m_puart->read();
    ctx.buf[ctx.iter++] = c;

    switch (ctx.state) {
        case NEW_CMD:
            ctx.state = (c == '+' ? IPD_STATUS: STATUS);
            if (ctx.state == IPD_MUX) {
               // Console.printf("RX looking for IPD\r\n");
            }
            else {
                // Console.printf("RX looking for STATUS\r\n");
            }

            break;
        case STATUS:
            //Console.printf("RX looking for new status\r\n");
            // if (ctx.iter < 2) { goto done; }
            if (ctx.iter < 2) {
                goto done;
            }

            if (strncmp(ctx.buf+ctx.iter-2, "> ", 2) == 0) {
                Console.printf("GOT PROMPT FOR TX!!!! \r\n");
                ctx_tx.lock.lock();
                ctx_tx.state = TRANSMIT;
                ctx_tx.lock.unlock();
                reset_rx_ctx();
                break;
            }

            if (strncmp(ctx.buf+ctx.iter-2, "\r\n", 2) != 0) {
                goto done; 
            }

            if (ctx.iter == 2 ) { reset_rx_ctx(); goto done; }

            if (ctx.iter ==  9 && 0 == strncmp(ctx.buf, "SEND OK\r\n", 9)) {
                Console.printf("Transfer complete!\r\n");
                ctx_tx.lock.lock();
                ctx_tx.state = TRANSMISSION_COMPLETE;
                ctx_tx.lock.unlock();
            }
            else if (ctx.iter == 11 && 0 == strncmp(ctx.buf, "SEND FAIL\r\n", 9))
            {
                Console.printf("Generic transmission failure!\r\n");
                ctx_tx.lock.lock();
                ctx_tx.state = TRANSMISSION_COMPLETE;
                ctx_tx.failed = true;
                strncpy(ctx_tx.reason, "Generic transmission failure", sizeof(ctx_tx.reason));
                ctx_tx.lock.unlock();
            }
            else if (ctx.iter == 12 && 0 == strcmp(ctx.buf, "busy p....")) {
                Console.printf("Transmission failure, chip busy!\r\n");
                ctx_tx.lock.lock();
                ctx_tx.state = TRANSMISSION_COMPLETE;
                ctx_tx.failed = true;
                strncpy(ctx_tx.reason, "Chip busy", sizeof(ctx_tx.reason));
                ctx_tx.lock.unlock();
            }

            ctx.buf[ctx.iter] = '\0';

            if (ctx.buf[0] != 'A') {
                *(strstr(ctx.buf, "\r"))=' ';
                *(strstr(ctx.buf, "\n"))=' ';
                Console.printf("RX last 0x%x statusLen: [%u]\r\n", ctx.buf[ctx.iter-2], strlen(ctx.buf));
                Console.printf("   Status: [%s]\r\n", ctx.buf);
                reset_rx_ctx();
                break;
            }

            if (ctx.iter-1 >= 11 && !strstr(ctx.buf, "AT+CIPSEND=")) {
                reset_rx_ctx();
                break;
            }

            parser = strstr(ctx.buf, "\r\n\r\n");
            if (!parser) {
                goto done;
            }

            parser += 4;
            if (parser == ctx.buf+ctx.iter) {
                goto done;
            }
            *(strstr(parser, "\r"))=' ';
            *(strstr(parser, "\n"))=' ';

            ctx_tx.lock.lock();
            if (strncmp(parser, "OK", 2) == 0) {
                ctx_tx.failed = false;
            }
            else if (strstr(ctx.buf, "link is not valid") != 0) {
                ctx_tx.failed = true;
                strncpy(ctx_tx.reason, "link is not valid", sizeof(ctx_tx.reason));
                reset_rx_ctx();
            }
            else {
                ctx_tx.failed = true;
                strncpy(ctx_tx.reason, "Generic transmission failure", sizeof(ctx_tx.reason));
                reset_rx_ctx();
            }

            Console.printf("CIPSEND STATUS: [%s]\r\n", ctx.buf);
            ctx_tx.lock.unlock();
            goto done;
            break;

        case IPD_STATUS:
            ctx.state = (c == 'I' ? IPD_MUX: STATUS);
            break;
        case IPD_MUX:
            if (ctx.iter-1 < (uint16_t)IPD_HEADER_LEN || c != ',') { goto done; }

            ctx.ipd_mux_term_index = ctx.iter-1;
            ctx.buf[ctx.iter] = '\0';

            /*
            Console.printf("RX looking for mux %s\r\n", ctx.buf);
            Console.printf("    ctx.iter %x\r\n", ctx.iter);
            Console.printf("    ipd.iter %x", IPD_HEADER_LEN);
            */
            ctx.buf[ctx.iter-1] = '\0';

            ctx.ipd_mux = atoi(ctx.buf+IPD_HEADER_LEN);

            //Console.printf("RX mux 0x%x vs %s\r\n", ctx.ipd_mux, ctx.buf+IPD_HEADER_LEN);
            ctx.buf[ctx.iter-1] = c;

            ctx.state = IPD_LENGTH;
            break;
        case IPD_LENGTH:
            if (c != ':') { goto done; }

            ctx.buf[ctx.iter-1] = '\0';
            ctx.ipd_length = atoi(ctx.buf+ctx.ipd_mux_term_index+1);
//            Console.printf("RX length 0x%x vs %s\r\n", ctx.ipd_length, ctx.buf);
            ctx.buf[ctx.iter-1] = c;

            ctx.ipd_header_length = ctx.iter-1;
            ctx.state = IPD_FRAME;
            break;
        case IPD_FRAME:
            cn= &(GConnects[ctx.ipd_mux]);
            if ((ctx.iter-1) - ctx.ipd_header_length < ctx.ipd_length) { 
                //Console.printf("RX IPD_FRAME CXN {%d} READ BYTES {%d} expected {%d}\r\n", ctx.ipd_mux, (ctx.iter-1)-ctx.ipd_header_length, ctx.ipd_length);
                goto done; 
            }

            //Console.printf("RX RECV CXN {%d} LEN {%d}\r\n", ctx.ipd_mux, ctx.ipd_length);
            //Console.printf("lock IPD %d\r\n", ctx.ipd_mux);
            cn->lock.lock();
            memcpy(cn->rx_data+cn->rx_len, ctx.buf+ctx.ipd_header_length+1, ctx.ipd_length);
            cn->rx_len += ctx.ipd_length;
            //Console.printf("RX MUX {%d} RECV MESSGE OF LEN {%d}\r\n", ctx.ipd_mux, cn->rx_len);
            cn->lock.unlock();
            //Console.printf("unlock IPD %d\r\n", ctx.ipd_mux);


            //Console.printf("RX RECV CXN {%d} LEN {%d}\r\n", ctx.ipd_mux, ctx.ipd_length);
            reset_rx_ctx();
            break;

        default:
            break;
    }


done:
    //Console.printf("EXIT super recv\r\n");
    recv_state = ctx.state;
    return;
}


#define TX_CHUNK_LEN 512
void ESP8266::stateful_tx(void) {
    tx_iter = (tx_iter + 1) % 16;
    Threads::Scope m(_lock);
    Threads::Scope m_tx(ctx_tx.lock);

    if (ctx.state != NEW_CMD) {
        //Console.printf("Reading command, give up on transmission\r\n");
        return;
    }

    bool tx_necessary = false;

    uint8_t mux_id = 0;
    for (mux_id =0; mux_id < 5; mux_id++) {
        connection_t* cxn = &GConnects[mux_id];
        cxn->tx_lock.lock();
        if (cxn->tx_len > 0) {
            tx_necessary = true;
            cxn->tx_lock.unlock();
            break;
        }
        cxn->tx_lock.unlock();
    }

    if (tx_necessary) {
        connection_t* cxn = &GConnects[ctx_tx.mux_id];
        uint16_t remain= 0 ;
        uint16_t len = 0;
        switch (ctx_tx.state) {
            case READY:
                ctx_tx.mux_id = mux_id;
                cxn = &GConnects[ctx_tx.mux_id];
                cxn->tx_lock.lock();
                len = cxn->tx_len < TX_CHUNK_LEN ? cxn->tx_len : TX_CHUNK_LEN;
                cxn->tx_lock.unlock();

                //Console.printf("*** TX data chunk %d..\n", len);

                ctx_tx.requested_tx_len = len;
                setupTransmission(mux_id, len);
                ctx_tx.state = WAIT_FOR_TRANSMISSION;
                break;

            case TRANSMIT:
                cxn->tx_lock.lock();
                Console.printf("*** Attempt TX with offset %d!\r\n", ctx_tx.wrote);
                if (!transmit(cxn->tx_data+ctx_tx.wrote, ctx_tx.requested_tx_len)) {
                    cxn->tx_lock.unlock();
                    break;
                }
                cxn->tx_lock.unlock();
                ctx_tx.state = WAIT_FOR_TRANSMISSION_RESULT;
                Console.printf("*** Attempt TX with offset %d complete, now wait!\r\n", ctx_tx.wrote);
                break;

            case TRANSMISSION_COMPLETE:
                if (ctx_tx.failed) {
                    Console.printf("FAILED TX %d bytes, reason: %s\r\n", ctx_tx.requested_tx_len, ctx_tx.reason);
                }

                cxn->tx_lock.lock();
                remain = cxn->tx_len - ctx_tx.requested_tx_len;
                ctx_tx.wrote += ctx_tx.requested_tx_len;

                //Console.printf("*** Moving %d bytes of data offset from..%d\n", remain, ctx_tx.requested_tx_len);
                //memmove(cxn->tx_data, cxn->tx_data+ctx_tx.requested_tx_len, remain);
                cxn->tx_len = remain;
                if (remain == 0) {
                    //Console.printf("*** Freeing data..\n");
                    ctx_tx.wrote = 0;
                    //free(cxn->tx_data);
                }
                cxn->tx_lock.unlock();

                //Console.printf("success TX %d bytes, buffer size: %d!\n", ctx_tx.requested_tx_len, cxn->tx_len);
                reset_tx_ctx();
                break;

            case WAIT_FOR_TRANSMISSION_RESULT:
            case WAIT_FOR_TRANSMISSION:
            default:
            if (ctx_tx.failed) {
                Console.printf("FAILED TX SETUP %d bytes, reason: %s\n", ctx_tx.requested_tx_len, ctx_tx.reason);
                if (strstr(ctx_tx.reason, "link is not valid")) {
                    ctx_tx.state = TRANSMISSION_COMPLETE;
                }
                else {
                    reset_tx_ctx();
                }
            }
           break;
        }
    }

    tx_state = ctx_tx.state;
}


bool ESP8266::super_recv_mux_done(recv_msg_t* msg) {
    bool ret = false;
    for (int i =0; i < 5; i++) {
        connection_t* cxn = &GConnects[i];
        char* loc;
        uint16_t  len;
        uint16_t  remain;
        cxn->lock.lock();
        if (cxn->rx_len <= 0) { 
            //Console.printf("Connection %d rx length is zero\r\n", i);
            cxn->lock.unlock();
            continue;
        }
        //Console.printf("Processing request on Connection %d\r\n", i);

        cxn->rx_data[cxn->rx_len] = '\0';
        loc = strstr(cxn->rx_data, "\r\n\r\n");
        if (loc == NULL ) { 
            Console.printf("Couldn't find HTTP terminator.. %s\r\n", cxn->rx_data);
            Console.printf("rx_data[len-8] %c %x\r\n", cxn->rx_data[cxn->rx_len-8], cxn->rx_data[cxn->rx_len-8]);
            Console.printf("rx_data[len-7] %c %x\r\n", cxn->rx_data[cxn->rx_len-7], cxn->rx_data[cxn->rx_len-7]);
            Console.printf("rx_data[len-6] %c %x\r\n", cxn->rx_data[cxn->rx_len-6], cxn->rx_data[cxn->rx_len-6]);
            Console.printf("rx_data[len-5] %c %x\r\n", cxn->rx_data[cxn->rx_len-5], cxn->rx_data[cxn->rx_len-5]);
            Console.printf("rx_data[len-4] %c %x\r\n", cxn->rx_data[cxn->rx_len-4], cxn->rx_data[cxn->rx_len-4]);
            Console.printf("rx_data[len-3] %c %x\r\n", cxn->rx_data[cxn->rx_len-3], cxn->rx_data[cxn->rx_len-3]);
            Console.printf("rx_data[len-2] %c %x\r\n", cxn->rx_data[cxn->rx_len-2], cxn->rx_data[cxn->rx_len-2]);
            Console.printf("rx_data[len-1] %c %x\r\n", cxn->rx_data[cxn->rx_len-1], cxn->rx_data[cxn->rx_len-1]);
            Console.printf("rx_data[len] %c %x\r\n"  , cxn->rx_data[cxn->rx_len], cxn->rx_data[cxn->rx_len]);
            cxn->rx_len = 0;
            cxn->lock.unlock();
            continue;
        }

        ret = true;
        len  = loc+4-cxn->rx_data;
        memcpy(msg->data, cxn->rx_data, len);
        msg->len = len;
        msg->mux = i;

        remain = cxn->rx_len -len;
        if (remain > 0) {
            memmove(cxn->rx_data, cxn->rx_data+len, remain);
        }
        cxn->rx_len = remain;
        //Console.printf("Connection[%d] rx_len {%d} vs calculated len {%d} \r\n", i, cxn->rx_len, len);
        cxn->lock.unlock();
        return ret;
    }
    return ret;
}

String ESP8266::runCommand(const String& cmd) {
    unsigned long start = millis();

    while (millis() - start < 5000) {
        _lock.lock();
        if (ctx.state == NEW_CMD && ctx_tx.state == READY) {
            break;
        }
        _lock.unlock();
    }

    m_puart->println(cmd.c_str());
    String ret = recvString("OK\r\n");

    _lock.unlock();
    Console.printf("AT COMMAND COMPLETE!\r\n");
    return ret;
}
