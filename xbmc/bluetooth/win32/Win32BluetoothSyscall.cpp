/*
 *      Copyright (C) 2005-2011 Team XBMC
 *      http://www.xbmc.org
 *
 *  This Program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This Program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with XBMC; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 *  http://www.gnu.org/copyleft/gpl.html
 *
 */

#define NTDDI_VERSION NTDDI_VISTASP2

#include "system.h"
#include "dialogs/GUIDialogKeyboard.h"
#include "dialogs/GUIDialogNumeric.h"
#include "dialogs/GUIDialogOK.h"
#include "dialogs/GUIDialogYesNo.h"
#include "dialogs/GUIDialogKaiToast.h"
#include "guilib/GUIWindowManager.h"
#include "utils/log.h"
#include "guilib/LocalizeStrings.h"
#include "utils/CharsetConverter.h"
#include "threads/Thread.h"

#include <string>
#include "Win32BluetoothSyscall.h"
#include <BluetoothAPIs.h>

#pragma comment(lib, "Bthprops.lib")

#if (NTDDI_VERSION > NTDDI_VISTASP1 || \
    (NTDDI_VERSION == NTDDI_VISTASP1 && defined(VISTA_KB942567)))
#define BLUETOOTH_AUTHENTICATE_EX 1
#else
#define BLUETOOTH_AUTHENTICATE_EX 0
#endif


/**************** CWin32BluetoothDevice ***************/

CWin32BluetoothDevice::CWin32BluetoothDevice(const BLUETOOTH_DEVICE_INFO_STRUCT& info)
  : m_info(info)
{
  CStdString buf;
  g_charsetConverter.wToUTF8(info.szName, buf);
  m_name = buf;
  buf.Format("%02x:%02x:%02x:%02x:%02x:%02x", info.Address.rgBytes[0]
                                            , info.Address.rgBytes[1]
                                            , info.Address.rgBytes[2]
                                            , info.Address.rgBytes[3]
                                            , info.Address.rgBytes[4]
                                            , info.Address.rgBytes[5]);
  m_address   = buf;

  GUID  services[32];
  DWORD count = sizeof(services) / sizeof(services[0]);
  DWORD result = BluetoothEnumerateInstalledServices(NULL, &info, &count, services);
  m_services.assign(services, services+count);
  
}

/**************** CWin32BluetoothSyscall ***************/
#if (BLUETOOTH_AUTHENTICATE_EX)
static BOOL CALLBACK AuthCallbackEx(LPVOID pvParam, PBLUETOOTH_AUTHENTICATION_CALLBACK_PARAMS params)
{
  CStdString key;

  BLUETOOTH_AUTHENTICATE_RESPONSE resp = {0};
  resp.authMethod       = params->authenticationMethod;
  resp.bthAddressRemote = params->deviceInfo.Address;

  switch(params->authenticationMethod) {
    case BLUETOOTH_AUTHENTICATION_METHOD_PASSKEY_NOTIFICATION: {
      key.Format("%lu", params->Passkey);
      if(!CGUIDialogYesNo::ShowAndGetInput(g_localizeStrings.Get(16500), g_localizeStrings.Get(16507), key, ""))
        return FALSE;
      break;
    }
    case BLUETOOTH_AUTHENTICATION_METHOD_PASSKEY:
    case BLUETOOTH_AUTHENTICATION_METHOD_LEGACY: {
      if(!CGUIDialogKeyboard::ShowAndGetInput(key, g_localizeStrings.Get(16506), true, true))
        return FALSE;
      break;
    }
    case BLUETOOTH_AUTHENTICATION_METHOD_NUMERIC_COMPARISON: {
      key.Format("%lu", params->Numeric_Value);
      if(!CGUIDialogYesNo::ShowAndGetInput(g_localizeStrings.Get(16500), g_localizeStrings.Get(16505), key, ""))
        return FALSE;
      break;
    }
    default:
      CLog::Log(LOGWARNING, "CWin32BluetoothSyscall - unsupported authentication method requested %d", params->authenticationMethod);
      return FALSE;
  }

  switch(resp.authMethod) {

    case BLUETOOTH_AUTHENTICATION_METHOD_LEGACY: {
      resp.pinInfo.pinLength = key.length();
      memcpy(resp.pinInfo.pin, key.c_str(),  resp.pinInfo.pinLength);
      break;
    }
    case BLUETOOTH_AUTHENTICATION_METHOD_PASSKEY_NOTIFICATION: {
      resp.passkeyInfo.passkey = params->Passkey;
      break;
    }
    case BLUETOOTH_AUTHENTICATION_METHOD_PASSKEY: {
      resp.passkeyInfo.passkey = atol(key.c_str());
      break;
    }
    case BLUETOOTH_AUTHENTICATION_METHOD_NUMERIC_COMPARISON: {
      resp.numericCompInfo.NumericValue = params->Numeric_Value;
      break;
    }
    default:
      return FALSE;
  }

  DWORD result = BluetoothSendAuthenticationResponseEx(NULL, &resp);
  if(result == ERROR_SUCCESS)
  {
    CGUIDialogKaiToast::QueueNotification(g_localizeStrings.Get(16500), "Paired with device");
    return TRUE;
  }
  else
  {
    CGUIDialogKaiToast::QueueNotification(g_localizeStrings.Get(16500), "Pairing failed with device");
    CLog::Log(LOGWARNING, "CWin32BluetoothSyscall - failed to authenticate device with error 0x%x", result);
    return FALSE;
  }
}

#else

static BOOL CALLBACK AuthCallback(LPVOID pvParam, PBLUETOOTH_DEVICE_INFO params)
{
  CStdString key;
  if(!CGUIDialogKeyboard::ShowAndGetInput(key, g_localizeStrings.Get(16506), true, true))
    return FALSE;
  CStdStringW keyW;
  g_charsetConverter.utf8ToW(key, keyW);
  DWORD result = BluetoothSendAuthenticationResponse(NULL, params, keyW.c_str());
  if(result == ERROR_SUCCESS)
  {
    CGUIDialogKaiToast::QueueNotification(g_localizeStrings.Get(16500), "Paired with device");
    return TRUE;
  }
  else
  {
    CGUIDialogKaiToast::QueueNotification(g_localizeStrings.Get(16500), "Pairing failed with device");
    CLog::Log(LOGWARNING, "CWin32BluetoothSyscall - failed to authenticate device with error 0x%x", result);
    return FALSE;
  }
}
#endif

CWin32BluetoothSyscall::CWin32BluetoothSyscall()
{
  BLUETOOTH_DEVICE_INFO_STRUCT info = {0};
  info.dwSize = sizeof(info);
	info.fAuthenticated = TRUE;
	info.fConnected = TRUE;
	info.fRemembered = TRUE;
#if (BLUETOOTH_AUTHENTICATE_EX)
  DWORD result = BluetoothRegisterForAuthenticationEx(&info, &m_auth, (PFN_AUTHENTICATION_CALLBACK_EX)AuthCallbackEx, this);
#else
  DWORD result = BluetoothRegisterForAuthentication(&info, &m_auth, (PFN_AUTHENTICATION_CALLBACK)AuthCallback, this);
#endif
  if(result != ERROR_SUCCESS)
    CLog::Log(LOGERROR, "CWin32BluetoothSyscall - failed to register for authentication 0x%x", result);
}

CWin32BluetoothSyscall::~CWin32BluetoothSyscall()
{
  if(!BluetoothUnregisterAuthentication(m_auth))
    CLog::Log(LOGERROR, "CWin32BluetoothSyscall - failed to unregister for authentication");
}

std::vector<BLUETOOTH_DEVICE_INFO_STRUCT> GetDeviceList(const BLUETOOTH_DEVICE_SEARCH_PARAMS& params)
{
  BLUETOOTH_DEVICE_INFO_STRUCT result = {0};
  result.dwSize = sizeof(result);

  HBLUETOOTH_DEVICE_FIND handle;
  
  handle = BluetoothFindFirstDevice(&params, &result);
  BOOL ok = handle == NULL ? FALSE : TRUE;

  std::vector<BLUETOOTH_DEVICE_INFO_STRUCT> devices;
  while(ok)
  {
    devices.push_back(result);
    ok = BluetoothFindNextDevice(handle, &result);
  }

  if(handle)
    BluetoothFindDeviceClose(handle);
  return devices;
}

class DiscoveryDeviceThread : public CThread
{
public:
  DiscoveryDeviceThread() 
    : CThread("Bluetooth Discovery")
  {}

  virtual void Process()
  {
    while(!m_bStop)
    {
      BLUETOOTH_DEVICE_SEARCH_PARAMS params = {0};
      params.dwSize = sizeof(params);
      params.fReturnAuthenticated = TRUE;
      params.fReturnConnected = TRUE;
      params.fReturnRemembered = TRUE;
      params.fReturnUnknown = TRUE;
      params.fIssueInquiry = TRUE;
      params.cTimeoutMultiplier = 1;
      params.hRadio = NULL;

      std::vector<BLUETOOTH_DEVICE_INFO_STRUCT> devices = GetDeviceList(params);
      CSingleLock lock(m_section);
      m_devices = devices;
      Sleep(500);
    }
  }

  CCriticalSection                          m_section;
  std::vector<BLUETOOTH_DEVICE_INFO_STRUCT> m_devices;
};

DiscoveryDeviceThread* g_discovery;

void CWin32BluetoothSyscall::StartDiscovery()
{
  if(g_discovery == NULL)
  {
    g_discovery = new DiscoveryDeviceThread();
    g_discovery->Create();
  }
}

void CWin32BluetoothSyscall::StopDiscovery()
{
  if(g_discovery)
  {
    g_discovery->StopThread(true);
    delete g_discovery;
    g_discovery = NULL;
  }
}

class AuthenticateDeviceThread : public CThread
{
public:
  AuthenticateDeviceThread(const BLUETOOTH_DEVICE_INFO_STRUCT& info) 
    : CThread("Bluetooth Authentication")
    , m_info(info)
  {}

  virtual void Process()
  {
#if (BLUETOOTH_AUTHENTICATE_EX)
    DWORD res = BluetoothAuthenticateDeviceEx(NULL, NULL, &m_info, NULL, MITMProtectionNotRequiredGeneralBonding);
#else
    DWORD res = BluetoothAuthenticateDevice(NULL, NULL, &m_info, NULL, 0);
#endif
    if(res != ERROR_SUCCESS)
      CLog::Log(LOGERROR, "CWin32BluetoothSyscall::BluetoothAuthenticateDevicex - failed with code 0x%x", res);
  }

  BLUETOOTH_DEVICE_INFO_STRUCT m_info;
};

void CWin32BluetoothSyscall::CreateDevice(const char *address)
{
  boost::shared_ptr<CWin32BluetoothDevice> device = boost::dynamic_pointer_cast<CWin32BluetoothDevice>(GetDevice(address));
  if(device)
  {
    AuthenticateDeviceThread* thread = new AuthenticateDeviceThread(device->GetInfo());
    thread->Create(true);
  }
}

void CWin32BluetoothSyscall::RemoveDevice(const char *id)
{
  
  boost::shared_ptr<CWin32BluetoothDevice> device = boost::dynamic_pointer_cast<CWin32BluetoothDevice>(GetDevice(id));
  if(device)
  {
    if(BluetoothRemoveDevice(&device->GetInfo().Address) == ERROR_SUCCESS)
      m_devices.erase(device->GetID());
  }
}

void CWin32BluetoothSyscall::ConnectDevice(const char *id)
{
}

void CWin32BluetoothSyscall::DisconnectDevice(const char *id)
{
}

bool CWin32BluetoothSyscall::PumpBluetoothEvents(IBluetoothEventsCallback *callback)
{
  if(g_discovery)
  {
    std::vector<BLUETOOTH_DEVICE_INFO_STRUCT> devices;
    { CSingleLock lock(g_discovery->m_section);
      devices = g_discovery->m_devices;
    }

    std::map<std::string, boost::shared_ptr<CWin32BluetoothDevice> > remain(m_devices);

    for(std::vector<BLUETOOTH_DEVICE_INFO_STRUCT>::iterator it = devices.begin(); it != devices.end(); ++it)
    {
      boost::shared_ptr<CWin32BluetoothDevice> p(new CWin32BluetoothDevice(*it));
      m_devices[p->GetID()] = p;

      std::map<std::string, boost::shared_ptr<CWin32BluetoothDevice> >::iterator cur = remain.find(p->GetID());
      if(cur == remain.end())
      {
        callback->OnDeviceFound(p.get());
      }
      else
      {
        if(!cur->second->IsConnected() &&  p->IsConnected())
          callback->OnDeviceConnected(cur->second.get());
        if( cur->second->IsConnected() && !p->IsConnected())
          callback->OnDeviceDisconnected(cur->second.get());

        if(!cur->second->IsCreated() &&  p->IsCreated())
          callback->OnDeviceCreated(cur->second.get());

        if( cur->second->IsPaired() != p->IsPaired())
          callback->OnDeviceCreated(cur->second.get());
        remain.erase(cur);
      }
    }

    for(std::map<std::string, boost::shared_ptr<CWin32BluetoothDevice> >::iterator it = remain.begin(); it != remain.end(); ++it)
    {
      m_devices.erase(it->first);
      callback->OnDeviceRemoved(it->second->GetAddress());
    }
  }

  return false;
}

