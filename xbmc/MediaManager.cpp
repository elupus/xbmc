/*
 *      Copyright (C) 2005-2008 Team XBMC
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

#include "stdafx.h"
#include "MediaManager.h"
#include "xbox/IoSupport.h"
#include "URL.h"
#include "Util.h"
#ifdef _LINUX
#include "LinuxFileSystem.h"
#elif defined(_WIN32PC)
#include "WIN32Util.h"
#endif
#include "GUIWindowManager.h"

using namespace std;

const char MEDIA_SOURCES_XML[] = { "special://profile/mediasources.xml" };

class CMediaManager g_mediaManager;

CMediaManager::CMediaManager()
{
}

bool CMediaManager::LoadSources()
{
  // clear our location list
  m_locations.clear();

  // load xml file...
  TiXmlDocument xmlDoc;
  if ( !xmlDoc.LoadFile( MEDIA_SOURCES_XML ) )
    return false;

  TiXmlElement* pRootElement = xmlDoc.RootElement();
  if ( !pRootElement || strcmpi(pRootElement->Value(), "mediasources") != 0)
  {
    CLog::Log(LOGERROR, "Error loading %s, Line %d (%s)", MEDIA_SOURCES_XML, xmlDoc.ErrorRow(), xmlDoc.ErrorDesc());
    return false;
  }

  // load the <network> block
  TiXmlNode *pNetwork = pRootElement->FirstChild("network");
  if (pNetwork)
  {
    TiXmlElement *pLocation = pNetwork->FirstChildElement("location");
    while (pLocation)
    {
      CNetworkLocation location;
      pLocation->Attribute("id", &location.id);
      if (pLocation->FirstChild())
      {
        location.path = pLocation->FirstChild()->Value();
        m_locations.push_back(location);
      }
      pLocation = pLocation->NextSiblingElement("location");
    }
  }
  return true;
}

bool CMediaManager::SaveSources()
{
  TiXmlDocument xmlDoc;
  TiXmlElement xmlRootElement("mediasources");
  TiXmlNode *pRoot = xmlDoc.InsertEndChild(xmlRootElement);
  if (!pRoot) return false;

  TiXmlElement networkNode("network");
  TiXmlNode *pNetworkNode = pRoot->InsertEndChild(networkNode);
  if (pNetworkNode)
  {
    for (vector<CNetworkLocation>::iterator it = m_locations.begin(); it != m_locations.end(); it++)
    {
      TiXmlElement locationNode("location");
      locationNode.SetAttribute("id", (*it).id);
      TiXmlText value((*it).path);
      locationNode.InsertEndChild(value);
      pNetworkNode->InsertEndChild(locationNode);
    }
  }
  return xmlDoc.SaveFile(MEDIA_SOURCES_XML);
}

void CMediaManager::GetLocalDrives(VECSOURCES &localDrives, bool includeQ)
{

#ifdef _WIN32PC
  CMediaSource share;
  if (includeQ)
  {
    CMediaSource share;
    share.strPath = "special://xbmc/";
    share.strName.Format(g_localizeStrings.Get(21438),'Q');
    share.m_ignore = true;
    localDrives.push_back(share) ;
  }

  CWIN32Util::GetDrivesByType(localDrives, LOCAL_DRIVES);

#else
#ifndef _LINUX
  // Local shares
  CMediaSource share;
  share.strPath = "C:\\";
  share.strName.Format(g_localizeStrings.Get(21438),'C');
  share.m_ignore = true;
  share.m_iDriveType = CMediaSource::SOURCE_TYPE_LOCAL;
  localDrives.push_back(share);
#endif

#ifdef _LINUX
  // Home directory
  CMediaSource share;
  share.strPath = getenv("HOME");
  share.strName = g_localizeStrings.Get(21440);
  share.m_ignore = true;
  share.m_iDriveType = CMediaSource::SOURCE_TYPE_LOCAL;
  localDrives.push_back(share);
#endif

  share.strPath = "D:\\";
  share.strName = g_localizeStrings.Get(218);
  share.m_iDriveType = CMediaSource::SOURCE_TYPE_DVD;
  localDrives.push_back(share);

  share.m_iDriveType = CMediaSource::SOURCE_TYPE_LOCAL;
#ifndef _LINUX
  share.strPath = "E:\\";
  share.strName.Format(g_localizeStrings.Get(21438),'E');
  localDrives.push_back(share);
  for (char driveletter=EXTEND_DRIVE_BEGIN; driveletter<=EXTEND_DRIVE_END; driveletter++)
  {
    if (CIoSupport::DriveExists(driveletter))
    {
      CMediaSource share;
      share.strPath.Format("%c:\\", driveletter);
      share.strName.Format(g_localizeStrings.Get(21438),driveletter);
      share.m_ignore = true;
      localDrives.push_back(share);
    }
  }
  if (includeQ)
  {
    CMediaSource share;
    share.strPath = "special://xbmc/";
    share.strName.Format(g_localizeStrings.Get(21438),'Q');
    share.m_ignore = true;
    localDrives.push_back(share) ;
  }
#endif

#ifdef _LINUX
  CLinuxFileSystem::GetLocalDrives(localDrives);
#endif
#endif
}

void CMediaManager::GetRemovableDrives(VECSOURCES &removableDrives)
{
#ifdef _LINUX
  CLinuxFileSystem::GetRemovableDrives(removableDrives); 
#endif
}

void CMediaManager::GetNetworkLocations(VECSOURCES &locations)
{
  // Load our xml file
  LoadSources();
  for (unsigned int i = 0; i < m_locations.size(); i++)
  {
    CMediaSource share;
    share.strPath = m_locations[i].path;
    CURL url(share.strPath);
    url.GetURLWithoutUserDetails(share.strName);
    locations.push_back(share);
  }
}

bool CMediaManager::AddNetworkLocation(const CStdString &path)
{
  CNetworkLocation location;
  location.path = path;
  location.id = (int)m_locations.size();
  m_locations.push_back(location);
  return SaveSources();
}

bool CMediaManager::HasLocation(const CStdString& path) const
{
  for (unsigned int i=0;i<m_locations.size();++i)
  {
    if (m_locations[i].path == path)
      return true;
  }

  return false;
}


bool CMediaManager::RemoveLocation(const CStdString& path)
{
  for (unsigned int i=0;i<m_locations.size();++i)
  {
    if (m_locations[i].path == path)
    {
      // prompt for sources, remove, cancel,
      m_locations.erase(m_locations.begin()+i);
      return SaveSources();
    }
  }

  return false;
}

bool CMediaManager::SetLocationPath(const CStdString& oldPath, const CStdString& newPath)
{
  for (unsigned int i=0;i<m_locations.size();++i)
  {
    if (m_locations[i].path == oldPath)
    {
      m_locations[i].path = newPath;
      return SaveSources();
    }
  }

  return false;
}

void CMediaManager::AddAutoSource(const CMediaSource &share)
{
  g_settings.AddShare("files",share);
  g_settings.AddShare("video",share);
  g_settings.AddShare("pictures",share);
  g_settings.AddShare("music",share);
  g_settings.AddShare("programs",share);
  CGUIMessage msg(GUI_MSG_NOTIFY_ALL, 0, 0, GUI_MSG_UPDATE_SOURCES);
  m_gWindowManager.SendThreadMessage( msg );
}

void CMediaManager::RemoveAutoSource(const CMediaSource &share)
{
  g_settings.DeleteSource("files", share.strName, share.strPath, true);
  g_settings.DeleteSource("video", share.strName, share.strPath, true);
  g_settings.DeleteSource("pictures", share.strName, share.strPath, true);
  g_settings.DeleteSource("music", share.strName, share.strPath, true);
  g_settings.DeleteSource("programs", share.strName, share.strPath, true);
  CGUIMessage msg(GUI_MSG_NOTIFY_ALL, 0, 0, GUI_MSG_UPDATE_SOURCES);
  m_gWindowManager.SendThreadMessage( msg );
}

/////////////////////////////////////////////////////////////
// AutoSource status functions:
// - check only in video as auto source is added to all types
// - could be also implemented as direct call to the device
// - TODO: translate cdda://local/ and cdda://<device>/

CStdString CMediaManager::translateDevice(const CStdString& devicePath)
{
  CStdString strDevice = devicePath;
#ifdef _WIN32PC
  CUtil::RemoveSlashAtEnd(strDevice);
#endif
  return strDevice;
}

bool CMediaManager::IsDiscInDrive(const CStdString& devicePath)
{  
  VECSOURCES *pShares = g_settings.GetSourcesFromType("video");
  if (!pShares) return false;

  VECSOURCES::const_iterator it;
  for(it=pShares->begin();it!=pShares->end();++it)
    if(it->strPath.Equals(translateDevice(devicePath))) return true;

  return false;
}

bool CMediaManager::IsAudio(const CStdString& devicePath)
{
  VECSOURCES *pShares = g_settings.GetSourcesFromType("video");
  if (!pShares) return false;
  
  VECSOURCES::const_iterator it;
  for(it=pShares->begin();it!=pShares->end();++it)
    if(it->strPath.Equals(translateDevice(devicePath)) && it->strStatus.Equals("Audio-CD")) return true;

  return false;
}

// End AutoSource status functions
//////////////////////////////////