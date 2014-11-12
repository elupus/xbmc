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


#include "system.h"

#ifdef HAS_GL
#include "system_gl.h"

#include "MatrixGLES.h"

CMatrixGLES g_matrices;

#define MODE_WITHIN_RANGE(m)       ((m >= 0) && (m < (int)MM_MATRIXSIZE))

CMatrixGLES::CMatrixGLES()
{
  for (unsigned int i=0; i < MM_MATRIXSIZE; i++)
    m_matrices[i].push_back(CMatrixGL());
  m_matrixMode = (EMATRIXMODE)-1;
}

CMatrixGLES::~CMatrixGLES()
{
}

GLfloat* CMatrixGLES::GetMatrix(EMATRIXMODE mode)
{
  if (MODE_WITHIN_RANGE(mode))
  {
    if (!m_matrices[mode].empty())
    {
      return m_matrices[mode].back();
    }
  }
  return NULL;
}

void CMatrixGLES::MatrixMode(EMATRIXMODE mode)
{
  if (MODE_WITHIN_RANGE(mode))
    m_matrixMode = mode;
  else
    m_matrixMode = (EMATRIXMODE)-1;
}

void CMatrixGLES::PushMatrix()
{
  if (MODE_WITHIN_RANGE(m_matrixMode))
    m_matrices[m_matrixMode].push_back(CMatrixGL(m_matrices[m_matrixMode].back()));
}

void CMatrixGLES::PopMatrix()
{
  if (MODE_WITHIN_RANGE(m_matrixMode))
  {
    if (m_matrices[m_matrixMode].size() > 1)
    { 
      m_matrices[m_matrixMode].pop_back();
    }
  }
}

static void __gluMultMatrixVecf(const GLfloat matrix[16], const GLfloat in[4], GLfloat out[4])
{
  int i;

  for (i=0; i<4; i++)
  {
    out[i] = in[0] * matrix[0*4+i] +
             in[1] * matrix[1*4+i] +
             in[2] * matrix[2*4+i] +
             in[3] * matrix[3*4+i];
  }
}

// gluProject implementation taken from Mesa3D
bool CMatrixGLES::Project(GLfloat objx, GLfloat objy, GLfloat objz, const GLfloat modelMatrix[16], const GLfloat projMatrix[16], const GLint viewport[4], GLfloat* winx, GLfloat* winy, GLfloat* winz)
{
  GLfloat in[4];
  GLfloat out[4];

  in[0]=objx;
  in[1]=objy;
  in[2]=objz;
  in[3]=1.0;
  __gluMultMatrixVecf(modelMatrix, in, out);
  __gluMultMatrixVecf(projMatrix, out, in);
  if (in[3] == 0.0)
    return false;
  in[0] /= in[3];
  in[1] /= in[3];
  in[2] /= in[3];
  /* Map x, y and z to range 0-1 */
  in[0] = in[0] * 0.5 + 0.5;
  in[1] = in[1] * 0.5 + 0.5;
  in[2] = in[2] * 0.5 + 0.5;

  /* Map x,y to viewport */
  in[0] = in[0] * viewport[2] + viewport[0];
  in[1] = in[1] * viewport[3] + viewport[1];

  *winx=in[0];
  *winy=in[1];
  *winz=in[2];
  return true;
}

#endif
