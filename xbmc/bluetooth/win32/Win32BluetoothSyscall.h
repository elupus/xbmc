#pragma once

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

#include "bluetooth/IBluetoothSyscall.h"
#include <BluetoothAPIs.h>

class CWin32BluetoothDevice : public IBluetoothDevice
{
public:
  CWin32BluetoothDevice(const BLUETOOTH_DEVICE_INFO_STRUCT& info);
  const char* GetID()        { return m_address.c_str(); }
  const char* GetName()      { return m_name.c_str(); }
  const char* GetAddress()   { return m_address.c_str(); }
  bool IsConnected()         { return !!m_info.fConnected; }
  bool IsCreated()           { return !!m_info.fRemembered; }
  bool IsPaired()            { return !!m_info.fAuthenticated; }
  DeviceType GetDeviceType() { return GetDeviceTypeFromClass(m_info.ulClassofDevice); }
  BLUETOOTH_DEVICE_INFO_STRUCT& GetInfo() { return m_info; }
private:
  std::string m_name;
  std::string m_address;
  BLUETOOTH_DEVICE_INFO_STRUCT m_info;
  std::vector<GUID> m_services;
};

class CWin32BluetoothSyscall : public IBluetoothSyscall
{
public:
  CWin32BluetoothSyscall();
  ~CWin32BluetoothSyscall();
  std::vector<boost::shared_ptr<IBluetoothDevice> > GetDevices() 
  { 
    std::vector<boost::shared_ptr<IBluetoothDevice> > devices;
    for(std::map<std::string, boost::shared_ptr<CWin32BluetoothDevice> >::iterator it = m_devices.begin(); it != m_devices.end(); ++it)
      devices.push_back(it->second);
    return devices;
  }

  boost::shared_ptr<IBluetoothDevice> GetDevice(const char *id)
  {
    std::map<std::string, boost::shared_ptr<CWin32BluetoothDevice> >::iterator it = m_devices.find(id);
    if(it == m_devices.end())
      return boost::shared_ptr<IBluetoothDevice>();
    else
      return boost::dynamic_pointer_cast<IBluetoothDevice>(it->second);
  }

  void StartDiscovery();
  void StopDiscovery();
  void CreateDevice(const char *address);
  void RemoveDevice(const char *id);
  void ConnectDevice(const char *id);
  void DisconnectDevice(const char *id);
  bool PumpBluetoothEvents(IBluetoothEventsCallback *callback);

private:
  std::map<std::string, boost::shared_ptr<CWin32BluetoothDevice> > m_devices;
  HBLUETOOTH_AUTHENTICATION_REGISTRATION m_auth;
};

