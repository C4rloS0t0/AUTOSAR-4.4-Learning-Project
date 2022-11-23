/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021 Parai Wang <parai@foxmail.com>
 *
 * ref: Specification of Diagnostic CommunicationManager AUTOSAR CP Release 4.4.0
 */
/* ================================ [ WEAD DEF  ] ============================================== */
#if defined(_WIN32) || defined(linux)
#define STD_DEF_API(ret, api, args)                                                                \
  ret __attribute__((weak)) api args {                                                             \
    return E_NOT_OK;                                                                               \
  }
#endif
/* ================================ [ INCLUDES  ] ============================================== */
#include "Dcm.h"
#include "Dcm_Cfg.h"
#include "Dcm_Internal.h"
#include "Std_Debug.h"
#include "Dem.h"
#include "NvM.h"
/* ================================ [ MACROS    ] ============================================== */
#define AS_LOG_DCM 1
/* ================================ [ TYPES     ] ============================================== */
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
#ifdef DCM_USE_SERVICE_ROUTINE_CONTROL
Std_ReturnType Dcm_DspRoutineControlStart(Dcm_MsgContextType *msgContext, Dcm_OpStatusType OpStatus,
                                          const Dcm_RoutineControlType *rtCtrl,
                                          Dcm_NegativeResponseCodeType *nrc) {
  Std_ReturnType r = E_NOT_OK;
  uint16_t currentDataLength = msgContext->reqDataLen - 3;

  r = rtCtrl->StartRoutineFnc(&msgContext->reqData[3], OpStatus, &msgContext->resData[3],
                              &currentDataLength, nrc);

  if (E_OK == r) {
    msgContext->resData[0] = 0x01;
    msgContext->resData[1] = (rtCtrl->id >> 8) & 0xFF;
    msgContext->resData[2] = rtCtrl->id & 0xFF;
    msgContext->resDataLen = 3 + currentDataLength;
  }

  return r;
}
#endif
/* ================================ [ FUNCTIONS ] ============================================== */
Std_ReturnType Dcm_DspSessionControl(Dcm_MsgContextType *msgContext,
                                     Dcm_NegativeResponseCodeType *nrc) {

  Std_ReturnType r = E_NOT_OK;
  Dcm_ContextType *context = Dcm_GetContext();
  const Dcm_ConfigType *config = Dcm_GetConfig();
  const Dcm_SessionControlConfigType *sesCtrlConfig =
    (const Dcm_SessionControlConfigType *)context->curService->config;
  Dcm_SesCtrlType sesCtrl = msgContext->reqData[0];
  int i;
  uint16_t u16V;

  if (1 == msgContext->reqDataLen) {
    for (i = 0; i < sesCtrlConfig->numOfSesCtrls; i++) {
      if (sesCtrl == sesCtrlConfig->sesCtrls[i]) {
        break;
      }
    }
    if (i >= sesCtrlConfig->numOfSesCtrls) {
      /* @SWS_Dcm_00307 */
      *nrc = DCM_E_SUB_FUNCTION_NOT_SUPPORTED;
    } else {
      r = E_OK;
    }
  } else {
    *nrc = DCM_E_INCORRECT_MESSAGE_LENGTH_OR_INVALID_FORMAT;
  }

  if (E_OK == r) {
    r = sesCtrlConfig->GetSesChgPermissionFnc(context->currentSession, sesCtrl, nrc);
  }

  if (E_OK == r) {
    Dcm_DslInit();
    Dcm_SessionChangeIndication(context->currentSession, sesCtrl);
    context->currentSession = sesCtrl;
    u16V = config->timing->S3Server * DCM_MAIN_FUNCTION_PERIOD;
    msgContext->resData[0] = sesCtrl;
    msgContext->resData[1] = (u16V >> 8) & 0xFF;
    msgContext->resData[2] = u16V & 0xFF;
    u16V = config->timing->P2ServerMax * DCM_MAIN_FUNCTION_PERIOD / 10;
    msgContext->resData[3] = (u16V >> 8) & 0xFF;
    msgContext->resData[4] = u16V & 0xFF;
    msgContext->resDataLen = 5;
    context->timerS3Server = config->timing->S3Server;
    context->timerP2Server = config->timing->P2ServerMin;
  }

  return r;
}
#ifdef DCM_USE_SERVICE_SECURITY_ACCESS
Std_ReturnType Dcm_DspSecurityAccess(Dcm_MsgContextType *msgContext,
                                     Dcm_NegativeResponseCodeType *nrc) {
  Std_ReturnType r = E_NOT_OK;
  Dcm_ContextType *context = Dcm_GetContext();
  const Dcm_SecurityAccessConfigType *secAccConfig =
    (const Dcm_SecurityAccessConfigType *)context->curService->config;
  const Dcm_SecLevelConfigType *secLevelConfig = NULL;
  Dcm_SecLevelType secLevel = (msgContext->reqData[0] + 1) / 2;
  int i;

  if (msgContext->reqDataLen >= 1) {
    for (i = 0; i < secAccConfig->numOfSesLevels; i++) {
      if (secAccConfig->secLevelConfig[i].secLevel == secLevel) {
        secLevelConfig = &secAccConfig->secLevelConfig[i];
      }
    }
    if (NULL != secLevelConfig) {
      r = Dcm_DslIsSessionSupported(context->currentSession, secLevelConfig->sessionMask);
      if (E_OK != r) {
        *nrc = DCM_E_SUB_FUNCTION_NOT_SUPPORTED_IN_ACTIVE_SESSION;
      }
    } else {
      *nrc = DCM_E_SUB_FUNCTION_NOT_SUPPORTED;
    }
  }

  if (E_OK == r) {
    if (msgContext->reqData[0] & 0x01) { /* request seed */
      if (1 == msgContext->reqDataLen) {
        r = Dcm_DslSecurityAccessRequestSeed(msgContext, secLevelConfig, nrc);
      } else {
        *nrc = DCM_E_INCORRECT_MESSAGE_LENGTH_OR_INVALID_FORMAT;
      }
    } else {
      if ((1 + secLevelConfig->keySize) == msgContext->reqDataLen) {
        r = Dcm_DslSecurityAccessCompareKey(msgContext, secLevelConfig, nrc);
      } else {
        *nrc = DCM_E_INCORRECT_MESSAGE_LENGTH_OR_INVALID_FORMAT;
      }
    }
  }
  return r;
}
#endif

#ifdef DCM_USE_SERVICE_ROUTINE_CONTROL
Std_ReturnType Dcm_DspRoutineControl(Dcm_MsgContextType *msgContext,
                                     Dcm_NegativeResponseCodeType *nrc) {
  Std_ReturnType r = E_NOT_OK;
  Dcm_ContextType *context = Dcm_GetContext();
  const Dcm_RoutineControlConfigType *rtCtrlConfig =
    (const Dcm_RoutineControlConfigType *)context->curService->config;
  const Dcm_RoutineControlType *rtCtrl = NULL;
  uint16_t id;
  int i;

  if (msgContext->reqDataLen >= 3) {
    id = ((uint16_t)msgContext->reqData[1] << 8) + msgContext->reqData[2];
    for (i = 0; i < rtCtrlConfig->numOfRtCtrls; i++) {
      if (rtCtrlConfig->rtCtrls[i].id == id) {
        rtCtrl = &rtCtrlConfig->rtCtrls[i];
        break;
      }
    }
    if (NULL != rtCtrl) {
      r = Dcm_DslServiceDIDSesSecPhyFuncCheck(context, &rtCtrl->SesSecAccess, nrc);
    } else {
      *nrc = DCM_E_REQUEST_OUT_OF_RANGE;
    }
  } else {
    *nrc = DCM_E_INCORRECT_MESSAGE_LENGTH_OR_INVALID_FORMAT;
  }

  if (E_OK == r) {
    switch (msgContext->reqData[0]) {
    case 0x01:
      r = Dcm_DspRoutineControlStart(msgContext, context->opStatus, rtCtrl, nrc);
      break;
    default:
      *nrc = DCM_E_SUB_FUNCTION_NOT_SUPPORTED;
      break;
    }
  }

  return r;
}
#endif
#ifdef DCM_USE_SERVICE_REQUEST_DOWNLOAD
Std_ReturnType Dcm_DspRequestDownload(Dcm_MsgContextType *msgContext,
                                      Dcm_NegativeResponseCodeType *nrc) {
  Std_ReturnType r = E_NOT_OK;
  Dcm_ContextType *context = Dcm_GetContext();
  const Dcm_ConfigType *config = Dcm_GetConfig();
  const Dcm_RequestDownloadConfigType *rdConfig =
    (const Dcm_RequestDownloadConfigType *)context->curService->config;
  uint8_t dataFormatIdentifier;
  uint8_t memorySizeLen;
  uint8_t memoryAddressLen;
  uint32_t memoryAddress;
  uint32_t memorySize;
  /* @SWS_Dcm_01420 */
  uint32_t BlockLength = config->rxBufferSize;
  int i;

  if (msgContext->reqDataLen > 3) {
    dataFormatIdentifier = msgContext->reqData[0];
    memorySizeLen = (msgContext->reqData[1] >> 4) & 0xF;
    memoryAddressLen = msgContext->reqData[1] & 0xF;
    if ((memorySizeLen >= 1) && (memorySizeLen <= 4) && (memoryAddressLen >= 1) &&
        (memoryAddressLen <= 4)) {
      if ((2 + memoryAddressLen + memorySizeLen) == msgContext->reqDataLen) {
        /* @SWS_Dcm_00856: support all possible case */
        r = E_OK;
        memoryAddress = 0;
        for (i = 0; i < memoryAddressLen; i++) {
          memoryAddress = (memoryAddress << 8) + msgContext->reqData[2 + i];
        }
        memorySize = 0;
        for (i = 0; i < memorySizeLen; i++) {
          memorySize = (memorySize << 8) + msgContext->reqData[2 + memoryAddressLen + i];
        }
      } else {
        *nrc = DCM_E_INCORRECT_MESSAGE_LENGTH_OR_INVALID_FORMAT;
      }
    } else {
      *nrc = DCM_E_REQUEST_OUT_OF_RANGE;
    }
  } else {
    *nrc = DCM_E_INCORRECT_MESSAGE_LENGTH_OR_INVALID_FORMAT;
  }

  if (E_OK == r) {
    if (DCM_UDT_IDLE_STATE != context->UDTData.state) {
      *nrc = DCM_E_REQUEST_SEQUENCE_ERROR;
    }
  }

  if (E_OK == r) {
    ASLOG(DCM, ("download memoryAddress=0x%X memorySize=0x%X\n", memoryAddress, memorySize));
    /* @SWS_Dcm_91070: MemoryIdentifier is not used, set it to 0x00 */
    r = rdConfig->ProcessRequestDownloadFnc(context->opStatus, dataFormatIdentifier, 0x00,
                                            memoryAddress, memorySize, &BlockLength, nrc);
  }

  if (E_OK == r) {
    msgContext->resData[0] = 0x20;
    msgContext->resData[1] = (BlockLength >> 8) & 0xFF;
    msgContext->resData[2] = BlockLength & 0xFF;
    msgContext->resDataLen = 3;
    context->UDTData.state = DCM_UDT_DOWNLOAD_STATE;
    context->UDTData.memoryAddress = memoryAddress;
    context->UDTData.memorySize = memorySize;
    context->UDTData.offset = 0;
    context->UDTData.blockSequenceCounter = 1;
  }

  return r;
}
#endif

#ifdef DCM_USE_SERVICE_TRANSFER_DATA
Std_ReturnType Dcm_DspTransferData(Dcm_MsgContextType *msgContext,
                                   Dcm_NegativeResponseCodeType *nrc) {
  Std_ReturnType r = E_NOT_OK;
  Dcm_ContextType *context = Dcm_GetContext();
  const Dcm_TransferDataConfigType *tfdConfig =
    (const Dcm_TransferDataConfigType *)context->curService->config;
  uint32_t memoryAddress = context->UDTData.memoryAddress + context->UDTData.offset;
  uint32_t memorySize = context->UDTData.memorySize - context->UDTData.offset;
  Dcm_ReturnWriteMemoryType retW;
  Dcm_ReturnReadMemoryType retR;

  if (msgContext->reqDataLen >= 1) {
    if (context->UDTData.state != DCM_UDT_IDLE_STATE) {
      if (context->UDTData.blockSequenceCounter == msgContext->reqData[0]) {
        if (DCM_UDT_DOWNLOAD_STATE == context->UDTData.state) {
          if (tfdConfig->WriteFnc != NULL) {
            r = E_OK;
          } else {
            *nrc = DCM_E_CONDITIONS_NOT_CORRECT;
          }
        } else {
          if (tfdConfig->ReadFnc != NULL) {
            r = E_OK;
          } else {
            *nrc = DCM_E_CONDITIONS_NOT_CORRECT;
          }
        }
      } else {
        *nrc = DCM_E_WRONG_BLOCK_SEQUENCE_COUNTER;
      }
    } else {
      *nrc = DCM_E_REQUEST_SEQUENCE_ERROR;
    }
  } else {
    *nrc = DCM_E_INCORRECT_MESSAGE_LENGTH_OR_INVALID_FORMAT;
  }

  if (E_OK == r) {
    if (DCM_UDT_DOWNLOAD_STATE == context->UDTData.state) {
      if (memorySize < (msgContext->reqDataLen - 1)) {
        *nrc = DCM_E_INCORRECT_MESSAGE_LENGTH_OR_INVALID_FORMAT;
        r = E_NOT_OK;
      } else {
        memorySize = msgContext->reqDataLen - 1;
      }
    } else {
      if (1 == msgContext->reqDataLen) {
        if (memorySize > (msgContext->resMaxDataLen - 1)) {
          memorySize = msgContext->resMaxDataLen - 1;
        }
      } else {
        *nrc = DCM_E_INCORRECT_MESSAGE_LENGTH_OR_INVALID_FORMAT;
        r = E_NOT_OK;
      }
    }
  }

  if (E_OK == r) {
    if (DCM_UDT_DOWNLOAD_STATE == context->UDTData.state) {
      retW = tfdConfig->WriteFnc(context->opStatus, 0x00, memoryAddress, memorySize,
                                 &msgContext->reqData[1], nrc);
      if (DCM_WRITE_PENDING == retW) {
        *nrc = DCM_E_RESPONSE_PENDING;
      } else if (DCM_WRITE_FORCE_RCRRP == retW) {
        r = DCM_FORCE_RCRRP_OK;
      } else if (DCM_WRITE_OK == retW) {
        context->UDTData.offset += memorySize;
        msgContext->resData[0] = context->UDTData.blockSequenceCounter;
        msgContext->resDataLen = 1;
        context->UDTData.blockSequenceCounter++;
      } else { /* FAILED */
        r = E_NOT_OK;
        if (DCM_POS_RESP == *nrc) {
          *nrc = DCM_E_GENERAL_PROGRAMMING_FAILURE;
        }
      }
    } else {
      retR = tfdConfig->ReadFnc(context->opStatus, 0x00, memoryAddress, memorySize,
                                &msgContext->resData[1], nrc);
      if (DCM_READ_PENDING == retR) {
        *nrc = DCM_E_RESPONSE_PENDING;
      } else if (DCM_READ_FORCE_RCRRP == retR) {
        r = DCM_FORCE_RCRRP_OK;
      } else if (DCM_READ_OK == retR) {
        context->UDTData.offset += memorySize;
        msgContext->resData[0] = context->UDTData.blockSequenceCounter;
        msgContext->resDataLen = 1 + memorySize;
        context->UDTData.blockSequenceCounter++;
      } else { /* FAILED */
        r = E_NOT_OK;
        if (DCM_POS_RESP == *nrc) {
          *nrc = DCM_E_CONDITIONS_NOT_CORRECT;
        }
      }
    }
  }
  return r;
}
#endif

#ifdef DCM_USE_SERVICE_REQUEST_TRANSFER_EXIT
Std_ReturnType Dcm_DspRequestTransferExit(Dcm_MsgContextType *msgContext,
                                          Dcm_NegativeResponseCodeType *nrc) {
  Std_ReturnType r = E_NOT_OK;
  Dcm_ContextType *context = Dcm_GetContext();
  const Dcm_TransferExitConfigType *tfeConfig =
    (const Dcm_TransferExitConfigType *)context->curService->config;

  if (0 == msgContext->reqDataLen) {
    r = E_OK;
  } else {
    *nrc = DCM_E_INCORRECT_MESSAGE_LENGTH_OR_INVALID_FORMAT;
  }

  if (E_OK == r) {
    if (DCM_UDT_IDLE_STATE == context->UDTData.state) {
      r = E_NOT_OK;
      *nrc = DCM_E_REQUEST_SEQUENCE_ERROR;
    }
  }

  if (E_OK == r) {
    r = tfeConfig->TransferExitFnc(context->opStatus, nrc);
  }

  if (E_OK == r) {
    context->UDTData.state = DCM_UDT_IDLE_STATE;
    context->UDTData.blockSequenceCounter = 0;
    context->UDTData.memoryAddress = 0;
    context->UDTData.memorySize = 0;
    context->UDTData.offset = 0;
  }

  return r;
}
#endif
#ifdef DCM_USE_SERVICE_ECU_RESET
Std_ReturnType Dcm_DspEcuReset(Dcm_MsgContextType *msgContext, Dcm_NegativeResponseCodeType *nrc) {
  Std_ReturnType r = E_NOT_OK;
  Dcm_ContextType *context = Dcm_GetContext();
  const Dcm_EcuResetConfigType *rstConfig =
    (const Dcm_EcuResetConfigType *)context->curService->config;
  if (1 == msgContext->reqDataLen) {
    r = E_OK;
  } else {
    *nrc = DCM_E_INCORRECT_MESSAGE_LENGTH_OR_INVALID_FORMAT;
  }

  if (r == E_OK) {
    switch (msgContext->reqData[0]) {
    case 0x01: /* hard reset */
    case 0x03: /* soft reset */
      context->resetType = msgContext->reqData[0];
      if (rstConfig->delay == 0) {
        context->timer2Reset = 1;
      } else {
        context->timer2Reset = rstConfig->delay;
      }
      msgContext->resData[0] = context->resetType;
      msgContext->resDataLen = 1;
      break;
    default:
      r = E_NOT_OK;
      *nrc = DCM_E_SUB_FUNCTION_NOT_SUPPORTED;
      break;
    }
  }

  return r;
}
#endif

#ifdef DCM_USE_SERVICE_READ_DATA_BY_IDENTIFIER
Std_ReturnType Dcm_DspReadDataByIdentifier(Dcm_MsgContextType *msgContext,
                                           Dcm_NegativeResponseCodeType *nrc) {
  Std_ReturnType r = E_OK;
  Dcm_ContextType *context = Dcm_GetContext();
  const Dcm_ReadDIDConfigType *rDidConfig =
    (const Dcm_ReadDIDConfigType *)context->curService->config;
  const Dcm_ReadDIDType *rDid = NULL;
  uint16_t id;
  uint16_t numOfDids = 0;
  Dcm_MsgLenType totalResLength = 0;
  int i, j;

  if ((msgContext->reqDataLen >= 2) && ((msgContext->reqDataLen & 0x01) == 0)) {
    numOfDids = msgContext->reqDataLen >> 1;
    for (i = 0; (i < numOfDids) && (E_OK == r); i++) {
      rDid = NULL;
      id = ((uint16_t)msgContext->reqData[i * 2] << 8) + msgContext->reqData[i * 2 + 1];
      for (j = 0; j < rDidConfig->numOfDIDs; j++) {
        if (rDidConfig->DIDs[j].id == id) {
          rDid = &rDidConfig->DIDs[j];
          break;
        }
      }
      if (NULL != rDid) {
        totalResLength += rDid->length + 2;
        r = Dcm_DslServiceDIDSesSecPhyFuncCheck(context, &rDid->SesSecAccess, nrc);
      } else {
        *nrc = DCM_E_REQUEST_OUT_OF_RANGE;
        r = E_NOT_OK;
      }
    }
  } else {
    *nrc = DCM_E_INCORRECT_MESSAGE_LENGTH_OR_INVALID_FORMAT;
    r = E_NOT_OK;
  }

  if (E_OK == r) {
    if (totalResLength > msgContext->resMaxDataLen) {
      *nrc = DCM_E_RESPONSE_TOO_LONG;
      r = E_NOT_OK;
    }
  }

  if (E_OK == r) {
    totalResLength = 0;
    for (i = 0; (i < numOfDids) && (E_OK == r); i++) {
      rDid = NULL;
      id = ((uint16_t)msgContext->reqData[i * 2] << 8) + msgContext->reqData[i * 2 + 1];
      for (j = 0; j < rDidConfig->numOfDIDs; j++) {
        if (rDidConfig->DIDs[j].id == id) {
          rDid = &rDidConfig->DIDs[j];
          break;
        }
      }
      if (NULL != rDid) {
        msgContext->resData[totalResLength] = (id >> 8) & 0xFF;
        msgContext->resData[totalResLength + 1] = id & 0xFF;
        r = rDid->readDIdFnc(&msgContext->resData[totalResLength + 2], rDid->length, nrc);
        totalResLength += rDid->length + 2;
      } else {
        *nrc = DCM_E_REQUEST_OUT_OF_RANGE;
        r = E_NOT_OK;
      }
    }
    msgContext->resDataLen = totalResLength;
  }

  return r;
}
#endif

#ifdef DCM_USE_SERVICE_WRITE_DATA_BY_IDENTIFIER
Std_ReturnType Dcm_DspWriteDataByIdentifier(Dcm_MsgContextType *msgContext,
                                            Dcm_NegativeResponseCodeType *nrc) {
  Std_ReturnType r = E_OK;
  Dcm_ContextType *context = Dcm_GetContext();
  const Dcm_WriteDIDConfigType *wDidConfig =
    (const Dcm_WriteDIDConfigType *)context->curService->config;
  const Dcm_WriteDIDType *wDid = NULL;
  uint16_t id;
  int i;

  if (msgContext->reqDataLen > 2) {
    wDid = NULL;
    id = ((uint16_t)msgContext->reqData[0] << 8) + msgContext->reqData[1];
    for (i = 0; i < wDidConfig->numOfDIDs; i++) {
      if (wDidConfig->DIDs[i].id == id) {
        wDid = &wDidConfig->DIDs[i];
        break;
      }
    }

    if (NULL != wDid) {
      r = Dcm_DslServiceDIDSesSecPhyFuncCheck(context, &wDid->SesSecAccess, nrc);
    } else {
      *nrc = DCM_E_REQUEST_OUT_OF_RANGE;
      r = E_NOT_OK;
    }
  } else {
    *nrc = DCM_E_INCORRECT_MESSAGE_LENGTH_OR_INVALID_FORMAT;
    r = E_NOT_OK;
  }

  if (E_OK == r) {
    msgContext->resData[0] = (wDid->id >> 8) & 0xFF;
    msgContext->resData[1] = wDid->id & 0xFF;
    r = wDid->writeDIdFnc(context->opStatus, &msgContext->reqData[2], wDid->length, nrc);
    msgContext->resDataLen = 2;
  }

  return r;
}
#endif
#ifdef DCM_USE_SERVICE_TESTER_PRESENT
Std_ReturnType Dcm_DspTesterPresent(Dcm_MsgContextType *msgContext,
                                    Dcm_NegativeResponseCodeType *nrc) {
  Std_ReturnType r = E_NOT_OK;

  if (1 == msgContext->reqDataLen) {
    if (0x00 == msgContext->reqData[0]) {
      msgContext->resData[0] = 0x00;
      msgContext->resDataLen = 1;
    } else {
      *nrc = DCM_E_SUB_FUNCTION_NOT_SUPPORTED;
    }
  } else {
    *nrc = DCM_E_INCORRECT_MESSAGE_LENGTH_OR_INVALID_FORMAT;
  }

  return r;
}
#endif

#ifdef DCM_USE_SERVICE_CONTROL_DTC_SETTING
Std_ReturnType Dcm_DspControlDTCSetting(Dcm_MsgContextType *msgContext,
                                        Dcm_NegativeResponseCodeType *nrc) {
  Std_ReturnType r = E_NOT_OK;

  if (1 == msgContext->reqDataLen) {
    switch (msgContext->reqData[0]) {
    case 0x01: /* ON */
      Dem_EnableDTCSetting(0);
      break;
    case 0x02: /* OFF */
      Dem_DisableDTCSetting(0);
      break;
    default:
      *nrc = DCM_E_SUB_FUNCTION_NOT_SUPPORTED;
      break;
    }
  } else {
    *nrc = DCM_E_INCORRECT_MESSAGE_LENGTH_OR_INVALID_FORMAT;
  }

  if (E_OK == r) {
    msgContext->resData[0] = msgContext->reqData[0];
    msgContext->resDataLen = 1;
  }

  return r;
}
#endif

#ifdef DCM_USE_SERVICE_CLEAR_DIAGNOSTIC_INFORMATION
Std_ReturnType Dcm_DspClearDTC(Dcm_MsgContextType *msgContext, Dcm_NegativeResponseCodeType *nrc) {

  Std_ReturnType r = E_NOT_OK;
  uint32_t groupDTC = 0x0;
  Dcm_ContextType *context = Dcm_GetContext();

  if (3 == msgContext->reqDataLen) {
    groupDTC = ((uint32_t)msgContext->reqData[0] << 16) + ((uint32_t)msgContext->reqData[1] << 8) +
               msgContext->reqData[2];
    r = E_OK;
  } else {
    *nrc = DCM_E_INCORRECT_MESSAGE_LENGTH_OR_INVALID_FORMAT;
  }

  if (E_OK == r) {
    if (DCM_INITIAL == context->opStatus) {
      r = Dem_SelectDTC(0, groupDTC, DEM_DTC_FORMAT_UDS, DEM_DTC_ORIGIN_PRIMARY_MEMORY);
      if (E_OK == r) {
        r = Dem_ClearDTC(0);
      }
      if (E_OK != r) {
        *nrc = DCM_E_REQUEST_OUT_OF_RANGE;
      } else {
#ifdef USE_NVM
        *nrc = DCM_E_RESPONSE_PENDING;
#endif
      }
    } else {
#ifdef USE_NVM
      if (MEMIF_IDLE != NvM_GetStatus()) {
        *nrc = DCM_E_RESPONSE_PENDING;
      }
#endif
    }
  }

  return r;
}
#endif

#ifdef DCM_USE_SERVICE_READ_DTC_INFORMATION
Std_ReturnType Dem_DspReportNumberOfDTCByStatusMask(Dcm_MsgContextType *msgContext,
                                                    Dcm_NegativeResponseCodeType *nrc) {
  Std_ReturnType r = E_NOT_OK;
  uint8_t statusMask;
  uint16_t NumberOfFilteredDTC = 0;

  if (2 == msgContext->reqDataLen) {
    statusMask = msgContext->reqData[1];

    r = Dem_SetDTCFilter(0, statusMask, DEM_DTC_FORMAT_UDS, DEM_DTC_ORIGIN_PRIMARY_MEMORY, FALSE, 0,
                         FALSE);
    if (E_OK == r) {
      r = Dem_GetNumberOfFilteredDTC(0, &NumberOfFilteredDTC);
    }

    if (E_OK == r) {
      msgContext->resData[0] = 0x01;
      msgContext->resData[1] = statusMask;
      msgContext->resData[2] = DEM_DTC_FORMAT_UDS; /* DTCFormatIdentifier */
      msgContext->resData[3] = (uint8_t)((NumberOfFilteredDTC >> 8) & 0xFF);
      msgContext->resData[4] = (uint8_t)(NumberOfFilteredDTC & 0xFF);
      msgContext->resDataLen = 5;
    } else {
      *nrc = DCM_E_REQUEST_OUT_OF_RANGE;
    }
  } else {
    *nrc = DCM_E_INCORRECT_MESSAGE_LENGTH_OR_INVALID_FORMAT;
  }

  return r;
}

Std_ReturnType Dem_DspReportDTCByStatusMask(Dcm_MsgContextType *msgContext,
                                            Dcm_NegativeResponseCodeType *nrc) {
  Std_ReturnType r = E_NOT_OK;
  uint8_t statusMask;
  uint16_t NumberOfFilteredDTC = 0;
  uint32_t DTCNumber;
  Dem_UdsStatusByteType udsStatus;
  int i;

  if (2 == msgContext->reqDataLen) {
    statusMask = msgContext->reqData[1];

    r = Dem_SetDTCFilter(0, statusMask, DEM_DTC_FORMAT_UDS, DEM_DTC_ORIGIN_PRIMARY_MEMORY, FALSE, 0,
                         FALSE);
    if (E_OK == r) {
      r = Dem_GetNumberOfFilteredDTC(0, &NumberOfFilteredDTC);
    }

    if (E_OK == r) {
      if ((NumberOfFilteredDTC * 4 + 2) <= msgContext->resMaxDataLen) {
        msgContext->resData[0] = 0x02;
        msgContext->resData[1] = statusMask;
        for (i = 0; (i < NumberOfFilteredDTC) && (E_OK == r); i++) {
          r = Dem_GetNextFilteredDTC(0, &DTCNumber, &udsStatus);
          if (E_OK == r) {
            msgContext->resData[2 + 4 * i] = (uint8_t)((DTCNumber >> 16) & 0xFF);
            msgContext->resData[3 + 4 * i] = (uint8_t)((DTCNumber >> 8) & 0xFF);
            msgContext->resData[4 + 4 * i] = (uint8_t)(DTCNumber & 0xFF);
            msgContext->resData[5 + 4 * i] = udsStatus;
          }
        }
        if (E_OK == r) {
          msgContext->resDataLen = NumberOfFilteredDTC * 4 + 2;
        } else {
          *nrc = DCM_E_REQUEST_OUT_OF_RANGE;
        }
      } else {
        *nrc = DCM_E_RESPONSE_TOO_LONG;
        r = E_NOT_OK;
      }
    } else {
      *nrc = DCM_E_REQUEST_OUT_OF_RANGE;
    }
  } else {
    *nrc = DCM_E_INCORRECT_MESSAGE_LENGTH_OR_INVALID_FORMAT;
  }

  return r;
}

Std_ReturnType Dem_DspReportDTCSnapshotIdentification(Dcm_MsgContextType *msgContext,
                                                      Dcm_NegativeResponseCodeType *nrc) {
  Std_ReturnType r = E_NOT_OK;
  uint16_t NumberOfFilteredRecords = 0;
  uint32_t DTCNumber;
  uint8_t RecordNumber;
  int i;

  if (1 == msgContext->reqDataLen) {
    r = Dem_SetFreezeFrameRecordFilter(0, DEM_DTC_FORMAT_UDS);
    if (E_OK == r) {
      r = Dem_GetNumberOfFreezeFrameRecords(0, &NumberOfFilteredRecords);
    }

    if (E_OK == r) {
      if ((NumberOfFilteredRecords * 4 + 1) <= msgContext->resMaxDataLen) {
        msgContext->resData[0] = 0x03;
        for (i = 0; (i < NumberOfFilteredRecords) && (E_OK == r); i++) {
          r = Dem_GetNextFilteredRecord(0, &DTCNumber, &RecordNumber);
          if (E_OK == r) {
            msgContext->resData[1 + 4 * i] = (uint8_t)((DTCNumber >> 16) & 0xFF);
            msgContext->resData[2 + 4 * i] = (uint8_t)((DTCNumber >> 8) & 0xFF);
            msgContext->resData[3 + 4 * i] = (uint8_t)(DTCNumber & 0xFF);
            msgContext->resData[4 + 4 * i] = RecordNumber;
          }
        }
        if (E_OK == r) {
          msgContext->resDataLen = NumberOfFilteredRecords * 4 + 1;
        } else {
          *nrc = DCM_E_REQUEST_OUT_OF_RANGE;
        }
      } else {
        *nrc = DCM_E_RESPONSE_TOO_LONG;
        r = E_NOT_OK;
      }
    } else {
      *nrc = DCM_E_REQUEST_OUT_OF_RANGE;
    }
  } else {
    *nrc = DCM_E_INCORRECT_MESSAGE_LENGTH_OR_INVALID_FORMAT;
  }

  return r;
}

Std_ReturnType Dem_DspReportDTCSnapshotRecordByDTCNumber(Dcm_MsgContextType *msgContext,
                                                         Dcm_NegativeResponseCodeType *nrc) {
  Std_ReturnType r = E_NOT_OK;
  uint32_t DTCNumber;
  uint8_t RecordNumber;
  uint16_t resLen = msgContext->resMaxDataLen - 1;

  if (5 == msgContext->reqDataLen) {
    DTCNumber = ((uint32_t)msgContext->reqData[1] << 16) + ((uint32_t)msgContext->reqData[2] << 8) +
                msgContext->reqData[3];
    RecordNumber = msgContext->reqData[4];
    r = Dem_SelectDTC(0, DTCNumber, DEM_DTC_FORMAT_UDS, DEM_DTC_ORIGIN_PRIMARY_MEMORY);
    if (E_OK == r) {
      r = Dem_SelectFreezeFrameData(0, RecordNumber);
    }

    if (E_OK == r) {
      r = Dem_GetNextFreezeFrameData(0, &msgContext->resData[1], &resLen);
    }

    if (E_OK == r) {
      msgContext->resData[0] = 0x04;
      msgContext->resDataLen = resLen + 1;
    } else if (DEM_BUFFER_TOO_SMALL == r) {
      *nrc = DCM_E_RESPONSE_TOO_LONG;
    } else {
      *nrc = DCM_E_REQUEST_OUT_OF_RANGE;
    }
  } else {
    *nrc = DCM_E_INCORRECT_MESSAGE_LENGTH_OR_INVALID_FORMAT;
  }

  return r;
}

Std_ReturnType Dem_DspReportDTCExtendedDataRecordByDTCNumber(Dcm_MsgContextType *msgContext,
                                                             Dcm_NegativeResponseCodeType *nrc) {
  Std_ReturnType r = E_NOT_OK;
  uint32_t DTCNumber;
  uint8_t RecordNumber;
  uint16_t resLen = msgContext->resMaxDataLen - 1;

  if (5 == msgContext->reqDataLen) {
    DTCNumber = ((uint32_t)msgContext->reqData[1] << 16) + ((uint32_t)msgContext->reqData[2] << 8) +
                msgContext->reqData[3];
    RecordNumber = msgContext->reqData[4];
    r = Dem_SelectDTC(0, DTCNumber, DEM_DTC_FORMAT_UDS, DEM_DTC_ORIGIN_PRIMARY_MEMORY);
    if (E_OK == r) {
      r = Dem_SelectFreezeFrameData(0, RecordNumber);
    }

    if (E_OK == r) {
      r = Dem_GetNextExtendedDataRecord(0, &msgContext->resData[1], &resLen);
    }

    if (E_OK == r) {
      msgContext->resData[0] = 0x06;
      msgContext->resDataLen = resLen + 1;
    } else if (DEM_BUFFER_TOO_SMALL == r) {
      *nrc = DCM_E_RESPONSE_TOO_LONG;
    } else {
      *nrc = DCM_E_REQUEST_OUT_OF_RANGE;
    }
  } else {
    *nrc = DCM_E_INCORRECT_MESSAGE_LENGTH_OR_INVALID_FORMAT;
  }

  return r;
}

Std_ReturnType Dcm_DspReadDTCInformation(Dcm_MsgContextType *msgContext,
                                         Dcm_NegativeResponseCodeType *nrc) {
  Std_ReturnType r = E_NOT_OK;
  Dcm_ContextType *context = Dcm_GetContext();
  const Dcm_ReadDTCInfoConfigType *config =
    (const Dcm_ReadDTCInfoConfigType *)context->curService->config;
  const Dcm_ReadDTCSubFunctionConfigType *subFunction = NULL;
  uint8_t type;
  int i;

  if (msgContext->reqDataLen >= 1) {
    type = msgContext->reqData[0];
    for (i = 0; i < config->numOfSubFunctions; i++) {
      if (config->subFunctions[i].type == type) {
        subFunction = &config->subFunctions[i];
        r = E_OK;
        break;
      }
    }
    if (E_OK != r) {
      *nrc = DCM_E_SUB_FUNCTION_NOT_SUPPORTED;
    }
  } else {
    *nrc = DCM_E_INCORRECT_MESSAGE_LENGTH_OR_INVALID_FORMAT;
  }

  if (E_OK == r) {
    r = subFunction->subFnc(msgContext, nrc);
  }

  return r;
}
#endif

#ifdef DCM_USE_INPUT_OUTPUT_CONTROL_BY_IDENTIFIER
Std_ReturnType Dcm_DspIOControlByIdentifier(Dcm_MsgContextType *msgContext,
                                            Dcm_NegativeResponseCodeType *nrc) {
  Std_ReturnType r = E_NOT_OK;
  Dcm_ContextType *context = Dcm_GetContext();
  const Dcm_IOControlConfigType *config =
    (const Dcm_IOControlConfigType *)context->curService->config;
  const Dcm_IOControlType *IOCtrl = NULL;
  uint16_t id;
  uint8_t action;
  uint16_t resDataLen = msgContext->resMaxDataLen - 3;
  int i;
  const Dcm_IOCtrlExecuteFncType *ExecuteFncs;

  if (msgContext->reqDataLen >= 3) {
    id = ((uint16_t)msgContext->reqData[0] << 8) + msgContext->reqData[1];
    action = msgContext->reqData[2];
    for (i = 0; i < config->numOfIOCtrls; i++) {
      if (config->IOCtrls[i].id == id) {
        IOCtrl = &config->IOCtrls[i];
        r = E_OK;
        break;
      }
    }

    if (E_OK != r) {
      *nrc = DCM_E_REQUEST_OUT_OF_RANGE;
    } else {
      r = Dcm_DslServiceDIDSesSecPhyFuncCheck(context, &IOCtrl->SesSecAccess, nrc);
    }
  } else {
    *nrc = DCM_E_INCORRECT_MESSAGE_LENGTH_OR_INVALID_FORMAT;
  }

  if (E_OK == r) {
    ExecuteFncs = &(IOCtrl->ReturnControlToEcuFnc);
    if (action <= DCM_IOCTRL_SHORT_TERM_ADJUSTMENT) {
      if (ExecuteFncs[action] != NULL) {
        r = ExecuteFncs[action](&msgContext->reqData[3], msgContext->reqDataLen - 3,
                                &msgContext->resData[3], &resDataLen, nrc);
      } else {
        *nrc = DCM_E_REQUEST_OUT_OF_RANGE;
        r = E_NOT_OK;
      }
    } else {
      *nrc = DCM_E_REQUEST_OUT_OF_RANGE;
      r = E_NOT_OK;
    }
  }

  if (E_OK == r) {
    msgContext->resData[0] = (IOCtrl->id >> 8) & 0xFF;
    msgContext->resData[1] = IOCtrl->id & 0xFF;
    msgContext->resData[2] = action;
    msgContext->resDataLen = 3 + resDataLen;
  }

  /* TODO: @SWS_Dcm_00858, @SWS_Dcm_00628
   * For now, it's depend on callback Dcm_SessionChangeIndication */

  return r;
}
#endif