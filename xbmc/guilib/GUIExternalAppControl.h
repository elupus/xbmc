/*
 * CGUIExternalAppControl.h
 *
 *  Created on: Jul 24, 2011
 *      Author: joakim
 */

#ifndef CGUIEXTERNALAPPCONTROL_H_
#define CGUIEXTERNALAPPCONTROL_H_

#ifdef HAVE_LIBXCOMPOSITE

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
  Window            m_root;
  GLXPixmap         m_pixmap_gl;
  Pixmap            m_pixmap;
  GLuint            m_texture;
  unsigned int      m_texture_format;
  XWindowAttributes m_attrib;
  GLXFBConfig       m_config;
  CRect             m_rect;

  unsigned int      m_button_state;

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

  bool GetCoordinates(int& x, int& y, const CPoint& point);

  void SendButtonEvent  (int x, int y, unsigned int type, unsigned int button);
  void SendCrossingEvent(int x, int y, Window window, int type, int detail);
  void SendKeyPress(KeySym sym, unsigned int state);

  Window FindSubWindow(int x, int y);
  void Resize();
  void Dispose();
  bool SetWindow(Window window);
  bool SetWindow(const CStdString& window_class, Window parent = None);
  bool PreparePixmaps();
  bool ProcessEvents();
};

#endif /* HAS_XCOMPOSITE */
#endif /* CGUIEXTERNALAPPCONTROL_H_ */
