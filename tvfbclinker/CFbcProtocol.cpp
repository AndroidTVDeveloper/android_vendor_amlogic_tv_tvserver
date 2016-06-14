#define LOG_NDEBUG 0

#define LOG_TAG "tvserver"
#define LOG_TV_TAG "CFbcProtocol"

#include "CFbcProtocol.h"
#include <time.h>
#include <CTvLog.h>


static CFbcProtocol *mInstace = NULL;
CFbcProtocol *GetFbcProtocolInstance()
{
    if (mInstace == NULL) {
        mInstace = new CFbcProtocol();
        mInstace->start();
    }

    return mInstace;
}

CFbcProtocol::CFbcProtocol()
{
    initCrc32Table();

    m_event.data.fd = -1;
    m_event.events = EPOLLIN | EPOLLET;

    mpRevDataBuf = new unsigned char[512];

    mUpgradeFlag = 0;
    mbDownHaveSend = 0;//false
    //mFbcMsgQueue.startMsgQueue();
    mbFbcKeyEnterDown = 0;//false
    mFbcEnterKeyDownTime = -1;
}

CFbcProtocol::~CFbcProtocol()
{
    m_event.data.fd = mSerialPort.getFd();
    m_event.events = EPOLLIN | EPOLLET;
    mEpoll.del(mSerialPort.getFd(), &m_event);
    closeAll();
    delete[] mpRevDataBuf;
}

int CFbcProtocol::start()
{
    //int serial_port = config_get_int("TV", "fbc.communication.serial", SERIAL_C);
    if (mSerialPort.OpenDevice(SERIAL_C) < 0) {
    } else {
        LOGD("%s %d be called......\n", __FUNCTION__, __LINE__);
        mSerialPort.setup_serial();
    }

    if (mEpoll.create() < 0) {
        return -1;
    }

    m_event.data.fd = mSerialPort.getFd();
    m_event.events = EPOLLIN | EPOLLET;
    mEpoll.add(mSerialPort.getFd(), &m_event);

    //timeout for long
    mEpoll.setTimeout(3000);

    this->run();
    return 0;
}

void CFbcProtocol::testUart()
{
    unsigned char write_buf[64], read_buf[64];
    int idx = 0;
    memset(write_buf, 0, sizeof(write_buf));
    memset(read_buf, 0, sizeof(read_buf));

    write_buf[0] = 0x5a;
    write_buf[1] = 0x5a;
    write_buf[2] = 0xb;
    write_buf[3] = 0x0;
    write_buf[4] = 0x0;
    write_buf[5] = 0x40;
    write_buf[6] = 0x0;

    write_buf[7] = 0x2;
    write_buf[8] = 0x3c;
    write_buf[9] = 0x75;
    write_buf[10] = 0x30;
    //LOGD("to write ..........\n");
    mSerialPort.writeFile(write_buf, 11);
    sleep(1);
    //LOGD("to read........\n");
    mSerialPort.readFile(read_buf, 12);
    for (idx = 0; idx < 12; idx++)
        LOGD("the data is:0x%x\n", read_buf[idx]);
    LOGD("end....\n");
}

void CFbcProtocol::showTime(struct timeval *_time)
{
    struct timeval curTime;

    if (_time == NULL) {
        gettimeofday(&curTime, NULL);
    } else {
        curTime.tv_sec = _time->tv_sec;
        curTime.tv_usec = _time->tv_usec;
    }
    if (curTime.tv_usec > 100000) {
        LOGD("[%ld.%ld]", curTime.tv_sec, curTime.tv_usec);
    } else if (curTime.tv_usec > 10000) {
        LOGD("[%ld.0%ld]", curTime.tv_sec, curTime.tv_usec);
    } else if (curTime.tv_usec > 1000) {
        LOGD("[%ld.00%ld]", curTime.tv_sec, curTime.tv_usec);
    } else if (curTime.tv_usec > 100) {
        LOGD("[%ld.000%ld]", curTime.tv_sec, curTime.tv_usec);
    } else if (curTime.tv_usec > 10) {
        LOGD("[%ld.0000%ld]", curTime.tv_sec, curTime.tv_usec);
    } else if (curTime.tv_usec > 1) {
        LOGD("[%ld.00000%ld]", curTime.tv_sec, curTime.tv_usec);
    }
}

long CFbcProtocol::getTime(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

void CFbcProtocol::initCrc32Table()
{
    int i, j;
    unsigned int Crc;
    for (i = 0; i < 256; i++) {
        Crc = i;
        for (j = 0; j < 8; j++) {
            if (Crc & 1)
                Crc = (Crc >> 1) ^ 0xEDB88320;
            else
                Crc >>= 1;
        }
        mCrc32Table[i] = Crc;
    }
}

void CFbcProtocol::sendAckCmd(bool isOk)
{
    int crc32value = 0;
    unsigned char ackcmd[12];
    ackcmd[0] = 0x5A;
    ackcmd[1] = 0x5A;
    ackcmd[2] = 12;//little endian
    ackcmd[3] = 0x00;
    ackcmd[4] = 0x80;//ack flag
    ackcmd[5] = 0xff;//cmd id
    if (isOk) {
        ackcmd[6] = 0xfe;
        ackcmd[7] = 0x7f;
    } else {
        ackcmd[6] = 0x80;
        ackcmd[7] = 0x01;
    }
    //*((unsigned int*) (ackcmd + 8)) = GetCrc32(ackcmd, 8);
    crc32value = Calcrc32(0, ackcmd, 8);
    ackcmd[8] = (crc32value >> 0) & 0xFF;
    ackcmd[9] = (crc32value >> 8) & 0xFF;
    ackcmd[10] = (crc32value >> 16) & 0xFF;
    ackcmd[11] = (crc32value >> 24) & 0xFF;
    LOGD("to send ack and crc is:0x%x\n", crc32value);
    sendDataOneway(COMM_DEV_SERIAL, ackcmd, 12, 0x00000000);
}

/*  函数名：GetCrc32
*  函数原型：unsigned int GetCrc32(char* InStr,unsigned int len)
*      参数：InStr  ---指向需要计算CRC32值的字符? *          len  ---为InStr的长帿 *      返回值为计算出来的CRC32结果? */
unsigned int CFbcProtocol::GetCrc32(unsigned char *InStr, unsigned int len)
{
    //开始计算CRC32校验便
    unsigned int Crc = 0xffffffff;
    for (int i = 0; i < (int)len; i++) {
        Crc = (Crc >> 8) ^ mCrc32Table[(Crc & 0xFF) ^ InStr[i]];
    }

    Crc ^= 0xFFFFFFFF;
    return Crc;
}

unsigned int CFbcProtocol::Calcrc32(unsigned int crc, const unsigned char *ptr, unsigned int buf_len)
{
    static const unsigned int s_crc32[16] = { 0, 0x1db71064, 0x3b6e20c8, 0x26d930ac, 0x76dc4190, 0x6b6b51f4,
                                        0x4db26158, 0x5005713c, 0xedb88320, 0xf00f9344, 0xd6d6a3e8,
                                        0xcb61b38c, 0x9b64c2b0, 0x86d3d2d4, 0xa00ae278, 0xbdbdf21c};
    unsigned int crcu32 = crc;
    //if (buf_len < 0)
    //    return 0;
    if (!ptr) return 0;
    crcu32 = ~crcu32;
    while (buf_len--) {
        unsigned char b = *ptr++;
        crcu32 = (crcu32 >> 4) ^ s_crc32[(crcu32 & 0xF) ^ (b & 0xF)];
        crcu32 = (crcu32 >> 4) ^ s_crc32[(crcu32 & 0xF) ^ (b >> 4)];
    }
    return ~crcu32;
}

int CFbcProtocol::sendDataOneway(int devno, unsigned char *pData, int dataLen, int flags __unused)
{
    int ret = -1;
    if (COMM_DEV_CEC == devno)
        ret = mHdmiCec.writeFile(pData, dataLen);
    else if (COMM_DEV_SERIAL == devno)
        ret = mSerialPort.writeFile(pData, dataLen);
    return ret;
}

//timeout ms
int CFbcProtocol::sendDataAndWaitReply(int devno, int waitForDevno, int waitForCmd, unsigned char *pData, int dataLen, int timeout, unsigned char *pReData, int *reDataLen, int flags)
{
    int ret = sendDataOneway(devno, pData, dataLen, flags);
    if (ret < 0) return ret;

    mReplyList.WaitDevNo = waitForDevno;
    mReplyList.WaitCmd = waitForCmd;
    mReplyList.WaitTimeOut = timeout;
    mReplyList.reDataLen = 0;
    mReplyList.replyData = mpRevDataBuf;
    LOGD("wait dev:%d, cmd:0x%x, timeout:%d", mReplyList.WaitDevNo, mReplyList.WaitCmd, mReplyList.WaitTimeOut);

    mLock.lock();
    ret = mReplyList.WaitReplyCondition.waitRelative(mLock, timeout);//wait reply
    LOGD("%s, %d, wait reply return = %d", __FUNCTION__, __LINE__, ret);
    mLock.unlock();

    if (mReplyList.reDataLen > 0) { //data have come in
        *reDataLen = mReplyList.reDataLen;
        memcpy(pReData, mReplyList.replyData, mReplyList.reDataLen);
        mReplyList.reDataLen = 0;
        return *reDataLen;
    } else {
        //timeout,not to wait continue
        mReplyList.WaitDevNo = -1;
        mReplyList.WaitCmd = 0xff;
        return -1;//timeout but data not come.
    }
    return 0;
}

int CFbcProtocol::closeAll()
{
    mSerialPort.CloseDevice();
    return 0;
}

int CFbcProtocol::SetUpgradeFlag(int flag)
{
    mUpgradeFlag = flag;
    return 0;
}

int CFbcProtocol::uartReadStream(unsigned char *retData, int rd_max_len, int timeout)
{
    int readLen = 0, bufIndex = 0, haveRead = 0;
    clock_t start_tm = clock();

    do {
        readLen = mSerialPort.readFile(retData + bufIndex, rd_max_len - haveRead);
        haveRead += readLen;
        bufIndex += readLen;
        if (haveRead == rd_max_len) {
            return haveRead;
        }

        LOGD("readLen = %d, haveRead = %d\n", readLen, haveRead);

        if (((clock() - start_tm) / (CLOCKS_PER_SEC / 1000)) > timeout) {
            return haveRead;
        }
    } while (true);

    return haveRead;
}

int CFbcProtocol::uartReadData(unsigned char *retData, int *retLen)
{
    unsigned char tempBuf[512];
    int cmdLen = 0;
    int bufIndex = 0;
    int readLen = 0;

    if (retData == NULL) {
        LOGD("the retData is NULL");
        return 0;
    }

    //leader codes 2 byte
    memset(tempBuf, 0, sizeof(tempBuf));
    do {
        readLen = mSerialPort.readFile(tempBuf + 0, 1);
        if (tempBuf[0] == 0x5A) {
            bufIndex = 1;
            readLen = mSerialPort.readFile(tempBuf + 1, 1);
            if (tempBuf[1] == 0x5A) {
                bufIndex = 2;
                LOGD("leading code coming...");
                break;
            }
        }
    } while (true);

    //data len 2 byte
    int needRead = 2, haveRead = 0;
    do {
        readLen = mSerialPort.readFile(tempBuf + bufIndex, needRead - haveRead);
        haveRead += readLen;
        bufIndex += readLen;
        if (haveRead == needRead) {
            break;
        }
    } while (true);

    //little endian
    cmdLen = (tempBuf[3] << 8) + tempBuf[2];
    //cmd data cmdLen - 2  -2
    needRead = cmdLen - 4, haveRead = 0;
    LOGD("cmdLen is:%d\n", cmdLen);

    do {
        readLen = mSerialPort.readFile(tempBuf + bufIndex, needRead - haveRead);
        haveRead += readLen;
        bufIndex += readLen;
        if (readLen > 0) {
            LOGD("data readLen is:%d\n", readLen);
        }
        if (haveRead == needRead) {
            break;
        }
    } while (true);

    unsigned int crc = 0;
    if (cmdLen > 4) {
        crc = Calcrc32(0, tempBuf, cmdLen - 4);//not include crc 4byte
    }
    unsigned int bufCrc = tempBuf[cmdLen - 4] |
                      tempBuf[cmdLen - 3] << 8 |
                      tempBuf[cmdLen - 2] << 16 |
                      tempBuf[cmdLen - 1] << 24;
    if (crc == bufCrc) {
        memcpy(retData, tempBuf, cmdLen % 512);
        *retLen = cmdLen;
        return cmdLen;
    }

    return -1;
}

int CFbcProtocol::processData(COMM_DEV_TYPE_E fromDev, unsigned char *pData, int dataLen)
{
    __u16 key_code = 0;
    switch (fromDev) {
        case COMM_DEV_CEC: {
            if (mReplyList.WaitDevNo == fromDev && mReplyList.WaitCmd == pData[1]) {
                mReplyList.reDataLen = dataLen;
                memcpy(mReplyList.replyData, pData, dataLen);
                mReplyList.WaitReplyCondition.signal();
            }
        }
        break;

        case COMM_DEV_SERIAL: {
            if (mReplyList.WaitDevNo == fromDev && mReplyList.WaitCmd == pData[5]) {
                mReplyList.reDataLen = dataLen;
                memcpy(mReplyList.replyData, pData, dataLen);
                mReplyList.WaitReplyCondition.signal();
            } else {
                //unsigned char cmd = pData[5];
                if (mbSendKeyCode) {
                    if (0x14 == pData[5]) {
                        key_code =(key_code | (pData[7]<<8)) | pData[6];
                        LOGD("to signal wait dataLen:0x%x, cmdId:0x%x , ,key:%d\n", dataLen, pData[5],key_code);
                        mCVirtualInput.sendVirtualkeyEvent(key_code);
                    }
                }
            }
        }
        break;
    }
    return 0;
}

bool CFbcProtocol::threadLoop()
{
    unsigned char readFrameBuf[512];
    while (!exitPending()) { //requietexit() or requietexitWait() not call
        while (mUpgradeFlag == 1) {
            usleep(1000 * 1000);
        }

        int num = mEpoll.wait();

        while (mUpgradeFlag == 1) {
            usleep(1000 * 1000);
        }

        for (int i = 0; i < num; ++i) {
            int fd = (mEpoll)[i].data.fd;
            /**
             * EPOLLIN event
             */
            if ((mEpoll)[i].events & EPOLLIN) {
                if (fd == mHdmiCec.getFd()) { //ce-----------------------------c
                    int bufIndex = 0;
                    int needRead = 4;
                    int haveRead = 0;
                    int idx = 0, readLen = 0;
                    do {
                        readLen = mHdmiCec.readFile(readFrameBuf + bufIndex,  needRead - haveRead);
                        haveRead += readLen;
                        bufIndex += readLen;
                        //if(haveRead == needRead) break;
                    } while (0);

                    if (readLen > 0) {
                        processData(COMM_DEV_CEC, readFrameBuf, readLen);
                    } else {
                    }
                } else if (fd == mSerialPort.getFd()) {
                    //seria---------------------------l
                    int cmdLen = 0, idx = 0;
                    LOGD("serial data come");
                    memset(readFrameBuf, 0, 512);
                    int ret = uartReadData(readFrameBuf, &cmdLen);

                    if (ret == -1) { //data error
                        sendAckCmd(false);
                    } else if (readFrameBuf[4] == 0x80) { //ack
                        LOGD("is ack come");
#ifdef TV_RESEND_UMUTE_TO_FBC
                        if (((readFrameBuf[7] << 8) | readFrameBuf[6]) == 0x8001 &&
                                readFrameBuf[5] == AUDIO_CMD_SET_MUTE) {
                            LOGD("resend unmute to 101 avoid 101 receiving unmute timeout\n");
                            Fbc_Set_Value_INT8(COMM_DEV_SERIAL, AUDIO_CMD_SET_MUTE, 1);
                        }
#endif
                    } else { //not ack
                        sendAckCmd(true);
                        processData(COMM_DEV_SERIAL, readFrameBuf, cmdLen);
                    }
                }
            }

            /**
             * EPOLLOUT event
             */
            //if ((mEpoll)[i].events & EPOLLOUT) {
            //}
        }
    }
    //exit
    //return true, run again, return false,not run.
    LOGD("thread exited..........\n");
    return false;
}

int CFbcProtocol::fbcSetBatchValue(COMM_DEV_TYPE_E toDev, unsigned char *cmd_buf, int count)
{
    LOGV("%s, dev_type=%d, cmd_id=%d, count=%d, mUpgradeFlag=%d", __FUNCTION__, toDev, *cmd_buf, count, mUpgradeFlag);
    if (mUpgradeFlag == 1) {
        return 0;
    }

    if ( 512 <= count) {
        return -1;
    }

    if (toDev == COMM_DEV_CEC) {
        unsigned char cmd[512], rxbuf[512];
        int rxlen = 0, idx = 0;
        memset(cmd, 0, 512);
        memset(rxbuf, 0, 512);
        cmd[0] = 0x40;
        for (idx = 0; idx < count; idx++) {
            cmd[idx + 1] = cmd_buf[idx];
        }
        sendDataOneway(COMM_DEV_CEC, cmd, count + 1, 0);
    } else if (toDev == COMM_DEV_SERIAL) {
        int crc32value = 0;
        unsigned char write_buf[512];
        unsigned char rxbuf[512];
        int rxlen = 0, idx = 0;

        //leading code
        write_buf[0] = 0x5a;
        write_buf[1] = 0x5a;
        //package length from begin to end
        write_buf[2] = (count + 9) & 0xFF;
        write_buf[3] = ((count + 9) >> 8) & 0xFF;
        //Ack byte : 0x80-> ack package;0x00->normal package;
        write_buf[4] = 0x00;

        for (idx = 0; idx < count; idx++) {
            write_buf[idx + 5] = cmd_buf[idx];
        }
        //crc32 little Endian
        crc32value = Calcrc32(0, write_buf, count + 5);
        write_buf[count + 5] = (crc32value >> 0) & 0xFF;
        write_buf[count + 6] = (crc32value >> 8) & 0xFF;
        write_buf[count + 7] = (crc32value >> 16) & 0xFF;
        write_buf[count + 8] = (crc32value >> 24) & 0xFF;
        sendDataOneway(COMM_DEV_SERIAL, write_buf, count + 9, 0);
    }
    return 0;
}

int CFbcProtocol::fbcGetBatchValue(COMM_DEV_TYPE_E fromDev, unsigned char *cmd_buf, int count)
{
    LOGV("%s, dev_type=%d, cmd_id=%d, count=%d, mUpgradeFlag=%d", __FUNCTION__, fromDev, *cmd_buf, count, mUpgradeFlag);
    if (mUpgradeFlag == 1) {
        return 0;
    }

    if ( 512 <= count) {
        return -1;
    }
    int ret = 0;
    // TODO: read value
    if (fromDev == COMM_DEV_CEC) {
        unsigned char cmd[512], rxbuf[512];
        int rxlen = 0, idx = 0;
        memset(cmd, 0, 512);
        memset(rxbuf, 0, 512);
        cmd[0] = 0x40;
        cmd[1] = cmd_buf[0];
        sendDataAndWaitReply(COMM_DEV_CEC, COMM_DEV_CEC, cmd[1], cmd, count + 1, 0, rxbuf, &rxlen, 0);

        if (rxlen > 2) {
            for (idx = 0; idx < rxlen; idx++) {
                cmd_buf[idx] = cmd[idx + 1];
            }
        }
    } else if (fromDev == COMM_DEV_SERIAL) {
        int crc32value = 0, idx = 0, rxlen = 0;
        unsigned char write_buf[512];
        unsigned char rxbuf[512];
        memset(write_buf, 0, 512);
        memset(rxbuf, 0, 512);

        //leading code
        write_buf[0] = 0x5a;
        write_buf[1] = 0x5a;
        //package length from begin to end
        write_buf[2] = (count + 9) & 0xFF;
        write_buf[3] = ((count + 9) >> 8) & 0xFF;
        //Ack byte
        write_buf[4] = 0x00;
        //cmd ID
        for (idx = 0; idx < count; idx++) {
            write_buf[idx + 5] = cmd_buf[idx];
        }
        //crc32 little Endian
        crc32value = Calcrc32(0, write_buf, count + 5);
        write_buf[count + 5] = (crc32value >> 0) & 0xFF;
        write_buf[count + 6] = (crc32value >> 8) & 0xFF;
        write_buf[count + 7] = (crc32value >> 16) & 0xFF;
        write_buf[count + 8] = (crc32value >> 24) & 0xFF;
        sendDataAndWaitReply(COMM_DEV_SERIAL, COMM_DEV_SERIAL, write_buf[5], write_buf, write_buf[2], 2000, rxbuf, &rxlen, 0);

        if (rxlen > 9) {
            for (idx = 0; idx < (rxlen - 9); idx++) {
                cmd_buf[idx] = rxbuf[idx + 5];
            }
        }
        ret = rxlen;
    }

    return ret;
}

void CFbcProtocol::CFbcMsgQueue::handleMessage ( CMessage &msg )
{
    LOGD ( "%s, CFbcProtocol::CFbcMsgQueue::handleMessage type = %d", __FUNCTION__,  msg.mType );
    //msg.mType is TV_MSG_COMMON or TV_MSG_SEND_KEY
}

