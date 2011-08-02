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

#include "GUIExternalAppControl.h"
#include "windowing/WindowingFactory.h"
#include "utils/log.h"
#include "utils/MathUtils.h"
#include "Key.h"
#include "utils/CharsetConverter.h"
#include "settings/Settings.h"
#include <GL/glew.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <GL/gl.h>
#include <GL/glx.h>
#include <X11/extensions/Xcomposite.h>

static int XErrorHandlerIgnore( Display *dpy, XErrorEvent *e )
{
    char error_desc[1024];
    XGetErrorText(dpy,e->error_code,error_desc,sizeof(error_desc));
    CLog::Log(LOGERROR, "CGUIExternalAppControl - X Error: %s\n", error_desc);
    return 0;
}

CGUIExternalAppControl::CGUIExternalAppControl(int parentID, int controlID, float posX, float posY, float width, float height)
 : CGUIControl(parentID, controlID, posX, posY, width, height)
 , m_window(None)
 , m_active(None)
 , m_pixmap_gl(None)
 , m_pixmap(None)
 , m_texture(0)
 , m_button_state(0)
{
  m_glXBindTexImageEXT    = (PFNGLXBINDTEXIMAGEEXTPROC)glXGetProcAddress((GLubyte *) "glXBindTexImageEXT");
  m_glXReleaseTexImageEXT = (PFNGLXRELEASETEXIMAGEEXTPROC)glXGetProcAddress((GLubyte *) "glXReleaseTexImageEXT");

  if(m_glXBindTexImageEXT    == NULL
  || m_glXReleaseTexImageEXT == NULL)
    CLog::Log(LOGERROR, "CGUIExternalAppControl::CGUIExternalAppControl - failed to get glXBindTexImageEXT");

  memset(&m_attrib, 0, sizeof(m_attrib));
  glGenTextures(1, &m_texture);

  m_display = XOpenDisplay(XDisplayString(g_Windowing.GetDisplay()));
  m_screen  = XDefaultScreen(m_display);
  m_root    = XDefaultRootWindow(m_display);
}

CGUIExternalAppControl::~CGUIExternalAppControl()
{
  Dispose();
  glDeleteTextures(1, &m_texture);
  XCloseDisplay(m_display);
}

void CGUIExternalAppControl::Dispose()
{

  if(m_pixmap_gl)
  {
    glXDestroyPixmap(m_display, m_pixmap_gl);
    m_pixmap_gl = None;
  }

  if(m_pixmap)
  {
    XFreePixmap(m_display, m_pixmap);
    m_pixmap = None;
  }

  if(m_texture)
  {
    glDeleteTextures(1, &m_texture);
    m_texture = 0;
  }
}

bool CGUIExternalAppControl::SetWindow(const CStdString& window_class, Window parent)
{
  Window root, parent2, *children;

  if(parent == None)
    parent = m_root;

  XClassHint hint;
  if(XGetClassHint(m_display, parent, &hint))
  {
    CStdString cls(hint.res_name);
    XFree(hint.res_class);
    XFree(hint.res_name);

    if(cls == window_class && SetWindow(parent))
      return true;
  }

  unsigned int count;
  if(XQueryTree(m_display, parent, &root, &parent2, &children, &count) == 0)
    return false;

  /* look through sub children */
  for(unsigned i = 0; i < count; ++i)
  {
    if(SetWindow(window_class, children[i]))
    {
      XFree(children);
      return true;
    }
  }
  XFree(children);
  return false;
}

bool CGUIExternalAppControl::SetWindow(Window window)
{
  Dispose();

  if(m_glXBindTexImageEXT    == NULL
  || m_glXReleaseTexImageEXT == NULL)
    return false;

  float top = 0.0f, bottom = 0.0f;

  XVisualInfo* visinfo;

  XGetWindowAttributes (m_display, window, &m_attrib);

  if(m_attrib.map_state != IsViewable)
    return false;

  VisualID visualid = XVisualIDFromVisual (m_attrib.visual);

  int nfbconfigs, i;
  GLXFBConfig* fbconfigs = glXGetFBConfigs (m_display, m_screen, &nfbconfigs);
  for (i = 0; i < nfbconfigs; i++)
  {
    int value;
    visinfo = glXGetVisualFromFBConfig (m_display, fbconfigs[i]);
    if (!visinfo || visinfo->visualid != visualid)
      continue;

    glXGetFBConfigAttrib (m_display, fbconfigs[i], GLX_DRAWABLE_TYPE, &value);
    if (!(value & GLX_PIXMAP_BIT))
      continue;

    glXGetFBConfigAttrib (m_display, fbconfigs[i],
        GLX_BIND_TO_TEXTURE_TARGETS_EXT,
        &value);
    if (!(value & GLX_TEXTURE_2D_BIT_EXT))
      continue;

    glXGetFBConfigAttrib (m_display, fbconfigs[i],
        GLX_BIND_TO_TEXTURE_RGBA_EXT,
        &value);
    if (value)
      m_texture_format = GLX_TEXTURE_FORMAT_RGBA_EXT;
    else
    {
      glXGetFBConfigAttrib (m_display, fbconfigs[i],
          GLX_BIND_TO_TEXTURE_RGB_EXT,
          &value);
      if (value == 0)
        continue;
      m_texture_format = GLX_TEXTURE_FORMAT_RGB_EXT;
    }


    glXGetFBConfigAttrib (m_display, fbconfigs[i],
        GLX_Y_INVERTED_EXT,
        &value);
    if (value)
    {
      top    = 0.0f;
      bottom = 1.0f;
    }
    else
    {
      top    = 1.0f;
      bottom = 0.0f;
    }

    break;
  }

  if (i == nfbconfigs)
    return false;

  m_config = fbconfigs[i];
  m_window = window;

  m_vertex[0].u = 0.0f;
  m_vertex[0].v = top;
  m_vertex[0].z = 0.0f;
  m_vertex[1].u = 1.0f;
  m_vertex[1].v = top;
  m_vertex[1].z = 0.0f;
  m_vertex[2].u = 1.0f;
  m_vertex[2].v = bottom;
  m_vertex[2].z = 0.0f;
  m_vertex[3].u = 0.0f;
  m_vertex[3].v = bottom;
  m_vertex[3].z = 0.0f;

  XCompositeRedirectWindow(m_display, window, CompositeRedirectAutomatic);
  XCompositeRedirectSubwindows(m_display, window, CompositeRedirectAutomatic);
  XSelectInput(m_display, window, LeaveWindowMask | EnterWindowMask | StructureNotifyMask);
  return true;
}

void CGUIExternalAppControl::Resize()
{
  int width  = MathUtils::round_int(m_width);
  int height = MathUtils::round_int(m_height);
  XResizeWindow(m_display, m_window, width, height);
}

bool CGUIExternalAppControl::PreparePixmaps()
{
  if (m_pixmap == None)
  {
    m_pixmap = XCompositeNameWindowPixmap (m_display, m_window);

    if (m_pixmap == None)
      return false;
  }
  if(m_pixmap_gl == None)
  {
    int pixmapAttribs[] = { GLX_TEXTURE_TARGET_EXT, GLX_TEXTURE_2D_EXT,
                            GLX_TEXTURE_FORMAT_EXT, m_texture_format,
                            None };
    m_pixmap_gl = glXCreatePixmap (m_display, m_config, m_pixmap, pixmapAttribs);
    if(m_pixmap_gl == None)
      return false;
  }
  return true;
}

bool CGUIExternalAppControl::ProcessEvents()
{
  XEvent event;
  bool dirty = false;
  while(XCheckWindowEvent(m_display, m_window, LeaveWindowMask | EnterWindowMask | StructureNotifyMask, &event))
  {
    if(event.type == LeaveNotify)
    {
      if(event.xcrossing.detail == NotifyNonlinear
      || event.xcrossing.detail == NotifyNonlinearVirtual
      || event.xcrossing.detail == NotifyVirtual)
        m_active = None;
    }
    else if(event.type == ConfigureNotify
         || event.type == MapNotify
         || event.type == UnmapNotify)
    {
      Dispose();
      dirty = true;
    }
    else if(event.type == DestroyNotify)
    {
      Dispose();
      m_window = None;
      return false;
    }
  }
  if(dirty){
      XErrorHandler handler = XSetErrorHandler(XErrorHandlerIgnore);
      XGetWindowAttributes(m_display, m_window, &m_attrib);
      XSetErrorHandler(handler); /* restore old handler */
  }
  return true;
}

void CGUIExternalAppControl::Process(unsigned int currentTime, CDirtyRegionList &dirtyregions)
{
  if(m_window == None)
    return;

  if(!ProcessEvents())
    return;

  if(m_attrib.map_state != IsViewable)
    return;

  if(!PreparePixmaps())
    return;

  float scale_x = m_width  / m_attrib.width;
  float scale_y = m_height / m_attrib.height;

  float w, h, x, y;
  if(scale_x < scale_y)
  {
    w = m_width;
    h = m_attrib.height * scale_x;
  }
  else
  {
    h = m_height;
    w = m_attrib.width  * scale_y;
  }

  x = m_posX + m_width  * 0.5 - w * 0.5;
  y = m_posY + m_height * 0.5 - h * 0.5;

  m_rect.SetRect(x, y, x + w, y + h);

  m_vertex[0].x = x;
  m_vertex[0].y = y;

  m_vertex[1].x = x + w;
  m_vertex[1].y = y;

  m_vertex[2].x = x + w;
  m_vertex[2].y = y + h;

  m_vertex[3].x = x;
  m_vertex[3].y = y + h;

  for(unsigned i = 0; i < 4; ++i)
    g_graphicsContext.ScaleFinalCoords(m_vertex[i].x, m_vertex[i].y, m_vertex[i].z);

  MarkDirtyRegion();
  CGUIControl::Process(currentTime, dirtyregions);
}

void CGUIExternalAppControl::Render()
{
  if(m_pixmap_gl == None)
    return;

  glEnable(GL_TEXTURE_2D);
  glBindTexture (GL_TEXTURE_2D, m_texture);

  m_glXBindTexImageEXT (m_display, m_pixmap_gl, GLX_FRONT_LEFT_EXT, NULL);

  glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE);
  glTexEnvf(GL_TEXTURE_ENV, GL_COMBINE_RGB , GL_MODULATE);
  glTexEnvf(GL_TEXTURE_ENV, GL_SOURCE0_RGB , GL_TEXTURE0);
  glTexEnvf(GL_TEXTURE_ENV, GL_OPERAND0_RGB, GL_SRC_COLOR);
  glTexEnvf(GL_TEXTURE_ENV, GL_SOURCE1_RGB , GL_PRIMARY_COLOR);
  glTexEnvf(GL_TEXTURE_ENV, GL_OPERAND1_RGB, GL_SRC_COLOR);

  glTexEnvf(GL_TEXTURE_ENV, GL_COMBINE_ALPHA, GL_REPLACE);
  glTexEnvf(GL_TEXTURE_ENV, GL_OPERAND0_RGB , GL_SRC_COLOR);
  glTexEnvf(GL_TEXTURE_ENV, GL_SOURCE0_ALPHA, GL_PRIMARY_COLOR);

  glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
  glEnable(GL_BLEND);

  glBegin (GL_QUADS);
  glColor4ub(0xff, 0xff, 0xff, 0xff);

  for(unsigned i = 0; i < 4; ++i)
  {
    glTexCoord2f (m_vertex[i].u, m_vertex[i].v);
    glVertex3f   (m_vertex[i].x, m_vertex[i].y, m_vertex[i].z);
  }

  glEnd ();

  m_glXReleaseTexImageEXT (m_display, m_pixmap_gl, GLX_FRONT_LEFT_EXT);
  glDisable(GL_TEXTURE_2D);
}

void CGUIExternalAppControl::SendKeyPress(KeySym sym)
{
  XKeyEvent event = {0};
  event.display     = m_display;
  event.window      = m_window;
  event.root        = m_root;
  event.subwindow   = None;
  event.x           = 1;
  event.y           = 1;
  event.x_root      = 1;
  event.y_root      = 1;
  event.same_screen = True;
  event.keycode     = XKeysymToKeycode(m_display, sym);

  event.time        = CurrentTime;
  event.state       = 0;
  event.type        = KeyPress;
  XSendEvent(m_display, m_window, True, KeyPressMask, (XEvent *)&event);
  event.state      |= KeyPressMask;
  event.type        = KeyRelease;
  XSendEvent(m_display, m_window, True, KeyReleaseMask, (XEvent *)&event);
}

bool CGUIExternalAppControl::OnAction(const CAction &action)
{
  if(action.GetID() >= KEY_ASCII && action.GetUnicode())
  {
    CStdStringW wide;
    CStdString  utf8;
    wide.Format(L"%c", action.GetUnicode());
    g_charsetConverter.wToUTF8(wide, utf8);
    SendKeyPress(XStringToKeysym(utf8.c_str()));
  }
  else
  {
    struct SActionToSym {
      int          action;
      KeySym       sym;
    } actions[] = {
        {ACTION_MOVE_UP   , XK_Up},
        {ACTION_MOVE_DOWN , XK_Down},
        {ACTION_MOVE_LEFT , XK_Left},
        {ACTION_MOVE_RIGHT, XK_Right},
        {ACTION_PAGE_UP   , XK_Page_Up},
        {ACTION_PAGE_DOWN , XK_Page_Down},
        {ACTION_NONE      , 0},
    };

    for(SActionToSym* it = actions; it->action != ACTION_NONE; ++it)
    {
      if(it->action == action.GetID())
      {
        SendKeyPress(it->sym);
        return true;
      }
    }

  }
  return CGUIControl::OnAction(action);
}

Window CGUIExternalAppControl::FindSubWindow(int x, int y)
{
  Window src, dst, child;
  dst   = m_window;
  src   = m_window;
  while(dst != None)
  {
    if(!XTranslateCoordinates(m_display, src, dst  , x, y, &x, &y, &child))
      return None;
    src = dst;
    dst = child;
  }
  return src;
}

bool CGUIExternalAppControl::IsParent(Window parent, Window child)
{
  while(child != None)
  {
    Window root, *children;
    unsigned int count;
    if(XQueryTree(m_display, child, &root, &child, &children, &count) == 0)
      return false;
    XFree(children);
    if(parent == child)
      return true;
  }
  return false;
}


void CGUIExternalAppControl::SendCrossingEvent(int x, int y, Window window, int type, int detail)
{
  XCrossingEvent ev = {0};
  ev.display       = m_display;
  ev.root          = m_root;
  ev.window        = window;
  XTranslateCoordinates(m_display, m_window, ev.root  , x, y, &ev.x_root, &ev.y_root, &ev.subwindow);
  XTranslateCoordinates(m_display, m_window, ev.window, x, y, &ev.x     , &ev.     y, &ev.subwindow);
  ev.mode          = NotifyNormal;
  ev.same_screen   = True;
  ev.focus         = True;
  ev.state         = m_button_state;
  ev.time          = CurrentTime;
  ev.detail        = detail;
  ev.type          = type;
  if(type == EnterNotify)
    XSendEvent(m_display,window,True,EnterWindowMask,(XEvent*)&ev);
  else
    XSendEvent(m_display,window,True,LeaveWindowMask,(XEvent*)&ev);
}

bool CGUIExternalAppControl::GetCoordinates(int& x, int& y, const CPoint &point)
{
  if(!m_rect.PtInRect(point))
    return false;

  x = MathUtils::round_int((point.x - m_rect.x1) / m_rect.Width()  * m_attrib.width);
  y = MathUtils::round_int((point.y - m_rect.y1) / m_rect.Height() * m_attrib.height);
  return true;
}

bool CGUIExternalAppControl::OnMouseOver(const CPoint &point)
{
  if(m_window == None)
    return true;

  int x, y;
  if(!GetCoordinates(x, y, point))
    return true;

  Window current = FindSubWindow(x, y);

  if(m_active == None)
  {
    SendCrossingEvent(x, y, m_window, EnterNotify, NotifyNonlinear);
    m_active = m_window;
  }

  if(m_active != current && (m_button_state && Button1Mask) == 0)
  {

    if(IsParent(current, m_active))
    {
      SendCrossingEvent(x, y, m_active    , LeaveNotify, NotifyAncestor);
      SendCrossingEvent(x, y, current     , EnterNotify, NotifyInferior);
    }
    else if(IsParent(m_active, current))
    {
      SendCrossingEvent(x, y, m_active    , LeaveNotify, NotifyInferior);
      SendCrossingEvent(x, y, current     , EnterNotify, NotifyAncestor);
    }
    else
    {
      SendCrossingEvent(x, y, m_active    , LeaveNotify, NotifyNonlinear);
      SendCrossingEvent(x, y, current     , EnterNotify, NotifyNonlinear);
    }
    m_active = current;
  }

  XMotionEvent event = {0};
  event.display       = m_display;
  event.window        = m_active;
  event.root          = m_root;
  event.subwindow     = None;
  event.time          = CurrentTime;
  event.same_screen   = True;
  event.is_hint       = 0;
  event.state         = m_button_state;
  event.type          = MotionNotify;

  XTranslateCoordinates(m_display, m_window, event.root  , x, y, &event.x_root, &event.y_root, &current);
  XTranslateCoordinates(m_display, m_window, event.window, x, y, &event.x     , &event.y     , &current);
  XSendEvent(m_display,event.window,True,PointerMotionMask,(XEvent*)&event);

  return CGUIControl::OnMouseOver(point);
}

void CGUIExternalAppControl::SendButtonEvent(int x, int y, unsigned int type, unsigned int button)
{
  XButtonEvent event = {0};
  event.display     = m_display;
  event.window      = m_active;
  event.root        = m_root;
  event.time        = CurrentTime;
  event.same_screen = True;
  event.send_event  = True;
  event.state       = m_button_state;
  event.button      = button;
  event.type        = type;

  unsigned int state;
  switch(button)
  {
    case Button1: state = Button1Mask; break;
    case Button2: state = Button2Mask; break;
    case Button3: state = Button3Mask; break;
    case Button4: state = Button4Mask; break;
    case Button5: state = Button5Mask; break;
    default:      state = 0;           break;
  }

  unsigned int mask;
  if(type == ButtonPress)
  {
    mask = ButtonPressMask;
    m_button_state |= state;
  }
  else
  {
    mask = ButtonReleaseMask;
    m_button_state &= ~state;
  }

  Window child;
  XTranslateCoordinates(m_display, m_window, event.root  , x, y, &event.x_root, &event.y_root, &child);
  XTranslateCoordinates(m_display, m_window, event.window, x, y, &event.x     , &event.y     , &child);
  XSendEvent(m_display, event.window, True, mask, (XEvent *)&event);
}

EVENT_RESULT CGUIExternalAppControl::OnMouseEvent(const CPoint &point, const CMouseEvent &event)
{
  if(m_window == None)
    return EVENT_RESULT_UNHANDLED;

  int x, y;
  if(!GetCoordinates(x, y, point))
    return EVENT_RESULT_UNHANDLED;

  if (event.m_id == ACTION_MOUSE_LEFT_CLICK)
  {
    SendButtonEvent(x, y, ButtonPress  , Button1);
    SendButtonEvent(x, y, ButtonRelease, Button1);
    return EVENT_RESULT_HANDLED;
  }
  else if(event.m_id == ACTION_MOUSE_DRAG)
  {
    if(event.m_state == 1)
    { // grab exclusive access
      CGUIMessage msg(GUI_MSG_EXCLUSIVE_MOUSE, GetID(), GetParentID());
      SendWindowMessage(msg);
#if(0)
      XGrabButton(m_display, AnyButton, AnyModifier
                 , m_window, True
                 , ButtonPressMask | ButtonReleaseMask | EnterWindowMask | LeaveWindowMask | PointerMotionMask
                 , GrabModeAsync
                 , GrabModeAsync
                 , None
                 , None);
#endif
      SendButtonEvent(x, y, ButtonPress  , Button1);
    }
    else if(event.m_state == 3)
    { // release exclusive access
      CGUIMessage msg(GUI_MSG_EXCLUSIVE_MOUSE, 0, GetParentID());
      SendWindowMessage(msg);
      SendButtonEvent(x, y, ButtonRelease, Button1);
    }
  }
  return EVENT_RESULT_UNHANDLED;
}

bool CGUIExternalAppControl::OnMessage(CGUIMessage& message)
{
  return CGUIControl::OnMessage(message);
}
