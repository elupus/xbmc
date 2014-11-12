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


#include <cmath>
#include <cstring>
#include <stack>

class CMatrixGL
{
public:

  CMatrixGL()                                  { memset(&m_matrix, 0, sizeof(m_matrix)); };
  CMatrixGL(const float matrix[16])            { memcpy(m_matrix, matrix, sizeof(m_matrix)); }
  CMatrixGL(const CMatrixGL &rhs )             { memcpy(m_matrix, rhs.m_matrix, sizeof(m_matrix)); }
  CMatrixGL &operator=( const CMatrixGL &rhs ) { memcpy(m_matrix, rhs.m_matrix, sizeof(m_matrix)); return *this;}
  operator float*()                            { return m_matrix; }
  operator const float*() const                { return m_matrix; }

  void LoadIdentity();
  void Ortho(GLfloat l, GLfloat r, GLfloat b, GLfloat t, GLfloat n, GLfloat f);
  void Ortho2D(GLfloat l, GLfloat r, GLfloat b, GLfloat t);
  void Frustum(GLfloat l, GLfloat r, GLfloat b, GLfloat t, GLfloat n, GLfloat f);
  void Translatef(GLfloat x, GLfloat y, GLfloat z);
  void Scalef(GLfloat x, GLfloat y, GLfloat z);
  void Rotatef(GLfloat angle, GLfloat x, GLfloat y, GLfloat z);
  void MultMatrixf(const GLfloat *matrix);
  void LookAt(GLfloat eyex, GLfloat eyey, GLfloat eyez, GLfloat centerx, GLfloat centery, GLfloat centerz, GLfloat upx, GLfloat upy, GLfloat upz);
  void PrintMatrix(void);

  GLfloat m_matrix[16];
};
