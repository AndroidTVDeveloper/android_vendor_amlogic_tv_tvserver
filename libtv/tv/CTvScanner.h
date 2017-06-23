//
//
//  amlogic 2013
//
//  @ Project : tv
//  @ File Name :
//  @ Date : 2013-11
//  @ Author :
//
//
#include <am_scan.h>
#include <am_epg.h>
#include <am_mem.h>
#include <am_db.h>
#include "am_cc.h"
#include "CTvChannel.h"
#include "CTvLog.h"
#include "CTvEv.h"
#include "tvin/CTvin.h"

#include <list>


#if !defined(_CTVSCANNER_H)
#define _CTVSCANNER_H
class CTvScanner {
public:
    /** ATSC Attenna type */
    static const int AM_ATSC_ATTENNA_TYPE_AIR = 1;
    static const int AM_ATSC_ATTENNA_TYPE_CABLE_STD = 2;
    static const int AM_ATSC_ATTENNA_TYPE_CABLE_IRC = 3;
    static const int AM_ATSC_ATTENNA_TYPE_CABLE_HRC = 4;
    CTvScanner();
    ~CTvScanner();

    /*deprecated{{*/
    int ATVManualScan(int min_freq, int max_freq, int std, int store_Type = 0, int channel_num = 0);
    int autoAtvScan(int min_freq, int max_freq, int std, int search_type);
    int autoAtvScan(int min_freq, int max_freq, int std, int search_type, int proc_mode);
    int autoDtmbScan();
    int manualDtmbScan(int beginFreq, int endFreq, int modulation = -1);
    int manualDtmbScan(int freq);
    int unsubscribeEvent();
    int dtvAutoScan(int mode);
    int dtvManualScan(int mode, int beginFreq, int endFreq, int para1, int para2);
    int dtvScan(int mode, int scan_mode, int beginFreq, int endFreq, int para1, int para2);
    int dtvScan(char *feparas, int scan_mode, int beginFreq, int endFreq);
    /*}}deprecated*/

    int Scan(char *feparas, char *scanparas);
    int stopScan();
    int pauseScan();
    int resumeScan();
    int getScanStatus(int *status);

    static const char *getDtvScanListName(int mode);
    static CTvScanner *getInstance();

    struct ScannerLcnInfo {

        public:
        #define MAX_LCN 4
        int net_id;
        int ts_id;
        int service_id;
        int visible[MAX_LCN];
        int lcn[MAX_LCN];
        int valid[MAX_LCN];
    };

    typedef std::list<ScannerLcnInfo*> lcn_list_t;


    class ScannerEvent: public CTvEv {
    public:
        static const int EVENT_SCAN_PROGRESS = 0;
        static const int EVENT_STORE_BEGIN   = 1;
        static const int EVENT_STORE_END     = 2;
        static const int EVENT_SCAN_END     = 3;
        static const int EVENT_BLINDSCAN_PROGRESS = 4;
        static const int EVENT_BLINDSCAN_NEWCHANNEL = 5;
        static const int EVENT_BLINDSCAN_END    = 6;
        static const int EVENT_ATV_PROG_DATA = 7;
        static const int EVENT_DTV_PROG_DATA = 8;
        static const int EVENT_SCAN_EXIT     = 9;
        static const int EVENT_SCAN_BEGIN    = 10;
        static const int EVENT_LCN_INFO_DATA = 11;

        ScannerEvent(): CTvEv(CTvEv::TV_EVENT_SCANNER)
        {
            reset();
        }
        void reset()
        {
            clear();
            mFEParas.clear();
        }
        void clear()
        {
            mType = -1;
            mProgramName[0] = '\0';
            mParas[0] = '\0';
            mAcnt = 0;
            mScnt = 0;

            memset(&mLcnInfo, 0, sizeof(ScannerLcnInfo));

            mMajorChannelNumber = -1;
            mMinorChannelNumber = -1;
        }
        ~ScannerEvent()
        {
        }

        //common
        int mType;
        int mPercent;
        int mTotalChannelCount;
        int mLockedStatus;
        int mChannelIndex;

        int mMode;
        long mFrequency;
        int mSymbolRate;
        int mModulation;
        int mBandwidth;
        int mReserved;

        int mSat_polarisation;
        int mStrength;
        int mSnr;

        char mProgramName[1024];
        int mprogramType;
        char mParas[128];

        //for atv
        int mVideoStd;
        int mAudioStd;
        int mIsAutoStd;

        //for DTV
        int mTsId;
        int mONetId;
        int mServiceId;
        int mVid;
        int mVfmt;
        int mAcnt;
        int mAid[32];
        int mAfmt[32];
        char mAlang[32][10];
        int mAtype[32];
        int mAExt[32];
        int mPcr;

        int mScnt;
        int mStype[32];
        int mSid[32];
        int mSstype[32];
        int mSid1[32];
        int mSid2[32];
        char mSlang[32][10];

        int mFree_ca;
        int mScrambled;

        int mScanMode;

        int mSdtVer;

        int mSortMode;

        ScannerLcnInfo  mLcnInfo;

        CFrontEnd::FEParas mFEParas;

        int mMajorChannelNumber;
        int mMinorChannelNumber;
        int mSourceId;
        char mAccessControlled;
        char mHidden;
        char mHideGuide;
    };

    class IObserver {
    public:
        IObserver() {};
        virtual ~IObserver() {};
        virtual void onEvent(const ScannerEvent &ev) = 0;
    };
    // 1 VS n
    //int addObserver(IObserver* ob);
    //int removeObserver(IObserver* ob);

    // 1 VS 1
    int setObserver(IObserver *ob)
    {
        mpObserver = ob;
        return 0;
    }

    class ScanParas : public Paras {

    public:
        ScanParas() : Paras() { }
        ScanParas(const ScanParas &sp) : Paras(sp.mparas) { }
        ScanParas(const char *paras) : Paras(paras) { }
        ScanParas& operator = (const ScanParas &spp);

        int getMode() const { return getInt(SCP_MODE, 0); }
        ScanParas& setMode(int m) { setInt(SCP_MODE, m); return *this; }
        int getAtvMode() const { return getInt(SCP_ATVMODE, 0); }
        ScanParas& setAtvMode(int a) { setInt(SCP_ATVMODE, a); return *this; }
        int getDtvMode() const { return getInt(SCP_DTVMODE, 0); }
        ScanParas& setDtvMode(int d) { setInt(SCP_DTVMODE, d); return *this; }
        ScanParas& setAtvFrequency1(int f) { setInt(SCP_ATVFREQ1, f); return *this; }
        int getAtvFrequency1() const { return getInt(SCP_ATVFREQ1, 0); }
        ScanParas& setAtvFrequency2(int f) { setInt(SCP_ATVFREQ2, f); return *this; }
        int getAtvFrequency2() const { return getInt(SCP_ATVFREQ2, 0); }
        ScanParas& setDtvFrequency1(int f) { setInt(SCP_DTVFREQ1, f); return *this; }
        int getDtvFrequency1() const { return getInt(SCP_DTVFREQ1, 0); }
        ScanParas& setDtvFrequency2(int f) { setInt(SCP_DTVFREQ2, f); return *this; }
        int getDtvFrequency2() const { return getInt(SCP_DTVFREQ2, 0); }
        ScanParas& setProc(int p) { setInt(SCP_PROC, p); return *this; }
        int getProc() const { return getInt(SCP_PROC, 0); }
        ScanParas& setDtvStandard(int s) { setInt(SCP_DTVSTD, s); return *this; }
        int getDtvStandard() const { return getInt(SCP_DTVSTD, -1); }

    public:
        static const char* SCP_MODE;
        static const char* SCP_ATVMODE;
        static const char* SCP_DTVMODE;
        static const char* SCP_ATVFREQ1;
        static const char* SCP_ATVFREQ2;
        static const char* SCP_DTVFREQ1;
        static const char* SCP_DTVFREQ2;
        static const char* SCP_PROC;
        static const char* SCP_DTVSTD;
    };

    int Scan(CFrontEnd::FEParas &fp, ScanParas &sp);

private:
    AM_Bool_t checkAtvCvbsLock(v4l2_std_id  *colorStd);
    static AM_Bool_t checkAtvCvbsLockHelper(void *);

    static void evtCallback(long dev_no, int event_type, void *param, void *data);

    void reconnectDmxToFend(int dmx_no, int fend_no);
    int getAtscChannelPara(int attennaType, Vector<sp<CTvChannel> > &vcp);
    void sendEvent(ScannerEvent &evt);
    //
    AM_SCAN_Handle_t mScanHandle;
    volatile bool mbScanStart;

    //scan para info
    /** General TV Scan Mode */
    static const int TV_MODE_ATV = 0;   // Only search ATV
    static const int TV_MODE_DTV = 1;   // Only search DTV
    static const int TV_MODE_ADTV = 2;  // A/DTV will share a same frequency list, like ATSC
    /** DTV scan mode */
    static const int DTV_MODE_AUTO   = 1;
    static const int DTV_MODE_MANUAL = 2;
    static const int DTV_MODE_ALLBAND = 3;
    static const int DTV_MODE_BLIND  = 4;

    /** DTV scan options, DONOT channge */
    static const int DTV_OPTION_UNICABLE = 0x10;      //Satellite unicable mode
    static const int DTV_OPTION_FTA      = 0x20;      //Only store free programs
    static const int DTV_OPTION_NO_TV    = 0x40;      //Only store tv programs
    static const int DTV_OPTION_NO_RADIO = 0x80;      //Only store radio programs

    /** ATV scan mode */
    static const int ATV_MODE_AUTO   = 1;
    static const int ATV_MODE_MANUAL = 2;

    //
    /*subtitle*/
    static const int TYPE_DVB_SUBTITLE = 1;
    static const int TYPE_DTV_TELETEXT = 2;
    static const int TYPE_ATV_TELETEXT = 3;
    static const int TYPE_DTV_CC = 4;
    static const int TYPE_ATV_CC = 5;

    typedef struct {
        int nid;
        int tsid;
        CFrontEnd::FEParas fe;
        int dtvstd;
    } SCAN_TsInfo_t;

    typedef std::list<SCAN_TsInfo_t*> ts_list_t;

#define AM_SCAN_MAX_SRV_NAME_LANG 4

    typedef struct {
        uint8_t srv_type, eit_sche, eit_pf, rs, free_ca;
        uint8_t access_controlled, hidden, hide_guide;
        uint8_t plp_id;
        int vid, vfmt, srv_id, pmt_pid, pcr_pid, src;
        int chan_num, scrambled_flag;
        int major_chan_num, minor_chan_num, source_id;
        char name[(AM_DB_MAX_SRV_NAME_LEN + 4)*AM_SCAN_MAX_SRV_NAME_LANG + 1];
        AM_SI_AudioInfo_t aud_info;
        AM_SI_SubtitleInfo_t sub_info;
        AM_SI_TeletextInfo_t ttx_info;
        AM_SI_CaptionInfo_t cap_info;
        int sdt_version;
        SCAN_TsInfo_t *tsinfo;
    } SCAN_ServiceInfo_t;

    typedef std::list<SCAN_ServiceInfo_t*> service_list_t;

    dvbpsi_pat_t *getValidPats(AM_SCAN_TS_t *ts);
    int getPmtPid(dvbpsi_pat_t *pats, int pn);
    SCAN_ServiceInfo_t* getServiceInfo();
    void extractCaScrambledFlag(dvbpsi_descriptor_t *p_first_descriptor, int *flag);
    void extractSrvInfoFromSdt(AM_SCAN_Result_t *result, dvbpsi_sdt_t *sdts, SCAN_ServiceInfo_t *service);
    void extractSrvInfoFromVc(AM_SCAN_Result_t *result, dvbpsi_atsc_vct_channel_t *vcinfo, SCAN_ServiceInfo_t *service);
    void updateServiceInfo(AM_SCAN_Result_t *result, SCAN_ServiceInfo_t *service);
    void notifyService(SCAN_ServiceInfo_t *service);
    void getLcnInfo(AM_SCAN_Result_t *result, AM_SCAN_TS_t *ts, lcn_list_t &llist);
    void notifyLcn(ScannerLcnInfo *lcn);
    int insertLcnList(lcn_list_t &llist, ScannerLcnInfo *lcn, int idx);
    int getParamOption(char *para);
    void addFixedATSCCaption(AM_SI_CaptionInfo_t *cap_info, int service, int cc, int text, int is_digital_cc);

    void processDvbTs(AM_SCAN_Result_t *result, AM_SCAN_TS_t *ts, service_list_t &slist);
    void processAnalogTs(AM_SCAN_Result_t *result, AM_SCAN_TS_t *ts, SCAN_TsInfo_t *tsinfo, service_list_t &slist);
    void processAtscTs(AM_SCAN_Result_t *result, AM_SCAN_TS_t *ts, SCAN_TsInfo_t *tsinfo, service_list_t &slist);

    void processTsInfo(AM_SCAN_Result_t *result, AM_SCAN_TS_t *ts, SCAN_TsInfo_t *info);

    void storeNewDvb(AM_SCAN_Result_t *result, AM_SCAN_TS_t *newts);
    void storeNewAnalog(AM_SCAN_Result_t *result, AM_SCAN_TS_t *newts);
    void storeNewAtsc(AM_SCAN_Result_t *result, AM_SCAN_TS_t *newts);

    void storeScan(AM_SCAN_Result_t *result, AM_SCAN_TS_t *curr_ts);

    int getScanDtvStandard(ScanParas &scp);
    int createAtvParas(AM_SCAN_ATVCreatePara_t &atv_para, CFrontEnd::FEParas &fp, ScanParas &sp);
    int freeAtvParas(AM_SCAN_ATVCreatePara_t &atv_para);
    int createDtvParas(AM_SCAN_DTVCreatePara_t &dtv_para, CFrontEnd::FEParas &fp, ScanParas &sp);
    int freeDtvParas(AM_SCAN_DTVCreatePara_t &dtv_para);

    int FETypeHelperCB(int id, void *para, void *user);
    void CC_VBINetworkCb(AM_CC_Handle_t handle, vbi_network *n);

    AM_Bool_t needVbiAssist();
    AM_Bool_t checkVbiDataReady(int freq);
    int startVBI();
    void stopVBI();
    void resetVBI();

    static void storeScanHelper(AM_SCAN_Result_t *result);
    static int FETypeHelperCBHelper(int id, void *para, void *user);
    static void CC_VBINetworkCbHelper(AM_CC_Handle_t handle, vbi_network *n);


private:
    static CTvScanner *mInstance;

    IObserver *mpObserver;
    //
    CTvin *mpTvin;
    int mMode;
    int mFendID;
    /** DTV parameters */
    int mTvMode;
    int mTvOptions;
    int mSat_id;
    int mSource;

    //showboz
    //TVSatelliteParams tv_satparams;
    int mTsSourceID;
    CTvChannel mStartChannel;
    Vector<CTvChannel> mvChooseListChannels;
    /** ATV parameters */
    int mAtvMode;
    int mStartFreq;
    int mDirection;
    int mChannelID;

    //extern for scanner
    //int channelID; //can be used for manual scan
    /** Atv set */
    int mMinFreq;
    int mMaxFreq;
    long long mCurScanStartFreq;
    long long mCurScanEndFreq;
    int tunerStd;
    /** Tv set */
    int demuxID;//default 0
    String8 defaultTextLang;
    String8 orderedTextLangs;
    //showboz
    //Vector<CTvChannel> ChannelList;//VS mvChooseListChannels

    /** Dtv-Sx set Unicable settings*/
    int user_band;
    int ub_freq;//!< kHz

    CFrontEnd::FEParas mFEParas;
    ScanParas mScanParas;

    int mFEType;

    static ScannerEvent mCurEv;

    static service_list_t service_list_dummy;

    AM_CC_Handle_t mVbi;
    int mVbiTsId;
    int mAtvIsAtsc;

    typedef struct {
        int freq;
        int tsid;
    }SCAN_TsIdInfo_t;
    typedef std::list<SCAN_TsIdInfo_t*> tsid_list_t;
    tsid_list_t tsid_list;

};
#endif //CTVSCANNER_H
