/*
 * Copyright (c) 2014 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description: c++ file
 */
#define LOG_TAG                 "tvserver"
#define LOG_TV_TAG              "CTvRrt"

#include <tinyxml.h>
#include "CTvRrt.h"

pthread_mutex_t rrt_update_mutex = PTHREAD_MUTEX_INITIALIZER;

CTvRrt *CTvRrt::mInstance;
CTvRrt *CTvRrt::getInstance()
{
    LOGD("start rrt action!\n");
    if (NULL == mInstance) {
        mInstance = new CTvRrt();
    }

    return mInstance;
}

CTvRrt::CTvRrt()
{
    mRrtScanHandle      = NULL;
    mpNewRrt            = NULL;
    mRrtScanStatus      = INVALID_ID;
    mDmx_id             = INVALID_ID;
    mLastRatingRegion   = INVALID_ID;
    mLastDimensionsDefined = INVALID_ID;
    mLastVersion        = INVALID_ID;
    mScanResult         = 0;
}

CTvRrt::~CTvRrt()
{
    if (mInstance != NULL) {
        delete mInstance;
        mInstance = NULL;
    }
}

/**
 * @Function: StartRrtUpdate
 * @Description: Start Update rrt info
 * @Param:mode:RRT_AUTO_SEARCH:auto search;    RRT_MANU_SEARCH:manual search
 * @Return: 0 success, -1 fail
 */
int CTvRrt::StartRrtUpdate(rrt_search_mode_t mode)
{
    int ret;
    pthread_mutex_lock(&rrt_update_mutex);

    ret = RrtCreate(0, 2, 0, NULL);    //2 is demux id which according to DVB moudle!
    if (ret < 0) {
        LOGD("RrtCreate failed!\n");
        pthread_mutex_unlock(&rrt_update_mutex);
        return 0;
    }

    ret = RrtScanStart();
    if (ret < 0) {
        LOGD("RrtScanStart failed!\n");
        pthread_mutex_unlock(&rrt_update_mutex);
        return 0;
    } else {
        if (mode == RRT_MANU_SEARCH) {//manual
            mRrtScanStatus = RrtEvent::EVENT_RRT_SCAN_SCANING;
            sleep(5);//scan 5s
            mRrtScanStatus = RrtEvent::EVENT_RRT_SCAN_END;
            LOGD("ScanResult = %d!\n", mScanResult);
            pthread_mutex_unlock(&rrt_update_mutex);
            return mScanResult;
        } else {//auto
            pthread_mutex_unlock(&rrt_update_mutex);
            return 1;
        }
    }
}

/**
 * @Function: StopRrtUpdate
 * @Description: Stop Update rrt info
 * @Param:
 * @Return: 0 success, -1 fail
 */
int CTvRrt::StopRrtUpdate(void)
{
    int ret = -1;

    ret = RrtScanStop();
    if (ret < 0) {
        LOGD("RrtScanStop failed!\n");
    }

    ret = RrtDestroy();
    if (ret < 0) {
        LOGE("RrtDestroy failed!\n");
    }

    return ret;
}

/**
 * @Function: GetRRTRating
 * @Description: search data for livetv from RRT file
 * @Param: rating_region_id: rating region id;
           dimension_id:  dimension id;
           value_id:     value id;
           ret:search results
 * @Return: 0 success, -1 fail
 */
int CTvRrt::GetRRTRating(int rating_region_id, int dimension_id, int value_id, rrt_select_info_t *ret)
{
    LOGD("rating_region_id = %d, dimension_id = %d, value_id = %d\n",rating_region_id, dimension_id, value_id);

    //check rrt_define_file exist
    struct stat tmp_st;
    if (stat(TV_RRT_DEFINE_PARAM_PATH, &tmp_st) != 0) {
        LOGD("file don't exist!\n");
        return -1;
    }

    TiXmlDocument *pRRTFile = new TiXmlDocument(TV_RRT_DEFINE_PARAM_PATH);
    if (!pRRTFile->LoadFile()) {
        LOGD("load %s error!\n", TV_RRT_DEFINE_PARAM_PATH);
        return -1;
    }

    memset(ret, 0, sizeof(rrt_select_info_t));
    TiXmlElement* pTmpElement = pRRTFile->RootElement()->FirstChildElement();
    if (pTmpElement != NULL) {
        do {
            if ((pTmpElement->FirstAttribute()->Next()->IntValue() ==rating_region_id) &&
                (pTmpElement->LastAttribute()->IntValue() == dimension_id )) {
                LOGD("%s\n",pTmpElement->FirstAttribute()->Next()->Next()->Value());
                int RationSize = strlen(pTmpElement->FirstAttribute()->Next()->Next()->Value());
                ret->rating_region_name_count = RationSize;
                const char *rating_region_name = pTmpElement->FirstAttribute()->Next()->Next()->Value();
                memcpy(ret->rating_region_name, rating_region_name, RationSize+1);
                LOGD("%s\n",pTmpElement->FirstAttribute()->Value());
                int DimensionSize = strlen(pTmpElement->FirstAttribute()->Value());
                ret->dimensions_name_count = DimensionSize;
                memcpy(ret->dimensions_name, pTmpElement->FirstAttribute()->Value(), DimensionSize+1);

                TiXmlElement* pElement = NULL;
                for (pElement=pTmpElement->FirstChildElement();pElement;pElement = pElement->NextSiblingElement()) {
                    if (pElement->LastAttribute()->IntValue() == value_id ) {
                        int ValueSize = strlen(pElement->FirstAttribute()->Value());
                        ret->rating_value_text_count = ValueSize;
                        LOGD("%s\n",pElement->FirstAttribute()->Value());
                        memcpy(ret->rating_value_text, pElement->FirstAttribute()->Value(), ValueSize+1);
                        return 0;
                    }
                }
            }
        } while(pTmpElement = pTmpElement->NextSiblingElement());
                LOGD("Don't find value !\n");
                return -1;
    } else {
        LOGD("XML file is NULL!\n");
        return -1;
    }
}

/**
 * @Function: RrtCreate
 * @Description: open dev for RRT and set RRT event
 * @Param: fend_id: fend dev id;
           dmx_id:  demux dev id;
           src:     source;
           textLangs:language;
 * @Return: 0 success, -1 fail
 */
int CTvRrt::RrtCreate(int fend_id, int dmx_id, int src, char * textLangs)
{
    AM_EPG_CreatePara_t para;
    AM_ErrorCode_t  ret;
    AM_DMX_OpenPara_t dmx_para;
    mDmx_id = dmx_id;

    memset(&dmx_para, 0, sizeof(dmx_para));
    LOGD("Opening demux%d ...", dmx_id);
    ret = AM_DMX_Open(mDmx_id, &dmx_para);
    if (ret != AM_SUCCESS) {
        LOGD("AM_DMX_Open failed");
        return - 1;
    }

    para.fend_dev       = fend_id;
    para.dmx_dev        = dmx_id;
    para.source         = src;
    para.hdb            = NULL;
    snprintf(para.text_langs, sizeof(para.text_langs), "%s", textLangs);

    ret = AM_EPG_Create(&para, &mRrtScanHandle);

    if (ret != AM_SUCCESS) {
        LOGD("AM_EPG_Create failed");
        return - 1;
    }

    /*register eit events notifications*/
    AM_EVT_Subscribe((long)mRrtScanHandle, AM_EPG_EVT_NEW_RRT, RrtEventCallback, NULL);
    AM_EPG_SetUserData(mRrtScanHandle, (void *)this);

    return 0;
}

/**
 * @Function: RrtDestroy
 * @Description: close dev for RRT and reset RRT event
 * @Param:
 * @Return: 0 success, -1 fail
 */
int CTvRrt::RrtDestroy()
{
    AM_ErrorCode_t  ret;

    if (mRrtScanHandle != NULL) {
        AM_EVT_Unsubscribe((long)mRrtScanHandle, AM_EPG_EVT_NEW_RRT, RrtEventCallback, NULL);
        ret = AM_EPG_Destroy(mRrtScanHandle);
        if (ret != AM_SUCCESS) {
            LOGD("AM_EPG_Destroy failed");
            return - 1;
        }
        mRrtScanHandle = NULL;
    }

    if (mDmx_id != INVALID_ID) {
        ret = AM_DMX_Close(mDmx_id);

        if (ret != AM_SUCCESS) {
            LOGD("AM_DMX_Close failed");
            return - 1;
        }
        mDmx_id = INVALID_ID;
    }

    return 0;
}

/**
 * @Function: RrtChangeMode
 * @Description: change epg mode
 * @Param: op: epg modul;
           mode:  epg mode;
 * @Return: 0 success, -1 fail
 */
int CTvRrt::RrtChangeMode(int op, int mode)
{
    AM_ErrorCode_t  ret;

    ret = AM_EPG_ChangeMode(mRrtScanHandle, op, mode);
    if (ret != AM_SUCCESS) {
        LOGD("AM_EPG_ChangeMode failed");
        return - 1;
    }

    return 0;
}

/**
 * @Function: RrtScanStart
 * @Description: start scan RRT info
 * @Param:
 * @Return: 0 success, -1 fail
 */
int CTvRrt::RrtScanStart(void)
{
    int ret = -1;
    ret = RrtChangeMode(MODE_ADD, SCAN_RRT);

    return ret;
}

/**
 * @Function: RrtScanStop
 * @Description: stop scan RRT info
 * @Param:
 * @Return: 0 success, -1 fail
 */
int CTvRrt::RrtScanStop(void)
{
    int ret = -1;
    ret = RrtChangeMode(MODE_REMOVE, SCAN_RRT);
    return ret;
}

/**
 * @Function: RrtEventCallback
 * @Description: RRT event callback function
 * @Param:dev_no: dev id
          event_type:RRT event type
          param:callback data
          user_data:
 * @Return:
 */
void CTvRrt::RrtEventCallback(long dev_no, int event_type, void *param, void *user_data)
{
    AM_EPG_GetUserData((AM_EPG_Handle_t)dev_no, (void **)&mInstance);

    if (mInstance == NULL) {
        LOGD("rrt mInstance is NULL!\n");
        return;
    }

    if (mInstance->mpObserver == NULL) {
        LOGD("rrt mpObserver is NULL!\n");
        return;
    }

    if (!param) {
        LOGD("rrt data is NULL!\n");
        if (mInstance->mRrtScanStatus == RrtEvent::EVENT_RRT_SCAN_SCANING) {
            mInstance->mScanResult = 0;
        }

        return;
    }

    switch (event_type) {
    case AM_EPG_EVT_NEW_RRT: {
        rrt_section_info_t *tmp = (rrt_section_info_t *)param;
        LOGD("RatingRegion:[0x%04x] DimensionsDefined:[0x%04x] Version:[0x%x]\n",
              tmp->rating_region, tmp->dimensions_defined, tmp->version_number);
        if (mInstance->RrtUpdataCheck(tmp->rating_region, tmp->dimensions_defined, tmp->version_number)) {
            LOGD("Same RRT data,no need update!\n");
        } else {
            if (mInstance->mRrtScanStatus == RrtEvent::EVENT_RRT_SCAN_SCANING) {
                mInstance->mScanResult = 1;
            }

            mInstance->mCurRrtEv.satus = CTvRrt::RrtEvent::EVENT_RRT_SCAN_START;
            mInstance->mpObserver->onEvent(mInstance->mCurRrtEv);

            mInstance->RrtDataUpdate(dev_no, event_type, param, user_data);

            mInstance->mCurRrtEv.satus = CTvRrt::RrtEvent::EVENT_RRT_SCAN_END;
            mInstance->mpObserver->onEvent(mInstance->mCurRrtEv);
        }
        break;
    }
    default:
        break;
    }
}

/**
 * @Function: GetElementPointerByName
 * @Description: search data from RRT file for save RRT data
 * @Param: pRootElement: Root element of RRT xml file;
           ElementName:  name of TiXmlElement need been search
 * @Return: the TiXmlElement which has been search
 */
TiXmlElement *GetElementPointerByName(TiXmlElement* pRootElement, char *ElementName)
{
    if (strcmp(ElementName, pRootElement->Value()) == 0) {
        return pRootElement;
    }

    TiXmlElement* pElement = NULL;
    TiXmlElement* pRetElement = NULL;
    for (pElement=pRootElement->FirstChildElement();pElement;pElement = pElement->NextSiblingElement()) {
         pRetElement = GetElementPointerByName(pElement, ElementName);
    }

    if (pRetElement != NULL) {
        LOGD("GetNodePointerByName: %s", pRetElement->Value());
        return pRetElement;
    } else {
        return NULL;
    }
}

/**
 * @Function: OpenXmlFile
 * @Description: Open XML file
 * @Param:
 * @Return: The pRRTFile which has been opened
 */
TiXmlDocument *OpenXmlFile(void)
{
    // define TiXmlDocument
    TiXmlDocument *pRRTFile = new TiXmlDocument();
    pRRTFile->LoadFile();
    if (NULL == pRRTFile) {
        LOGD("create RRTFile error!\n");
        return NULL;
    }

    //add Declaration
    LOGD("start create Declaration!\n");
    TiXmlDeclaration *pNewDeclaration = new TiXmlDeclaration("1.0","utf-8","");
    if (NULL == pNewDeclaration) {
        LOGD("create Declaration error!\n");
        return NULL;
    }
    pRRTFile->LinkEndChild(pNewDeclaration);

    //add root element
    LOGD("start create RootElement!\n");
    TiXmlElement *pRootElement = new TiXmlElement("rating-system-definitions");
    if (NULL == pRootElement) {
        LOGD("create RootElement error!\n");
        return NULL;
    }
    pRRTFile->LinkEndChild(pRootElement);
    pRootElement->SetAttribute("xmlns:android", "http://schemas.android.com/apk/res/android");
    pRootElement->SetAttribute("android:versionCode", "2");

    return pRRTFile;
}

/**
 * @Function: SaveDataToXml
 * @Description: Save data to XML file
 * @Param:pRRTFile:The pRRTFile which has been opened
          rrt_info:Charge for GetRRTRating
 * @Return: true:save success;    false:save failed
 */
bool SaveDataToXml(TiXmlDocument *pRRTFile, rrt_info_t rrt_info)
{
    if (pRRTFile == NULL) {
        LOGE("xml file don't open!\n");
        return false;
    }

    TiXmlElement *pRootElement = pRRTFile->RootElement();
    if (pRootElement->FirstChildElement() == NULL) {
        TiXmlElement *pRatingSystemElement = new TiXmlElement("rating-system-definition");
        if (NULL == pRatingSystemElement) {
            LOGD("create pRatingSystemElement error!\n");
            return false;
        }
        pRootElement->LinkEndChild(pRatingSystemElement);
        pRatingSystemElement->SetAttribute("android:name", rrt_info.dimensions_name);
        pRatingSystemElement->SetAttribute("android:rating", rrt_info.rating_region);
        pRatingSystemElement->SetAttribute("android:country",rrt_info.rating_region_name);
        pRatingSystemElement->SetAttribute("android:dimension_id",rrt_info.dimensions_id);

        TiXmlElement *pNewElement = new TiXmlElement("rating-definition");
        if (NULL == pNewElement) {
            return false;
        }
        pRatingSystemElement->LinkEndChild(pNewElement);
        pNewElement->SetAttribute("android:title",rrt_info.abbrev_rating_value_text);
        pNewElement->SetAttribute("android:description",rrt_info.rating_value_text);
        pNewElement->SetAttribute("android:rating_id",rrt_info.rating_value_id);

    } else {
        TiXmlElement *pTmpElement = GetElementPointerByName(pRootElement, "rating-system-definition");
        if ((strcmp(pTmpElement->FirstAttribute()->Value(), rrt_info.dimensions_name) == 0) &&
            (strcmp(pTmpElement->FirstAttribute()->Next()->Next()->Value(), rrt_info.rating_region_name) == 0) &&
            (pTmpElement->LastAttribute()->IntValue() == rrt_info.dimensions_id)) {
            LOGD("add new rating-definition to rating-system-definition!\n");
            TiXmlElement *pNewElement = new TiXmlElement("rating-definition");
            if (NULL == pNewElement) {
                return false;
            }
            pTmpElement->LinkEndChild(pNewElement);
            pNewElement->SetAttribute("android:title",rrt_info.abbrev_rating_value_text);
            pNewElement->SetAttribute("android:description",rrt_info.rating_value_text);
            pNewElement->SetAttribute("android:rating_id",rrt_info.rating_value_id);
        } else {
            LOGD("create new rating-system-definition!\n");
            TiXmlElement *pRatingSystemElement = new TiXmlElement("rating-system-definition");
            if (NULL == pRatingSystemElement) {
                LOGD("create pRatingSystemElement error!\n");
                return false;
            }
            pRootElement->LinkEndChild(pRatingSystemElement);
            pRatingSystemElement->SetAttribute("android:name", rrt_info.dimensions_name);
            pRatingSystemElement->SetAttribute("android:rating", rrt_info.rating_region);
            pRatingSystemElement->SetAttribute("android:country",rrt_info.rating_region_name);
            pRatingSystemElement->SetAttribute("android:dimension_id",rrt_info.dimensions_id);

            TiXmlElement *pNewElement = new TiXmlElement("rating-definition");
            if (NULL == pNewElement) {
                return false;
            }
            pRatingSystemElement->LinkEndChild(pNewElement);
            pNewElement->SetAttribute("android:title",rrt_info.abbrev_rating_value_text);
            pNewElement->SetAttribute("android:description",rrt_info.rating_value_text);
            pNewElement->SetAttribute("android:rating_id",rrt_info.rating_value_id);
        }
    }

    return true;
}

/**
 * @Function: RrtDataUpdate
 * @Description: RRT data parser
 * @Param:dev_no: dev id
          event_type:RRT event type
          param:callback data
          user_data:
 * @Return:
 */
void CTvRrt::RrtDataUpdate(long dev_no, int event_type, void *param, void *user_data)
{
    switch (event_type) {
    case AM_EPG_EVT_NEW_RRT: {
        INT8U i, j;
        rrt_info_t rrt_info;
        memset(&rrt_info, 0, sizeof(rrt_info_t));

        mpNewRrt = (rrt_section_info_t *)param;

        //open xml file
        TiXmlDocument *pRRTFile = OpenXmlFile();
        if (pRRTFile == NULL) {
            LOGD("open xml file failed!\n");
            return;
        }

        while (mpNewRrt != NULL) {
            LOGD("T [RRT:0x%02x][rr:0x%04x][dd:0x%04x] v[0x%x]\n", mpNewRrt->i_table_id, mpNewRrt->rating_region,
                    mpNewRrt->dimensions_defined, mpNewRrt->version_number);

            //save rating_region
            rrt_info.rating_region = mpNewRrt->rating_region;

            //parser rating_region_name
            MultipleStringParser(mpNewRrt->rating_region_name, rrt_info.rating_region_name);

            //parser dimensions_info
            rrt_dimensions_info  *dimensions_info = mpNewRrt->dimensions_info;

            while (dimensions_info != NULL) {
                //parser dimensions_name
                MultipleStringParser(dimensions_info->dimensions_name, rrt_info.dimensions_name);
                LOGD("graduated_scale[%d] values_defined[%d]\n", mpNewRrt->dimensions_info->graduated_scale,mpNewRrt->dimensions_info->values_defined);

                //paser and save data to xml
                for (j=1;j<dimensions_info->values_defined;j++) {
                    //save rating_id
                    rrt_info.rating_value_id = j;
                    MultipleStringParser(dimensions_info->rating_value[j].abbrev_rating_value_text, rrt_info.abbrev_rating_value_text);
                    MultipleStringParser(dimensions_info->rating_value[j].rating_value_text, rrt_info.rating_value_text);

                    bool ret = SaveDataToXml(pRRTFile, rrt_info);
                    if (!ret) {
                        LOGD("Save XML element error!\n");
                    }
                }
                //save dimensions_id
                rrt_info.dimensions_id ++ ;

                dimensions_info = dimensions_info->p_next;
            }
            mpNewRrt = mpNewRrt->p_next;
        }

        if (!pRRTFile->SaveFile(TV_RRT_DEFINE_PARAM_PATH)) {
            LOGD("save RRT XML File error!\n");
        } else {
            LOGD("save RRT XML File success!\n");
        }
        break;
    }
    default:
        break;
    }
}

/**
 * @Function: MultipleStringParser
 * @Description: Multiple string data parser
 * @Param:atsc_multiple_string: Multiple string data
          ret: data after parser
 * @Return:
 */
void CTvRrt::MultipleStringParser(atsc_multiple_string_t atsc_multiple_string, char *ret)
{
    int i;
    for (i=0;i<atsc_multiple_string.i_string_count;i++) {
        int size = strlen((char *)atsc_multiple_string.string[0].string);
        if (ret != NULL) {
            memcpy(ret, atsc_multiple_string.string[0].string, size+1);
        }
    }

    return;
}

/**
 * @Function: RrtUpdataCheck
 * @Description: Check RRT xml file need update or not
 * @Param:atsc_multiple_string: Multiple string data
          ret: data after parser
 * @Return:
 */

bool CTvRrt::RrtUpdataCheck(int rating_region, int dimensions_defined, int version_number)
{
    if ((mLastRatingRegion == rating_region)
        && (mLastDimensionsDefined == dimensions_defined)
        && (mLastVersion == version_number)){
        return true;
    } else {
        mLastRatingRegion = rating_region;
        mLastDimensionsDefined = dimensions_defined;
        mLastVersion =version_number;
        return false;
    }
}
