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
  GLXPixmap         m_pixmap_gl;
  Pixmap            m_pixmap;
  GLuint            m_texture;
  XWindowAttributes m_attrib;
  GLXFBConfig       m_config;
  float             m_top;
  float             m_bottom;

  struct SVertex
  {
    float x, y, z;
    float u, v;
  } m_vertex[4];

public:
  CGUIExternalAppControl();
  virtual ~CGUIExternalAppControl();
  virtual void Render();

  virtual void Process  (unsigned int currentTime, CDirtyRegionList &dirtyregions);
  virtual bool OnAction (const CAction &action) ;
  virtual bool OnMessage(CGUIMessage& message);
  virtual EVENT_RESULT OnMouseEvent(const CPoint &point, const CMouseEvent &event);

  virtual CGUIExternalAppControl *Clone() const { return NULL; };

  void FillXKeyEvent(XKeyEvent& event);
  void SendKeyPress(KeySym sym);
  void Resize();
  void Dispose();
  bool SetWindow(Window window);
};

#endif /* CGUIEXTERNALAPPCONTROL_H_ */
