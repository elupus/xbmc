/*
 *      Copyright (C) 2005-2013 Team XBMC
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

/*!
 * @file StereoscopicsManager.cpp
 * @brief This class acts as container for stereoscopic related functions
 */

#pragma once

#include <stdlib.h>
#include "StereoscopicsManager.h"

#include "dialogs/GUIDialogKaiToast.h"
#include "guilib/LocalizeStrings.h"
#include "guilib/GUIWindowManager.h"
#include "settings/ISettingCallback.h"
#include "settings/Setting.h"
#include "settings/Settings.h"
#include "rendering/RenderSystem.h"
#include "utils/log.h"
#include "windowing/WindowingFactory.h"


CStereoscopicsManager::CStereoscopicsManager(void)
{
  m_lastStereoMode = RENDER_STEREO_MODE_OFF;
}

CStereoscopicsManager::~CStereoscopicsManager(void)
{
}

CStereoscopicsManager& CStereoscopicsManager::Get(void)
{
  static CStereoscopicsManager sStereoscopicsManager;
  return sStereoscopicsManager;
}

bool CStereoscopicsManager::HasStereoscopicSupport(void)
{
  return (bool) CSettings::Get().GetBool("videoscreen.hasstereoscopicsupport");
}

RENDER_STEREO_MODE CStereoscopicsManager::GetStereoMode(void)
{
  return (RENDER_STEREO_MODE) CSettings::Get().GetInt("videoscreen.stereoscopicmode");
}

void CStereoscopicsManager::SetStereoMode(const RENDER_STEREO_MODE &mode)
{
  RENDER_STEREO_MODE currentMode = GetStereoMode();
  if (mode != currentMode)
    CSettings::Get().SetInt("videoscreen.stereoscopicmode", mode);
}

CStdString CStereoscopicsManager::GetLabelForStereoMode(const RENDER_STEREO_MODE &mode)
{
  return g_localizeStrings.Get(36502 + mode);
}
void CStereoscopicsManager::OnSettingChanged(const CSetting *setting)
{
  if (setting == NULL)
    return;

  const std::string &settingId = setting->GetId();

  if (settingId == "videoscreen.hasstereoscopicsupport")
  {
    // turn off 3D mode if global toggle has been disabled
    if (((CSettingBool*)setting)->GetValue() == false)
    {
      SetStereoMode(RENDER_STEREO_MODE_OFF);
      ApplyStereoMode(RENDER_STEREO_MODE_OFF);
    }
    else
    {
      ApplyStereoMode(GetStereoMode());
    }
  }
  else if (settingId == "videoscreen.stereoscopicmode")
  {
    RENDER_STEREO_MODE mode = GetStereoMode();
    CLog::Log(LOGDEBUG, "StereoscopicsManager: stereo mode setting changed to %s", GetLabelForStereoMode(mode).c_str());
    if(HasStereoscopicSupport())
      ApplyStereoMode(mode);
  }
}

void CStereoscopicsManager::ApplyStereoMode(const RENDER_STEREO_MODE &mode, bool notify)
{
  RENDER_STEREO_MODE currentMode = g_graphicsContext.GetStereoMode();
  CLog::Log(LOGDEBUG, "StereoscopicsManager::ApplyStereoMode: trying to apply stereo mode. Current: %s | Target: %s", GetLabelForStereoMode(currentMode).c_str(), GetLabelForStereoMode(mode).c_str());
  if (currentMode != mode)
  {
    g_graphicsContext.SetStereoMode(mode);
    CLog::Log(LOGDEBUG, "StereoscopicsManager: stereo mode changed to %s", GetLabelForStereoMode(mode).c_str());
    if (notify)
      CGUIDialogKaiToast::QueueNotification(CGUIDialogKaiToast::Info, g_localizeStrings.Get(36501), GetLabelForStereoMode(mode));
  }
}
