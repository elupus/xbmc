/*
 *      Copyright (C) 2013 Team XBMC
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
 *  along with XBMC; see the file COPYING.  If not, see
 *  <http://www.gnu.org/licenses/>.
 *
 */

#include <float.h>
#include <stdlib.h>

#include "DisplaySettings.h"
#include "dialogs/GUIDialogYesNo.h"
#include "guilib/GraphicContext.h"
#include "guilib/gui3d.h"
#include "guilib/LocalizeStrings.h"
#include "guilib/StereoscopicsManager.h"
#include "settings/AdvancedSettings.h"
#include "settings/Setting.h"
#include "settings/Settings.h"
#include "threads/SingleLock.h"
#include "utils/log.h"
#include "utils/StringUtils.h"
#include "utils/XMLUtils.h"
#include "windowing/WindowingFactory.h"

// 0.1 second increments
#define MAX_REFRESH_CHANGE_DELAY 200

using namespace std;

static RESOLUTION_INFO EmptyResolution;
static RESOLUTION_INFO EmptyModifiableResolution;

float square_error(float x, float y)
{
  float yonx = (x > 0) ? y / x : 0;
  float xony = (y > 0) ? x / y : 0;
  return std::max(yonx, xony);
}

CDisplaySettings::CDisplaySettings()
{
  m_resolutions.insert(m_resolutions.begin(), RES_CUSTOM, RESOLUTION_INFO());

  m_zoomAmount = 1.0f;
  m_pixelRatio = 1.0f;
  m_verticalShift = 0.0f;
  m_nonLinearStretched = false;
}

CDisplaySettings::~CDisplaySettings()
{ }

CDisplaySettings& CDisplaySettings::Get()
{
  static CDisplaySettings sDisplaySettings;
  return sDisplaySettings;
}

bool CDisplaySettings::Load(const TiXmlNode *settings)
{
  CSingleLock lock(m_critical);
  m_calibrations.clear();

  if (settings == NULL)
    return false;

  const TiXmlElement *pElement = settings->FirstChildElement("resolutions");
  if (!pElement)
  {
    CLog::Log(LOGERROR, "CDisplaySettings: settings file doesn't contain <resolutions>");
    return false;
  }

  const TiXmlElement *pResolution = pElement->FirstChildElement("resolution");
  while (pResolution)
  {
    // get the data for this calibration
    RESOLUTION_INFO cal;

    XMLUtils::GetString(pResolution, "description", cal.strMode);
    XMLUtils::GetInt(pResolution, "subtitles", cal.iSubtitles);
    XMLUtils::GetFloat(pResolution, "pixelratio", cal.fPixelRatio);
#ifdef HAS_XRANDR
    XMLUtils::GetFloat(pResolution, "refreshrate", cal.fRefreshRate);
    XMLUtils::GetString(pResolution, "output", cal.strOutput);
    XMLUtils::GetString(pResolution, "xrandrid", cal.strId);
#endif

    const TiXmlElement *pOverscan = pResolution->FirstChildElement("overscan");
    if (pOverscan)
    {
      XMLUtils::GetInt(pOverscan, "left", cal.Overscan.left);
      XMLUtils::GetInt(pOverscan, "top", cal.Overscan.top);
      XMLUtils::GetInt(pOverscan, "right", cal.Overscan.right);
      XMLUtils::GetInt(pOverscan, "bottom", cal.Overscan.bottom);
    }

    // mark calibration as not updated
    // we must not delete those, resolution just might not be available
    cal.iWidth = cal.iHeight = 0;

    // store calibration, avoid adding duplicates
    bool found = false;
    for (ResolutionInfos::const_iterator  it = m_calibrations.begin(); it != m_calibrations.end(); ++it)
    {
      if (it->strMode.Equals(cal.strMode))
      {
        found = true;
        break;
      }
    }
    if (!found)
      m_calibrations.push_back(cal);

    // iterate around
    pResolution = pResolution->NextSiblingElement("resolution");
  }

  ApplyCalibrations();
  return true;
}

bool CDisplaySettings::Save(TiXmlNode *settings) const
{
  if (settings == NULL)
    return false;

  CSingleLock lock(m_critical);
  TiXmlElement xmlRootElement("resolutions");
  TiXmlNode *pRoot = settings->InsertEndChild(xmlRootElement);
  if (pRoot == NULL)
    return false;

  // save calibrations
  for (ResolutionInfos::const_iterator it = m_calibrations.begin(); it != m_calibrations.end(); ++it)
  {
    // Write the resolution tag
    TiXmlElement resElement("resolution");
    TiXmlNode *pNode = pRoot->InsertEndChild(resElement);
    if (pNode == NULL)
      return false;

    // Now write each of the pieces of information we need...
    XMLUtils::SetString(pNode, "description", it->strMode);
    XMLUtils::SetInt(pNode, "subtitles", it->iSubtitles);
    XMLUtils::SetFloat(pNode, "pixelratio", it->fPixelRatio);
#ifdef HAS_XRANDR
    XMLUtils::SetFloat(pNode, "refreshrate", it->fRefreshRate);
    XMLUtils::SetString(pNode, "output", it->strOutput);
    XMLUtils::SetString(pNode, "xrandrid", it->strId);
#endif

    // create the overscan child
    TiXmlElement overscanElement("overscan");
    TiXmlNode *pOverscanNode = pNode->InsertEndChild(overscanElement);
    if (pOverscanNode == NULL)
      return false;

    XMLUtils::SetInt(pOverscanNode, "left", it->Overscan.left);
    XMLUtils::SetInt(pOverscanNode, "top", it->Overscan.top);
    XMLUtils::SetInt(pOverscanNode, "right", it->Overscan.right);
    XMLUtils::SetInt(pOverscanNode, "bottom", it->Overscan.bottom);
  }

  return true;
}

void CDisplaySettings::Clear()
{
  CSingleLock lock(m_critical);
  m_calibrations.clear();
  m_resolutions.clear();

  m_zoomAmount = 1.0f;
  m_pixelRatio = 1.0f;
  m_verticalShift = 0.0f;
  m_nonLinearStretched = false;
}

bool CDisplaySettings::OnSettingChanging(const CSetting *setting)
{
  if (setting == NULL)
    return false;

  const std::string &settingId = setting->GetId();
  if (settingId == "videoscreen.resolution" ||
      settingId == "videoscreen.screen" ||
      settingId == "videoscreen.screenmode")
  {
    // check if this is the revert call for a failed OnSettingChanging
    // in which case we don't want to ask the user again
    if (m_ignoreSettingChanging.find(make_pair(settingId, true)) == m_ignoreSettingChanging.end())
    {
      RESOLUTION newRes = RES_DESKTOP;
      DisplayMode oldDisplayMode = GetCurrentDisplayMode();
      if (settingId == "videoscreen.resolution")
        newRes = (RESOLUTION)((CSettingInt*)setting)->GetValue();
      else if (settingId == "videoscreen.screen")
        newRes = GetResolutionForScreen();
      else if (settingId == "videoscreen.screenmode")
        newRes = GetResolutionFromString(((CSettingString*)setting)->GetValue());

      // We need to change and save videoscreen.screenmode which will
      // trigger another call to this OnSettingChanging() which should not
      // trigger a user-input dialog which is already triggered by the callback
      // of the changed setting
      bool save = settingId != "videoscreen.screenmode";
      if (save)
        m_ignoreSettingChanging.insert(make_pair("videoscreen.screenmode", true));
      SetCurrentResolution(newRes, save);
      g_graphicsContext.SetVideoResolution(newRes);

      // check if the old or the new resolution was/is windowed
      // in which case we don't show any prompt to the user
      if (newRes != RES_WINDOW && oldDisplayMode != DM_WINDOWED)
      {
        // check if this setting is temporarily blocked from showing the dialog
        if (newRes != RES_WINDOW &&
            m_ignoreSettingChanging.find(make_pair(settingId, false)) == m_ignoreSettingChanging.end())
        {
          bool cancelled = false;
          if (!CGUIDialogYesNo::ShowAndGetInput(13110, 13111, 20022, 20022, -1, -1, cancelled, 10000))
          {
            // we need to ignore the next OnSettingChanging() call for
            // the same setting which is executed to broadcast that
            // changing the setting has failed
            m_ignoreSettingChanging.insert(make_pair(settingId, false));
            return false;
          }
        }
        else
          m_ignoreSettingChanging.erase(make_pair(settingId, false));
      }

      if (settingId == "videoscreen.screen")
        m_ignoreSettingChanging.insert(make_pair("videoscreen.resolution", true));
    }
    else
      m_ignoreSettingChanging.erase(make_pair(settingId, true));
  }

  return true;
}

bool CDisplaySettings::OnSettingUpdate(CSetting* &setting, const char *oldSettingId, const TiXmlNode *oldSettingNode)
{
  if (setting == NULL)
    return false;

  const std::string &settingId = setting->GetId();
  if (settingId == "videoscreen.screenmode")
  {
    CSettingString *screenmodeSetting = (CSettingString*)setting;
    std::string screenmode = screenmodeSetting->GetValue();
    // in Eden there was no character ("i" or "p") indicating interlaced/progressive
    // at the end so we just add a "p" and assume progressive
    if (screenmode.size() == 20)
      return screenmodeSetting->SetValue(screenmode + "p");
  }

  return false;
}

void CDisplaySettings::SetCurrentResolution(RESOLUTION resolution, bool save /* = false */)
{
  if (save)
  {
    string mode = GetStringFromResolution(resolution);
    CSettings::Get().SetString("videoscreen.screenmode", mode.c_str());
  }

  m_currentResolution = resolution;

  SetChanged();
}

RESOLUTION CDisplaySettings::GetDisplayResolution() const
{
  return GetResolutionFromString(CSettings::Get().GetString("videoscreen.screenmode"));
}

const RESOLUTION_INFO& CDisplaySettings::GetResolutionInfo(size_t index) const
{
  CSingleLock lock(m_critical);
  if (index >= m_resolutions.size())
    return EmptyResolution;

  return m_resolutions[index];
}

const RESOLUTION_INFO& CDisplaySettings::GetResolutionInfo(RESOLUTION resolution) const
{
  if (resolution <= RES_INVALID)
    return EmptyResolution;

  return GetResolutionInfo((size_t)resolution);
}

RESOLUTION_INFO& CDisplaySettings::GetResolutionInfo(size_t index)
{
  CSingleLock lock(m_critical);
  if (index >= m_resolutions.size())
  {
    EmptyModifiableResolution = RESOLUTION_INFO();
    return EmptyModifiableResolution;
  }

  return m_resolutions[index];
}

RESOLUTION_INFO& CDisplaySettings::GetResolutionInfo(RESOLUTION resolution)
{
  if (resolution <= RES_INVALID)
  {
    EmptyModifiableResolution = RESOLUTION_INFO();
    return EmptyModifiableResolution;
  }

  return GetResolutionInfo((size_t)resolution);
}

void CDisplaySettings::AddResolutionInfo(const RESOLUTION_INFO &resolution)
{
  CSingleLock lock(m_critical);
  m_resolutions.push_back(resolution);
}

void CDisplaySettings::ApplyCalibrations()
{
  CSingleLock lock(m_critical);
  // apply all calibrations to the resolutions
  for (ResolutionInfos::const_iterator itCal = m_calibrations.begin(); itCal != m_calibrations.end(); ++itCal)
  {
    // find resolutions
    for (size_t res = 0; res < m_resolutions.size(); ++res)
    {
      if (res == RES_WINDOW)
        continue;
      if (itCal->strMode.Equals(m_resolutions[res].strMode))
      {
        // overscan
        m_resolutions[res].Overscan.left = itCal->Overscan.left;
        if (m_resolutions[res].Overscan.left < -m_resolutions[res].iWidth/4)
          m_resolutions[res].Overscan.left = -m_resolutions[res].iWidth/4;
        if (m_resolutions[res].Overscan.left > m_resolutions[res].iWidth/4)
          m_resolutions[res].Overscan.left = m_resolutions[res].iWidth/4;

        m_resolutions[res].Overscan.top = itCal->Overscan.top;
        if (m_resolutions[res].Overscan.top < -m_resolutions[res].iHeight/4)
          m_resolutions[res].Overscan.top = -m_resolutions[res].iHeight/4;
        if (m_resolutions[res].Overscan.top > m_resolutions[res].iHeight/4)
          m_resolutions[res].Overscan.top = m_resolutions[res].iHeight/4;

        m_resolutions[res].Overscan.right = itCal->Overscan.right;
        if (m_resolutions[res].Overscan.right < m_resolutions[res].iWidth / 2)
          m_resolutions[res].Overscan.right = m_resolutions[res].iWidth / 2;
        if (m_resolutions[res].Overscan.right > m_resolutions[res].iWidth * 3/2)
          m_resolutions[res].Overscan.right = m_resolutions[res].iWidth *3/2;

        m_resolutions[res].Overscan.bottom = itCal->Overscan.bottom;
        if (m_resolutions[res].Overscan.bottom < m_resolutions[res].iHeight / 2)
          m_resolutions[res].Overscan.bottom = m_resolutions[res].iHeight / 2;
        if (m_resolutions[res].Overscan.bottom > m_resolutions[res].iHeight * 3/2)
          m_resolutions[res].Overscan.bottom = m_resolutions[res].iHeight * 3/2;

        m_resolutions[res].iSubtitles = itCal->iSubtitles;
        if (m_resolutions[res].iSubtitles < m_resolutions[res].iHeight / 2)
          m_resolutions[res].iSubtitles = m_resolutions[res].iHeight / 2;
        if (m_resolutions[res].iSubtitles > m_resolutions[res].iHeight* 5/4)
          m_resolutions[res].iSubtitles = m_resolutions[res].iHeight* 5/4;

        m_resolutions[res].fPixelRatio = itCal->fPixelRatio;
        if (m_resolutions[res].fPixelRatio < 0.5f)
          m_resolutions[res].fPixelRatio = 0.5f;
        if (m_resolutions[res].fPixelRatio > 2.0f)
          m_resolutions[res].fPixelRatio = 2.0f;
        break;
      }
    }
  }
}

void CDisplaySettings::UpdateCalibrations()
{
  CSingleLock lock(m_critical);
  for (size_t res = RES_DESKTOP; res < m_resolutions.size(); ++res)
  {
    // find calibration
    bool found = false;
    for (ResolutionInfos::iterator itCal = m_calibrations.begin(); itCal != m_calibrations.end(); ++itCal)
    {
      if (itCal->strMode.Equals(m_resolutions[res].strMode))
      {
        // TODO: erase calibrations with default values
        *itCal = m_resolutions[res];
        found = true;
        break;
      }
    }

    if (!found)
      m_calibrations.push_back(m_resolutions[res]);
  }
}

DisplayMode CDisplaySettings::GetCurrentDisplayMode() const
{
  if (GetCurrentResolution() == RES_WINDOW)
    return DM_WINDOWED;

  return GetCurrentResolutionInfo().iScreen;
}

RESOLUTION CDisplaySettings::GetResolutionFromString(const std::string &strResolution)
{
  if (strResolution == "DESKTOP")
    return RES_DESKTOP;
  else if (strResolution == "WINDOW")
    return RES_WINDOW;
  else if (strResolution.size() == 21)
  {
    // format: SWWWWWHHHHHRRR.RRRRRP, where S = screen, W = width, H = height, R = refresh, P = interlace
    int screen = strtol(StringUtils::Mid(strResolution, 0,1).c_str(), NULL, 10);
    int width = strtol(StringUtils::Mid(strResolution, 1,5).c_str(), NULL, 10);
    int height = strtol(StringUtils::Mid(strResolution, 6,5).c_str(), NULL, 10);
    float refresh = (float)strtod(StringUtils::Mid(strResolution, 11,9).c_str(), NULL);
    // look for 'i' and treat everything else as progressive,
    // and use 100/200 to get a nice square_error.
    int interlaced = (StringUtils::Right(strResolution, 1) == "i") ? 100 : 200;

    // find the closest match to these in our res vector.  If we have the screen, we score the res
    RESOLUTION bestRes = RES_DESKTOP;
    float bestScore = FLT_MAX;
    
    for (size_t resolution = 0; resolution < CDisplaySettings::Get().ResolutionInfoSize(); resolution++)
    {
      const RESOLUTION_INFO &info = CDisplaySettings::Get().GetResolutionInfo(resolution);
      if (info.iScreen != screen)
        continue;
      float score = 10 * (square_error((float)info.iScreenWidth, (float)width) +
                    square_error((float)info.iScreenHeight, (float)height) +
                    square_error(info.fRefreshRate, refresh) +
                    square_error((float)((info.dwFlags & D3DPRESENTFLAG_INTERLACED) ? 100 : 200), (float)interlaced));
      if (score < bestScore)
      {
        bestScore = score;
        bestRes = (RESOLUTION)resolution;
      }
    }
    return bestRes;
  }

  return RES_DESKTOP;
}

std::string CDisplaySettings::GetStringFromResolution(RESOLUTION resolution, float refreshrate /* = 0.0f */)
{
  if (resolution == RES_WINDOW)
    return "WINDOW";

  if (resolution >= RES_CUSTOM && resolution < (RESOLUTION)CDisplaySettings::Get().ResolutionInfoSize())
  {
    const RESOLUTION_INFO &info = CDisplaySettings::Get().GetResolutionInfo(resolution);
    return StringUtils::Format("%1i%05i%05i%09.5f%s", info.iScreen,
                               info.iScreenWidth, info.iScreenHeight,
                               refreshrate > 0.0f ? refreshrate : info.fRefreshRate,
                               (info.dwFlags & D3DPRESENTFLAG_INTERLACED) ? "i":"p");
  }

  return "DESKTOP";
}

RESOLUTION CDisplaySettings::GetResolutionForScreen()
{
  DisplayMode mode = CSettings::Get().GetInt("videoscreen.screen");
  if (mode == DM_WINDOWED)
    return RES_WINDOW;

  for (int idx=0; idx < g_Windowing.GetNumScreens(); idx++)
  {
    if (CDisplaySettings::Get().GetResolutionInfo(RES_DESKTOP + idx).iScreen == mode)
      return (RESOLUTION)(RES_DESKTOP + idx);
  }

  return RES_DESKTOP;
}

void CDisplaySettings::SettingOptionsRefreshChangeDelaysFiller(const CSetting *setting, std::vector< std::pair<std::string, int> > &list, int &current)
{
  list.push_back(make_pair(g_localizeStrings.Get(13551), 0));
          
  for (int i = 1; i <= MAX_REFRESH_CHANGE_DELAY; i++)
    list.push_back(make_pair(StringUtils::Format(g_localizeStrings.Get(13553).c_str(), (double)i / 10.0), i));
}

void CDisplaySettings::SettingOptionsRefreshRatesFiller(const CSetting *setting, std::vector< std::pair<std::string, std::string> > &list, std::string &current)
{
  // get the proper resolution
  RESOLUTION res = GetResolutionForScreen();
  if (res < RES_WINDOW)
    return;

  // only add "Windowed" if in windowed mode
  if (res == RES_WINDOW)
  {
    current = "WINDOW";
    list.push_back(make_pair(g_localizeStrings.Get(242), current));
    return;
  }

  // The only meaningful parts of res here are iScreen, iScreenWidth, iScreenHeight
  vector<REFRESHRATE> refreshrates = g_Windowing.RefreshRates(CDisplaySettings::Get().GetResolutionInfo(res).iScreen,
                                                              CDisplaySettings::Get().GetResolutionInfo(res).iScreenWidth,
                                                              CDisplaySettings::Get().GetResolutionInfo(res).iScreenHeight,
                                                              CDisplaySettings::Get().GetResolutionInfo(res).dwFlags);

  bool match = false;
  for (vector<REFRESHRATE>::const_iterator refreshrate = refreshrates.begin(); refreshrate != refreshrates.end(); refreshrate++)
  {
    std::string screenmode = GetStringFromResolution(res, refreshrate->RefreshRate);
    if (!match && StringUtils::EqualsNoCase(((CSettingString*)setting)->GetValue(), screenmode))
      match = true;
    list.push_back(make_pair(StringUtils::Format("%.02f", refreshrate->RefreshRate), screenmode));
  }

  if (!match)
    current = GetStringFromResolution(res, g_Windowing.DefaultRefreshRate(CDisplaySettings::Get().GetResolutionInfo(res).iScreen, refreshrates).RefreshRate);
}

void CDisplaySettings::SettingOptionsResolutionsFiller(const CSetting *setting, std::vector< std::pair<std::string, int> > &list, int &current)
{
  RESOLUTION res = RES_INVALID;
  DisplayMode screen = CSettings::Get().GetInt("videoscreen.screen");
  if (screen == DM_WINDOWED)
  {
    res = RES_WINDOW;
    list.push_back(make_pair(g_localizeStrings.Get(242), RES_WINDOW));
  }
  else
  {
    vector<RESOLUTION_WHR> resolutions = g_Windowing.ScreenResolutions(screen);
    for (vector<RESOLUTION_WHR>::const_iterator resolution = resolutions.begin(); resolution != resolutions.end(); resolution++)
    {
      list.push_back(make_pair(
        StringUtils::Format("%dx%d%s", resolution->width, resolution->height, (resolution->interlaced == D3DPRESENTFLAG_INTERLACED) ? "i" : "p"),
        resolution->ResInfo_Index));

      RESOLUTION_INFO res1 = CDisplaySettings::Get().GetCurrentResolutionInfo();
      RESOLUTION_INFO res2 = CDisplaySettings::Get().GetResolutionInfo(resolution->ResInfo_Index);
      if (res1.iScreen == res2.iScreen &&
          res1.iScreenWidth  == res2.iScreenWidth &&
          res1.iScreenHeight == res2.iScreenHeight &&
          (res1.dwFlags & D3DPRESENTFLAG_INTERLACED) == (res2.dwFlags & D3DPRESENTFLAG_INTERLACED))
        res = (RESOLUTION)resolution->ResInfo_Index;
    }
  }

  if (res != RES_INVALID)
    current = res;
}

void CDisplaySettings::SettingOptionsScreensFiller(const CSetting *setting, std::vector< std::pair<std::string, int> > &list, int &current)
{
  if (g_advancedSettings.m_canWindowed)
    list.push_back(make_pair(g_localizeStrings.Get(242), DM_WINDOWED));

  for (int idx = 0; idx < g_Windowing.GetNumScreens(); idx++)
  {
    int screen = CDisplaySettings::Get().GetResolutionInfo(RES_DESKTOP + idx).iScreen;
    list.push_back(make_pair(StringUtils::Format(g_localizeStrings.Get(241), screen + 1), screen));
  }

  RESOLUTION res = CDisplaySettings::Get().GetDisplayResolution();

  if (res == RES_WINDOW)
    current = DM_WINDOWED;
  else
  {
    RESOLUTION_INFO resInfo = CDisplaySettings::Get().GetResolutionInfo(res);
    current = resInfo.iScreen;
  }
}

void CDisplaySettings::SettingOptionsVerticalSyncsFiller(const CSetting *setting, std::vector< std::pair<std::string, int> > &list, int &current)
{
#if defined(_LINUX) && !defined(TARGET_DARWIN)
  list.push_back(make_pair(g_localizeStrings.Get(13101), VSYNC_DRIVER));
#endif
  list.push_back(make_pair(g_localizeStrings.Get(13106), VSYNC_DISABLED));
  list.push_back(make_pair(g_localizeStrings.Get(13107), VSYNC_VIDEO));
  list.push_back(make_pair(g_localizeStrings.Get(13108), VSYNC_ALWAYS));
}

void CDisplaySettings::SettingOptionsStereoscopicModesFiller(const CSetting *setting, std::vector< std::pair<std::string, int> > &list, int &current)
{
  list.push_back(make_pair(g_localizeStrings.Get(36502),RENDER_STEREO_MODE_OFF));
  list.push_back(make_pair(g_localizeStrings.Get(36503),RENDER_STEREO_MODE_SPLIT_HORIZONTAL));
  list.push_back(make_pair(g_localizeStrings.Get(36504),RENDER_STEREO_MODE_SPLIT_VERTICAL));
  list.push_back(make_pair(g_localizeStrings.Get(36505),RENDER_STEREO_MODE_ANAGLYPH_RED_CYAN));
  list.push_back(make_pair(g_localizeStrings.Get(36506),RENDER_STEREO_MODE_ANAGLYPH_GREEN_MAGENTA));
  list.push_back(make_pair(g_localizeStrings.Get(36507),RENDER_STEREO_MODE_INTERLACED));
  list.push_back(make_pair(g_localizeStrings.Get(36508),RENDER_STEREO_MODE_HARDWAREBASED));

  // Remove any mode not supported by render system
  for(std::vector< std::pair<std::string, int> >::iterator it = list.begin(); it != list.end();)
  {
    if(!g_Windowing.SupportsStereo((RENDER_STEREO_MODE)it->second))
      it = list.erase(it);
    else
      ++it;
  }
}
