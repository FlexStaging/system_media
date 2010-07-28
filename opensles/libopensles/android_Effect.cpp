/*
 * Copyright (C) 2010 The Android Open Source Project
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
 */


#include "sles_allinclusive.h"
#include "math.h"
#include "utils/RefBase.h"


static const int EQUALIZER_PARAM_SIZE_MAX = sizeof(effect_param_t) + 2 * sizeof(int32_t)
        + EFFECT_STRING_LEN_MAX;


//-----------------------------------------------------------------------------
uint32_t eq_paramSize(int32_t param)
{
    uint32_t size;

    switch (param) {
    case EQ_PARAM_NUM_BANDS:
    case EQ_PARAM_LEVEL_RANGE:
    case EQ_PARAM_CUR_PRESET:
    case EQ_PARAM_GET_NUM_OF_PRESETS:
        size = sizeof(int32_t);
        break;
    case EQ_PARAM_BAND_LEVEL:
    case EQ_PARAM_CENTER_FREQ:
    case EQ_PARAM_BAND_FREQ_RANGE:
    case EQ_PARAM_GET_BAND:
    case EQ_PARAM_GET_PRESET_NAME:
        size = 2 * sizeof(int32_t);
        break;
    default:
        size = 2 * sizeof(int32_t);
        SL_LOGE("Trying to use an unknown EQ parameter %d", param);
        break;
    }
    return size;
}

uint32_t eq_valueSize(int32_t param)
{
    uint32_t size;

    switch (param) {
    case EQ_PARAM_NUM_BANDS:
    case EQ_PARAM_CUR_PRESET:
    case EQ_PARAM_GET_NUM_OF_PRESETS:
        size = sizeof(int16_t);
        break;
    case EQ_PARAM_BAND_LEVEL:
    case EQ_PARAM_GET_BAND:
    case EQ_PARAM_CENTER_FREQ:
        size = sizeof(int32_t);
        break;
    case EQ_PARAM_LEVEL_RANGE:
    case EQ_PARAM_BAND_FREQ_RANGE:
        size = 2 * sizeof(int32_t);
        break;
    case EQ_PARAM_GET_PRESET_NAME:
        size = EFFECT_STRING_LEN_MAX;
        break;
    default:
        size = sizeof(int32_t);
        SL_LOGE("Trying to access an unknown EQ parameter %d", param);
        break;
    }
    return size;
}


//-----------------------------------------------------------------------------
android::status_t android_eq_getParam(android::sp<android::AudioEffect> pFx,
        int32_t param, int32_t param2, void *pValue)
{
     android::status_t status;
     uint32_t buf32[(EQUALIZER_PARAM_SIZE_MAX - 1) / sizeof(uint32_t) + 1];
     effect_param_t *p = (effect_param_t *)buf32;

     p->psize = eq_paramSize(param);
     *(int32_t *)p->data = param;
     if (p->psize == 2 * sizeof(int32_t)) {
         *((int32_t *)p->data + 1) = param2;
     }
     p->vsize = eq_valueSize(param);
     status = pFx->getParameter(p);
     if (android::NO_ERROR == status) {
         status = p->status;
         if (android::NO_ERROR == status) {
             memcpy(pValue, p->data + p->psize, p->vsize);
         }
     }

     return status;
 }


//-----------------------------------------------------------------------------
android::status_t android_eq_setParam(android::sp<android::AudioEffect> pFx,
        int32_t param, int32_t param2, void *pValue)
{
    android::status_t status;
    uint32_t buf32[(EQUALIZER_PARAM_SIZE_MAX - 1) / sizeof(uint32_t) + 1];
    effect_param_t *p = (effect_param_t *)buf32;

    p->psize = eq_paramSize(param);
    *(int32_t *)p->data = param;
    if (p->psize == 2 * sizeof(int32_t)) {
        *((int32_t *)p->data + 1) = param2;
    }
    p->vsize = eq_valueSize(param);
    memcpy(p->data + p->psize, pValue, p->vsize);
    status = pFx->setParameter(p);
    if (android::NO_ERROR == status) {
        status = p->status;
    }

    return status;
}


//-----------------------------------------------------------------------------
void android_eq_init(int sessionId, CAudioPlayer* ap) {
    SL_LOGV("android_initEq on session %d", sessionId);

    ap->mEqualizer.mEqEffect = new android::AudioEffect(
            &ap->mEqualizer.mEqDescriptor.type,//(const effect_uuid_t*)SL_IID_EQUALIZER,//
            &ap->mEqualizer.mEqDescriptor.uuid,
            0,// priority
            0,// effect callback
            0,// callback data
            sessionId,// session ID
            0 );// output
    android::status_t status = ap->mEqualizer.mEqEffect->initCheck();
    if (android::NO_ERROR != status) {
        SL_LOGE("EQ initCheck() returned %d", status);
        return;
    }

    // initialize number of bands, band level range
    uint32_t num = 0;
    if (android::NO_ERROR == android_eq_getParam(ap->mEqualizer.mEqEffect,
            EQ_PARAM_NUM_BANDS, 0, &num)) {
        ap->mEqualizer.mNumBands = num;
    }
    int32_t range[2] = {0, 0};
    if (android::NO_ERROR == android_eq_getParam(ap->mEqualizer.mEqEffect,
            EQ_PARAM_LEVEL_RANGE, 0, range)) {
        ap->mEqualizer.mBandLevelRangeMin = range[0];
        ap->mEqualizer.mBandLevelRangeMax = range[1];
    }

    SL_LOGV(" EQ init: num presets = %u, band range=[%d %d]mB", num, range[0], range[1]);

    // initialize preset number and names, store in IEngine
    uint32_t numPresets = 0;
    if (android::NO_ERROR == android_eq_getParam(ap->mEqualizer.mEqEffect,
            EQ_PARAM_GET_NUM_OF_PRESETS, 0, &numPresets)) {
        ap->mObject.mEngine->mEqNumPresets = numPresets;
    }
    char name[EFFECT_STRING_LEN_MAX];
    if ((0 < numPresets) && (NULL == ap->mObject.mEngine->mEqPresetNames)) {
        ap->mObject.mEngine->mEqPresetNames = (char **)new char *[numPresets];
        for(uint32_t i = 0 ; i < numPresets ; i++) {
            if (android::NO_ERROR == android_eq_getParam(ap->mEqualizer.mEqEffect,
                    EQ_PARAM_GET_PRESET_NAME, i, name)) {
                ap->mObject.mEngine->mEqPresetNames[i] = new char[strlen(name) + 1];
                strcpy(ap->mObject.mEngine->mEqPresetNames[i], name);
                SL_LOGV(" EQ init: presets = %u is %s", i, ap->mObject.mEngine->mEqPresetNames[i]);
            }

        }
    }
#if 0
    // configure the EQ so it can easily be heard, for test only
    uint32_t freq = 1977;
    uint32_t frange[2];
    int32_t value = ap->mEqualizer.mBandLevelRangeMin;
    for(int32_t i=0 ; i< ap->mEqualizer.mNumBands ; i++) {
        android_eq_setParam(ap->mEqualizer.mEqEffect, EQ_PARAM_BAND_LEVEL, i, &value);
        // display EQ characteristics
        android_eq_getParam(ap->mEqualizer.mEqEffect, EQ_PARAM_CENTER_FREQ, i, &freq);
        android_eq_getParam(ap->mEqualizer.mEqEffect, EQ_PARAM_BAND_FREQ_RANGE, i, frange);
        SL_LOGV(" EQ init: band %d = %d - %d - %dHz", i, frange[0]/1000, freq/1000,
                frange[1]/1000);
    }
    value = ap->mEqualizer.mBandLevelRangeMax;
    if (ap->mEqualizer.mNumBands > 2) {
        android_eq_setParam(ap->mEqualizer.mEqEffect, EQ_PARAM_BAND_LEVEL, 2, &value);
    }
    if (ap->mEqualizer.mNumBands > 3) {
        android_eq_setParam(ap->mEqualizer.mEqEffect, EQ_PARAM_BAND_LEVEL, 3, &value);
    }

    ap->mEqualizer.mEqEffect->setEnabled(true);
#endif
}

//-----------------------------------------------------------------------------
SLresult android_fx_statusToResult(android::status_t status) {

    if ((android::INVALID_OPERATION == status) || (android::DEAD_OBJECT == status)) {
        return SL_RESULT_CONTROL_LOST;
    } else {
        return SL_RESULT_SUCCESS;
    }
}
