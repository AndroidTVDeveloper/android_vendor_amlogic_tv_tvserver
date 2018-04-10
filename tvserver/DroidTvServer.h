/*
 * Copyright (C) 2016 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *  @author   Tellen Yu
 *  @version  1.0
 *  @date     2018/1/15
 *  @par function description:
 *  - 1 droidlogic tvserver daemon, hwbiner implematation
 */

#ifndef ANDROID_DROIDLOGIC_HDMI_CEC_V1_0_H
#define ANDROID_DROIDLOGIC_HDMI_CEC_V1_0_H

#include <binder/IBinder.h>
#include <utils/Mutex.h>
#include <vector>
#include <map>

#include "DroidTvServiceIntf.h"
#include <vendor/amlogic/hardware/tvserver/1.0/ITvServer.h>

namespace vendor {
namespace amlogic {
namespace hardware {
namespace tvserver {
namespace V1_0 {
namespace implementation {

using ::vendor::amlogic::hardware::tvserver::V1_0::ITvServer;
using ::vendor::amlogic::hardware::tvserver::V1_0::ITvServerCallback;
using ::vendor::amlogic::hardware::tvserver::V1_0::ConnectType;
using ::vendor::amlogic::hardware::tvserver::V1_0::SignalInfo;
using ::vendor::amlogic::hardware::tvserver::V1_0::FormatInfo;
using ::vendor::amlogic::hardware::tvserver::V1_0::TvHidlParcel;
using ::vendor::amlogic::hardware::tvserver::V1_0::Result;
using ::android::hardware::hidl_vec;
using ::android::hardware::hidl_string;
using ::android::hardware::Return;
using ::android::hardware::Void;
using ::android::hidl::memory::V1_0::IMemory;
using ::android::sp;

using namespace android;

class DroidTvServer : public ITvServer, public TvServiceNotify {
public:
    DroidTvServer();
    virtual ~DroidTvServer();

    Return<void> disconnect() override;

    Return<void> lock() override;

    Return<void> unlock() override;

    Return<int32_t> processCmd(int32_t type, int32_t size) override;

    Return<int32_t> startTv() override;
    Return<int32_t> stopTv() override;
    Return<int32_t> switchInputSrc(int32_t inputSrc) override;
    Return<int32_t> getInputSrcConnectStatus(int32_t inputSrc) override;
    Return<int32_t> getCurrentInputSrc() override;
    Return<int32_t> getHdmiAvHotplugStatus() override;
    Return<void> getSupportInputDevices(getSupportInputDevices_cb _hidl_cb) override;
    Return<int32_t> getHdmiPorts() override;

    Return<void> getCurSignalInfo(getCurSignalInfo_cb _hidl_cb) override;
    Return<int32_t> setMiscCfg(const hidl_string& key, const hidl_string& val) override;
    Return<void> getMiscCfg(const hidl_string& key, const hidl_string& def, getMiscCfg_cb _hidl_cb) override;

    Return<int32_t> isDviSIgnal() override;
    Return<int32_t> isVgaTimingInHdmi() override;
    Return<int32_t> setHdmiEdidVersion(int32_t port_id, int32_t ver) override;
    Return<int32_t> getHdmiEdidVersion(int32_t port_id) override;
    Return<int32_t> saveHdmiEdidVersion(int32_t port_id, int32_t ver) override;
    Return<int32_t> handleGPIO(const hidl_string& key, int32_t is_out, int32_t edge) override;
    Return<int32_t> setSourceInput(int32_t inputSrc) override;
    Return<int32_t> setSourceInputExt(int32_t inputSrc, int32_t vInputSrc) override;
    Return<int32_t> getSaveBlackoutEnable() override;
    Return<void> getATVMinMaxFreq(getATVMinMaxFreq_cb _hidl_cb) override;
    Return<int32_t> setAmAudioPreMute(int32_t mute) override;
    Return<int32_t> setDvbTextCoding(const hidl_string& coding) override;
    Return<int32_t> operateDeviceForScan(int32_t type) override;
    Return<int32_t> atvAutoScan(int32_t videoStd, int32_t audioStd, int32_t searchType, int32_t procMode) override;
    Return<int32_t> atvMunualScan(int32_t startFreq, int32_t endFreq, int32_t videoStd, int32_t audioStd) override;
    Return<int32_t> Scan(const hidl_string& feparas, const hidl_string& scanparas) override;
    Return<int32_t> dtvScan(int32_t mode, int32_t scan_mode, int32_t beginFreq, int32_t endFreq, int32_t para1, int32_t para2) override;
    Return<int32_t> pauseScan() override;
    Return<int32_t> resumeScan() override;
    Return<int32_t> dtvStopScan() override;
    Return<int32_t> tvSetFrontEnd(const hidl_string& feparas, int32_t force) override;
    Return<int32_t> sendPlayCmd(int32_t cmd, const hidl_string& id, const hidl_string& param) override;
    Return<int32_t> getCurrentSourceInput() override;
    Return<int32_t> getCurrentVirtualSourceInput() override;
    Return<int32_t> dtvSetAudioChannleMod(int32_t audioChannelIdx) override;
    Return<void> dtvGetVideoFormatInfo(dtvGetVideoFormatInfo_cb _hidl_cb) override;
    Return<void> dtvGetScanFreqListMode(int32_t mode, dtvGetScanFreqListMode_cb _hidl_cb) override;
    Return<int32_t> atvdtvGetScanStatus() override;

    Return<void> setCallback(const sp<ITvServerCallback>& callback, ConnectType type) override;

    virtual void onEvent(const TvHidlParcel &hidlParcel);

private:

    const char* getConnectTypeStr(ConnectType type);

    // Handle the case where the callback registered for the given type dies
    void handleServiceDeath(uint32_t type);

    bool mDebug = false;
    bool mListenerStarted = false;
    DroidTvServiceIntf *mTvServiceIntf;
    std::map<uint32_t, sp<ITvServerCallback>> mClients;

    mutable Mutex mLock;

    class DeathRecipient : public android::hardware::hidl_death_recipient  {
    public:
        DeathRecipient(sp<DroidTvServer> server);

        // hidl_death_recipient interface
        virtual void serviceDied(uint64_t cookie,
            const ::android::wp<::android::hidl::base::V1_0::IBase>& who) override;

    private:
        sp<DroidTvServer> droidTvServer;
    };

    sp<DeathRecipient> mDeathRecipient;
};

}  // namespace implementation
}  // namespace V1_0
}  // namespace tvserver
}  // namespace hardware
}  // namespace amlogic
}  // namespace vendor
#endif /* ANDROID_DROIDLOGIC_HDMI_CEC_V1_0_H */
