
#include "../../../../stdafx.h"
#include "DVDAudioCodecLibFaad.h"
#include "..\..\DVDPLayerDLL.h"

#define DLL_LIBFAAD "Q:\\system\\players\\dvdplayer\\libfaad.dll"

CDVDAudioCodecLibFaad::CDVDAudioCodecLibFaad() : CDVDAudioCodec()
{
  m_bDllLoaded = false;
  m_bInitializedDecoder = false;
  
  m_pHandle = NULL;
}

CDVDAudioCodecLibFaad::~CDVDAudioCodecLibFaad()
{
  Dispose();
}

bool CDVDAudioCodecLibFaad::Open(CodecID codecID, int iChannels, int iSampleRate, int iBits)
{
  // for safety
  if (m_pHandle) Dispose();
  
  if (!m_bDllLoaded)
  {
    DllLoader* pDll = g_sectionLoader.LoadDLL(DLL_LIBFAAD);
    if (!pDll)
    {
      CLog::Log(LOGERROR, "CDVDAudioCodecLibFaad: Unable to load dll %s", DLL_LIBFAAD);
      return false;
    }
    
    if (!dvdplayer_load_dll_libfaad(*pDll))
    {
      CLog::Log(LOGERROR, "CDVDAudioCodecLibFaad: Unable to resolve exports from %s", DLL_LIBFAAD);
      Dispose();
      return false;
    }
    m_bDllLoaded = true;
  }
  
  memset(&m_frameInfo, 0, sizeof(m_frameInfo));
  
  return OpenDecoder();
}

void CDVDAudioCodecLibFaad::Dispose()
{
  CloseDecoder();
  
  if (m_bDllLoaded)
  {
    g_sectionLoader.UnloadDLL(DLL_LIBFAAD);
    m_bDllLoaded = false;
  }
}

int CDVDAudioCodecLibFaad::Decode(BYTE* pData, int iSize)
{
  m_iDecodedDataSize = 0;
  
  if (m_pHandle)
  {
    int iBytesFree = LIBFAAD_INPUT_SIZE - m_iInputBufferSize;
    int iBytesToCopy = iSize;
    
    if (iBytesToCopy > iBytesFree) iBytesToCopy = iBytesFree;
    
    fast_memcpy(m_inputBuffer + m_iInputBufferSize, pData, iBytesToCopy);
    m_iInputBufferSize += iBytesToCopy;
    
    // if the caller does not supply enough data, return
    if (m_iInputBufferSize < FAAD_MIN_STREAMSIZE)
    {
      return iBytesToCopy;
    }
    
    // initialize decoder if needed
    if (!m_bInitializedDecoder)
    {
      unsigned long samplerate;
      unsigned char channels;
      
      int res = faacDecInit(m_pHandle, m_inputBuffer, m_iInputBufferSize, &samplerate, &channels);
      if (0 == res)
      {
        m_iSourceSampleRate = samplerate;
        m_iSourceChannels = channels;
        m_iSourceBitrate = 0;
    
        m_bInitializedDecoder = true;
      }
    }
    
    BYTE* pInputData = m_inputBuffer;
    bool bFullDecodedBuffer = false;
    
    // decode as long there is enough source data available, and
    // the output buffer has enough free bytes to save the decoded data
    while (m_iInputBufferSize >= FAAD_MIN_STREAMSIZE && !bFullDecodedBuffer)
    {
      void* pSamples = faacDecDecode(m_pHandle, &m_frameInfo, pInputData, m_iInputBufferSize);
      if (!m_frameInfo.error && pSamples)
      {
        // we set this info again, it could be this info changed 
        m_iSourceSampleRate = m_frameInfo.samplerate;
        m_iSourceChannels = m_frameInfo.channels;
        m_iSourceBitrate = 0;
        
        // copy the data
        if (m_frameInfo.samples > 0)
        {
          int iSize = m_frameInfo.samples * sizeof(short);
          fast_memcpy(m_decodedData + m_iDecodedDataSize, pSamples, iSize);
          m_iDecodedDataSize += iSize;
          
          // check for the next run if can can save all decoded data
          if ((m_iDecodedDataSize + iSize) > LIBFAAD_DECODED_SIZE)
          {
            bFullDecodedBuffer = true;
          }
        }
        
        m_iInputBufferSize -= m_frameInfo.bytesconsumed;
        pInputData += m_frameInfo.bytesconsumed;
      }
      else
      {
        m_iInputBufferSize = 0;
        pInputData = NULL; // this pointer should not be used anymore
        
        if (m_frameInfo.error)
        {
          char* strError = faacDecGetErrorMessage(m_frameInfo.error);
          CLog::Log(LOGERROR, "CDVDAudioCodecLibFaad() : %s", strError);
        }
      }
    }
    
    // move remaining data to start of buffer
    if (m_iInputBufferSize > 0)
    {
      memmove(m_inputBuffer, pInputData, m_iInputBufferSize);
    }
        
    return iBytesToCopy;
  }
  
  return 0;
}

int CDVDAudioCodecLibFaad::GetData(BYTE** dst)
{
  *dst = m_decodedData;
  return m_iDecodedDataSize;
}

void CDVDAudioCodecLibFaad::Reset()
{
  CloseDecoder();
  OpenDecoder();
}

void CDVDAudioCodecLibFaad::CloseDecoder()
{
  if (m_pHandle)
  {
    faacDecClose(m_pHandle);
    m_pHandle = NULL;
  }
}

bool CDVDAudioCodecLibFaad::OpenDecoder()
{
  if (m_pHandle)
  {
    CLog::Log(LOGWARNING, "CDVDAudioCodecLibFaad : Decoder already opened");
    return false;
  }
  
  m_bInitializedDecoder = false;
  
  m_iInputBufferSize = 0;
  m_iDecodedDataSize = 0;
  
  m_iSourceSampleRate = 0;
  m_iSourceChannels = 0;
  m_iSourceBitrate = 0;
  
  m_pHandle = faacDecOpen();
  
  if (m_pHandle)
  {
    faacDecConfigurationPtr pConfiguration;
    pConfiguration = faacDecGetCurrentConfiguration(m_pHandle);
    
    // modify some stuff here
    pConfiguration->outputFormat = FAAD_FMT_16BIT; // already default
    
    faacDecSetConfiguration(m_pHandle, pConfiguration);
    
    return true;
  }
  
  return false;
}
