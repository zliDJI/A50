/** @file DJI_API.cpp
 *  @version 3.1.9
 *  @date November 10, 2016
 *
 *  @brief
 *  Core API for DJI onboardSDK library
 *
 *  @copyright 2016 DJI. All right reserved.
 *
 */

#include "DJI_API.h"
#include <stdio.h>
#include <string.h>
#include "DJI_HardDriver.h"

using namespace DJI;
using namespace DJI::onboardSDK;

#ifdef USE_ENCRYPT
uint8_t DJI::onboardSDK::encrypt = 1;
#else
uint8_t DJI::onboardSDK::encrypt = 0;
#endif  // USE_ENCRYPT

CoreAPI::CoreAPI(HardDriver *sDevice, Version SDKVersion,
                 bool userCallbackThread, CallBack userRecvCallback,
                 UserData userData) {
  CallBackHandler handler;
  handler.callback = userRecvCallback;
  handler.userData = userData;
  init(sDevice, sDevice->getMmu(), handler, userCallbackThread, SDKVersion);
}

void CoreAPI::init(HardDriver *sDevice, MMU *mmuPtr,
                   CallBackHandler userRecvCallback, bool userCallbackThread,
                   Version SDKVersion) {
  serialDevice = sDevice;
  // serialDevice->init();

  seq_num              = 0;
  ackFrameStatus       = 11;
  broadcastFrameStatus = false;

  filter.recvIndex  = 0;
  filter.reuseCount = 0;
  filter.reuseIndex = 0;
  filter.encode     = 0;

  //! @todo replace by Filter
  broadcastCallback.callback     = 0;
  broadcastCallback.userData     = 0;
  fromMobileCallback.callback    = 0;
  fromMobileCallback.userData    = 0;
  hotPointCallback.callback      = 0;
  wayPointCallback.callback      = 0;
  hotPointCallback.userData      = 0;
  wayPointEventCallback.callback = 0;
  wayPointEventCallback.userData = 0;
  wayPointCallback.userData      = 0;
  followCallback.callback        = 0;
  followCallback.userData        = 0;
  missionCallback.callback       = 0;
  missionCallback.userData       = 0;
  testCallback.callback          = 0;
  testCallback.userData          = 0;
  subscribeCallback.callback     = 0;
  subscribeCallback.userData     = 0;

  recvCallback.callback = userRecvCallback.callback;
  recvCallback.userData = userRecvCallback.userData;

  callbackThread = false;
  hotPointData   = false;
  followData     = false;
  wayPointData   = false;
  callbackThread = userCallbackThread;  //! @todo implement

  nonBlockingCBThreadEnable = false;
  ack_data                  = 99;
  versionData.version       = SDKVersion;

  //! @todo simplify code above
  memset((unsigned char *)&broadcastData, 0, sizeof(broadcastData));

  mmu = mmuPtr;

  setup();
}

CoreAPI::CoreAPI(HardDriver *sDevice, Version SDKVersion,
                 CallBackHandler userRecvCallback, bool userCallbackThread) {
  init(sDevice, sDevice->getMmu(), userRecvCallback, userCallbackThread,
       SDKVersion);
  getSDKVersion();
}

void CoreAPI::send(unsigned char session, unsigned char is_enc, CMD_SET cmdSet,
                   unsigned char cmdID, void *pdata, int len,
                   CallBack ackCallback, int timeout, int retry) {
  Command param;
  unsigned char *ptemp = (unsigned char *)encodeSendData;
  *ptemp++             = cmdSet;
  *ptemp++             = cmdID;

  memcpy(encodeSendData + SET_CMD_SIZE, pdata, len);

  param.handler     = ackCallback;
  param.sessionMode = session;
  param.length      = len + SET_CMD_SIZE;
  param.buf         = encodeSendData;
  param.retry       = retry;

  param.timeout = timeout;
  param.encrypt = is_enc;

  param.userData = 0;

  sendInterface(&param);
}

void CoreAPI::send(unsigned char session_mode, bool is_enc, CMD_SET cmd_set,
                   unsigned char cmd_id, void *pdata, size_t len, int timeout,
                   int retry_time, CallBack ack_handler, UserData userData) {
  Command param;
  unsigned char *ptemp = (unsigned char *)encodeSendData;
  *ptemp++             = cmd_set;
  *ptemp++             = cmd_id;

  memcpy(encodeSendData + SET_CMD_SIZE, pdata, len);

  param.handler     = ack_handler;
  param.sessionMode = session_mode;
  param.length      = len + SET_CMD_SIZE;
  param.buf         = encodeSendData;
  param.retry       = retry_time;

  param.timeout = timeout;
  param.encrypt = is_enc ? 1 : 0;

  param.userData = userData;

  sendInterface(&param);
}

void CoreAPI::send(Command *parameter) { sendInterface(parameter); }

void CoreAPI::ack(req_id_t req_id, unsigned char *ackdata, int len) {
  Ack param;

  memcpy(encodeACK, ackdata, len);

  param.sessionID = req_id.session_id;
  param.seqNum    = req_id.sequence_number;
  param.encrypt   = req_id.need_encrypt;
  param.buf       = encodeACK;
  param.length    = len;

  this->ackInterface(&param);
}

void CoreAPI::getDroneVersion(CallBack callback, UserData userData) {
  versionData.version_ack     = ACK_COMMON_NO_RESPONSE;
  versionData.version_crc     = 0x0;
  versionData.version_name[0] = 0;

  unsigned cmd_timeout   = 100;  // unit is ms
  unsigned retry_time    = 3;
  unsigned char cmd_data = 0;

  send(2, 0, SET_ACTIVATION, CODE_GETVERSION, (unsigned char *)&cmd_data, 1,
       cmd_timeout, retry_time,
       callback ? callback : CoreAPI::getDroneVersionCallback, userData);
}

VersionData CoreAPI::getDroneVersion(int timeout) {
  versionData.version_ack     = ACK_COMMON_NO_RESPONSE;
  versionData.version_crc     = 0x0;
  versionData.version_name[0] = 0;

  unsigned cmd_timeout   = 100;  // unit is ms
  unsigned retry_time    = 3;
  unsigned char cmd_data = 0;

  send(2, 0, SET_ACTIVATION, CODE_GETVERSION, (unsigned char *)&cmd_data, 1,
       cmd_timeout, retry_time, 0, 0);

  // Wait for end of ACK frame to arrive
  serialDevice->lockACK();
  serialDevice->wait(timeout);
  serialDevice->freeACK();

  // Parse return value
  unsigned char *ptemp = &(missionACKUnion.droneVersion.ack[0]);

  if (versionData.version != versionA3_32) {
    versionData.version_ack = ptemp[0] + (ptemp[1] << 8);
    ptemp += 2;
    versionData.version_crc =
        ptemp[0] + (ptemp[1] << 8) + (ptemp[2] << 16) + (ptemp[3] << 24);
    ptemp += 4;
    if (versionData.version != versionM100_23) {
      memcpy(versionData.version_ID, ptemp, 11);
      ptemp += 11;
    }
    memcpy(versionData.version_name, ptemp, 32);
  } else {
    versionData.version_ack = missionACKUnion.missionACK;
    memcpy(versionData.version_name, "versionA3_32",
           strlen("versionA3_32") + 1);
  }

  return versionData;
}

void CoreAPI::activate(ActivateData *data, CallBack callback,
                       UserData userData) {
  data->version        = versionData.version;
  accountData          = *data;
  accountData.reserved = 2;

  for (int i             = 0; i < 32; ++i)
    accountData.iosID[i] = '0';  //! @note for ios verification
  API_LOG(serialDevice, DEBUG_LOG, "version 0x%X\n", versionData.version);
  API_LOG(serialDevice, DEBUG_LOG, "%.32s", accountData.iosID);
  send(2, 0, SET_ACTIVATION, CODE_ACTIVATE, (unsigned char *)&accountData,
       sizeof(accountData) - sizeof(char *), 1000, 3,
       callback ? callback : CoreAPI::activateCallback, userData);

  ack_data = missionACKUnion.simpleACK;
  if (ack_data == ACK_ACTIVE_SUCCESS && accountData.encKey)
    setKey(accountData.encKey);
}

unsigned short CoreAPI::activate(ActivateData *data, int timeout) {
  data->version        = versionData.version;
  accountData          = *data;
  accountData.reserved = 2;

  for (int i             = 0; i < 32; ++i)
    accountData.iosID[i] = '0';  //! @note for ios verification
  API_LOG(serialDevice, DEBUG_LOG, "version 0x%X/n", versionData.version);
  API_LOG(serialDevice, DEBUG_LOG, "%.32s", accountData.iosID);
  send(2, 0, SET_ACTIVATION, CODE_ACTIVATE, (unsigned char *)&accountData,
       sizeof(accountData) - sizeof(char *), 1000, 3, 0, 0);

  // Wait for end of ACK frame to arrive
  serialDevice->lockACK();
  serialDevice->wait(timeout);
  serialDevice->freeACK();
  ack_data = missionACKUnion.simpleACK;
  if (ack_data == ACK_ACTIVE_SUCCESS && accountData.encKey)
    setKey(accountData.encKey);

  return ack_data;
}

TimeStampData CoreAPI::getTime() const { return broadcastData.timeStamp; }

FlightStatus CoreAPI::getFlightStatus() const { return broadcastData.status; }

void CoreAPI::setFromMobileCallback(CallBackHandler FromMobileEntrance) {
  fromMobileCallback = FromMobileEntrance;
}

void CoreAPI::setMisssionCallback(CallBackHandler callback) {
  missionCallback = callback;
}

void CoreAPI::setHotPointCallback(CallBackHandler callback) {
  hotPointCallback = callback;
}

void CoreAPI::setWayPointCallback(CallBackHandler callback) {
  wayPointCallback = callback;
}

void CoreAPI::setFollowCallback(CallBackHandler callback) {
  followCallback = callback;
}

void CoreAPI::setSubscribeCallback(CallBack handler, UserData UserData) {
  subscribeCallback.callback = handler;
  subscribeCallback.userData = UserData;
}

void CoreAPI::setSubscribeCallback(CallBackHandler callback) {
  subscribeCallback = callback;
}

ActivateData CoreAPI::getAccountData() const { return accountData; }

void CoreAPI::setAccountData(const ActivateData &value) { accountData = value; }
void CoreAPI::setHotPointData(bool value) { hotPointData = value; }
void CoreAPI::setWayPointData(bool value) { wayPointData = value; }
void CoreAPI::setFollowData(bool value) { followData = value; }
bool CoreAPI::getHotPointData() const { return hotPointData; }
bool CoreAPI::getWayPointData() const { return wayPointData; }
bool CoreAPI::getFollowData() const { return followData; }

HardDriver *CoreAPI::getDriver() const { return serialDevice; }

SimpleACK CoreAPI::getSimpleACK() const { return missionACKUnion.simpleACK; }

void CoreAPI::setDriver(HardDriver *sDevice) { serialDevice = sDevice; }

void CoreAPI::getDroneVersionCallback(CoreAPI *api, Header *protocolHeader,
                                      UserData userData __UNUSED) {
  unsigned char *ptemp = ((unsigned char *)protocolHeader) + sizeof(Header);
  size_t ack_length    = protocolHeader->length - CoreAPI::PackageMin;

  if (ack_length > 1) {
    api->versionData.version_ack = ptemp[0] + (ptemp[1] << 8);
    ptemp += 2;
    api->versionData.version_crc =
        ptemp[0] + (ptemp[1] << 8) + (ptemp[2] << 16) + (ptemp[3] << 24);
    ptemp += 4;
    if (api->versionData.version != versionM100_23) {
      memcpy(api->versionData.version_ID, ptemp, 11);
      ptemp += 11;
    }
    memcpy(api->versionData.version_name, ptemp, 32);
  } else {
    api->versionData.version_ack = ptemp[0];
    memcpy(api->versionData.version_name, "versionA3_32",
           strlen("versionA3_32") + 1);
  }

  API_LOG(api->serialDevice, STATUS_LOG, "version ack = %d\n",
          api->versionData.version_ack);

  if (api->versionData.version != versionA3_32) {
    API_LOG(api->serialDevice, STATUS_LOG, "version crc = 0x%X\n",
            api->versionData.version_crc);
    if (api->versionData.version != versionM100_23)
      API_LOG(api->serialDevice, STATUS_LOG, "version ID = %.11s\n",
              api->versionData.version_ID);
  }

  API_LOG(api->serialDevice, STATUS_LOG, "version name = %.32s\n",
          api->versionData.version_name);
}

void CoreAPI::activateCallback(CoreAPI *api, Header *protocolHeader,
                               UserData userData __UNUSED) {
  unsigned short ack_data;
  if (protocolHeader->length - CoreAPI::PackageMin <= 2) {
    memcpy((unsigned char *)&ack_data,
           ((unsigned char *)protocolHeader) + sizeof(Header),
           (protocolHeader->length - CoreAPI::PackageMin));
    switch (ack_data) {
      case ACK_ACTIVE_SUCCESS:
        API_LOG(api->serialDevice, STATUS_LOG, "Activated successfully\n");

        if (api->accountData.encKey) api->setKey(api->accountData.encKey);
        return;
      case ACK_ACTIVE_NEW_DEVICE:
        API_LOG(api->serialDevice, STATUS_LOG,
                "New device, please link DJIGO to your "
                "remote controller and try again\n");
        break;
      case ACK_ACTIVE_PARAMETER_ERROR:
        API_LOG(api->serialDevice, ERROR_LOG, "Wrong parameter\n");
        break;
      case ACK_ACTIVE_ENCODE_ERROR:
        API_LOG(api->serialDevice, ERROR_LOG, "Encode error\n");
        break;
      case ACK_ACTIVE_APP_NOT_CONNECTED:
        API_LOG(api->serialDevice, ERROR_LOG, "DJIGO not connected\n");
        break;
      case ACK_ACTIVE_NO_INTERNET:
        API_LOG(api->serialDevice, ERROR_LOG,
                "DJIGO not "
                "connected to the internet\n");
        break;
      case ACK_ACTIVE_SERVER_REFUSED:
        API_LOG(api->serialDevice, ERROR_LOG,
                "DJI server rejected "
                "your request, please use your SDK ID\n");
        break;
      case ACK_ACTIVE_ACCESS_LEVEL_ERROR:
        API_LOG(api->serialDevice, ERROR_LOG, "Wrong SDK permission\n");
        break;
      case ACK_ACTIVE_VERSION_ERROR:
        API_LOG(api->serialDevice, ERROR_LOG, "SDK version did not match\n");
        api->getDroneVersion();
        break;
      default:
        if (!api->decodeACKStatus(ack_data)) {
          API_LOG(api->serialDevice, ERROR_LOG, "While calling this function");
        }
        break;
    }
  } else {
    API_LOG(api->serialDevice, ERROR_LOG,
            "ACK is exception, session id %d,sequence %d\n",
            protocolHeader->sessionID, protocolHeader->sequenceNumber);
  }
}

Version CoreAPI::getSDKVersion() const { return versionData.version; }

void CoreAPI::setBroadcastCallback(CallBackHandler callback) {
  broadcastCallback = callback;
}

SDKFilter CoreAPI::getFilter() const { return filter; }

void CoreAPI::setVersion(const Version &value) { versionData.version = value; }

VersionData CoreAPI::getVersionData() const { return versionData; }

void CoreAPI::setTestCallback(const CallBackHandler &value) {
  testCallback = value;
}
