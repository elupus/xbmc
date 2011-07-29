/*
 * CGUIExternalAppControl.h
 *
 *  Created on: Jul 24, 2011
 *      Author: joakim
 */

#ifndef CGUIEXTERNALAPPCONTROL_H_
#define CGUIEXTERNALAPPCONTROL_H_

#include "GUIControl.h"
#include <GL/gl.h>
#include <GL/glx.h>

class CGUIExternalAppControl: public CGUIControl
{
  Display*   m_display;
  int        m_screen;

  PFNGLXBINDTEXIMAGEEXTPROC    m_glXBindTexImageEXT;
  PFNGLXRELEASETEXIMAGEEXTPROC m_glXReleaseTexImageEXT;

  Window            m_window;
  Window            m_active;
  GLXPixmap         m_pixmap_gl;
  Pixmap            m_pixmap;
  GLuint            m_texture;
  XWindowAttributes m_attrib;
  GLXFBConfig       m_config;
  CRect             m_rect;

  struct SVertex
  {
    float x, y, z;
    float u, v;
  } m_vertex[4];

public:
  CGUIExternalAppControl(int parentID, int controlID, float posX, float posY, float width, float height);
  virtual ~CGUIExternalAppControl();
  virtual void Render();

  virtual void Process  (unsigned int currentTime, CDirtyRegionList &dirtyregions);
  virtual bool OnAction (const CAction &action) ;
  virtual bool OnMessage(CGUIMessage& message);

  virtual EVENT_RESULT OnMouseEvent(const CPoint &point, const CMouseEvent &event);
  virtual bool         OnMouseOver(const CPoint &point);

  virtual CGUIExternalAppControl *Clone() const { return NULL; };

  bool IsParent(Window parent, Window child);

  void SendCrossingEvent(int x, int y, Window window, int type, int detail);

  bool FindSubWindow(int& x, int& y, Window& w, long mask);
  void FillXKeyEvent(XKeyEvent& event);
  void SendKeyPress(KeySym sym);
  void Resize();
  void Dispose();
  bool SetWindow(Window window);
};

#endif /* CGUIEXTERNALAPPCONTROL_H_ */
