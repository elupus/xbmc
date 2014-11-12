#pragma once
/*
*      Copyright (C) 2005-2013 Team XBMC
 *      http://xbmc.org
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

#include <vector>
#include <string.h>
#include "MatrixGL.h"
#include "utils/Log.h"

enum EMATRIXMODE
{
  MM_PROJECTION = 0,
  MM_MODELVIEW,
  MM_TEXTURE,
  MM_MATRIXSIZE  // Must be last! used for size of matrices
};

#define MM_MODE_WITHIN_RANGE(m)       ((m >= 0) && (m < (int)MM_MATRIXSIZE))

class CMatrixGLES
{
public:
  CMatrixGLES();
  ~CMatrixGLES();
  
  GLfloat* GetMatrix(EMATRIXMODE mode);

  void MatrixMode(EMATRIXMODE mode);
  void PushMatrix();
  void PopMatrix();

  void LoadIdentity()
  {
    if (MM_MODE_WITHIN_RANGE(m_matrixMode))
      m_matrices[m_matrixMode].back().LoadIdentity();
  }

  void Ortho(GLfloat l, GLfloat r, GLfloat b, GLfloat t, GLfloat n, GLfloat f)
  {
    if (MM_MODE_WITHIN_RANGE(m_matrixMode))
      m_matrices[m_matrixMode].back().Ortho(l, r, b, t, n, f);
  }

  void Ortho2D(GLfloat l, GLfloat r, GLfloat b, GLfloat t)
  {
    if (MM_MODE_WITHIN_RANGE(m_matrixMode))
      m_matrices[m_matrixMode].back().Ortho2D(l, r, b, t);
  }

  void Frustum(GLfloat l, GLfloat r, GLfloat b, GLfloat t, GLfloat n, GLfloat f)
  {
    if (MM_MODE_WITHIN_RANGE(m_matrixMode))
      m_matrices[m_matrixMode].back().Frustum(l, r, b, t, n, f);
  }

  void Translatef(GLfloat x, GLfloat y, GLfloat z)
  {
    if (MM_MODE_WITHIN_RANGE(m_matrixMode))
      m_matrices[m_matrixMode].back().Translatef(x, y, z);
  }

  void Scalef(GLfloat x, GLfloat y, GLfloat z)
  {
    if (MM_MODE_WITHIN_RANGE(m_matrixMode))
      m_matrices[m_matrixMode].back().Scalef(x, y, z);
  }

  void Rotatef(GLfloat angle, GLfloat x, GLfloat y, GLfloat z)
  {
    if (MM_MODE_WITHIN_RANGE(m_matrixMode))
      m_matrices[m_matrixMode].back().Rotatef(angle, x, y, z);
  }

  void MultMatrixf(const GLfloat *matrix)
  {
    if (MM_MODE_WITHIN_RANGE(m_matrixMode))
      m_matrices[m_matrixMode].back().MultMatrixf(matrix);
  }

  void LookAt(GLfloat eyex, GLfloat eyey, GLfloat eyez, GLfloat centerx, GLfloat centery, GLfloat centerz, GLfloat upx, GLfloat upy, GLfloat upz)
  {
    if (MM_MODE_WITHIN_RANGE(m_matrixMode))
      m_matrices[m_matrixMode].back().LookAt(eyex, eyey, eyez, centerx, centery, centerz, upx, upy, upz);
  }

  void PrintMatrix(void)
  {
    for (unsigned int i=0; i < MM_MATRIXSIZE; i++)
    {
      CLog::Log(LOGDEBUG, "MatrixGLES - Matrix:%d", i);
      m_matrices[i].back().PrintMatrix();
    }
  }

  static bool Project(GLfloat objx, GLfloat objy, GLfloat objz, const GLfloat modelMatrix[16], const GLfloat projMatrix[16], const GLint viewport[4], GLfloat* winx, GLfloat* winy, GLfloat* winz);

protected:

  std::vector<CMatrixGL> m_matrices[(int)MM_MATRIXSIZE];
  EMATRIXMODE m_matrixMode;
};

extern CMatrixGLES g_matrices;
