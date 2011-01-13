/*
Copyright_License {

  XCSoar Glide Computer - http://www.xcsoar.org/
  Copyright (C) 2000-2010 The XCSoar Project
  A detailed list of copyright holders can be found in the file "AUTHORS".

  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public License
  as published by the Free Software Foundation; either version 2
  of the License, or (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
}
*/

#include "Screen/OpenGL/VertexArray.hpp"
#include "Math/FastMath.h"
#include <assert.h>

GLCircleVertices::GLCircleVertices(GLvalue center_x, GLvalue center_y,
                                   GLvalue radius)
{
  assert(4096 % SIZE == 0);  // implies: assert(SIZE % 2 == 0)
  RasterPoint *p = v, *p2 = v + (SIZE/2);

  for (unsigned i = 0; i < SIZE/2; ++i) {
    int offset_x = ICOSTABLE[i * (4096 / SIZE)] * (int)radius / 1024.;
    int offset_y = ISINETABLE[i * (4096 / SIZE)] * (int)radius / 1024.;

    p->x = center_x + offset_x;
    p->y = center_y + offset_y;
    ++p;
    p2->x = center_x - offset_x;
    p2->y = center_y - offset_y;
    ++p2;
  }
}
