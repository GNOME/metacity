/* LIBGIMP - The GIMP Library
 * Copyright (C) 1995-1997 Peter Mattis and Spencer Kimball
 *
 * matrix.h
 * Copyright (C) 1998 Jay Cox <jaycox@earthlink.net>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef __MATRIX_H__
#define __MATRIX_H__

G_BEGIN_DECLS

/* For information look into the C source or the html documentation */

typedef struct _Matrix2 Matrix2;
typedef struct _Matrix3 Matrix3;
typedef struct _Matrix4 Matrix4;
typedef struct _Vector3 Vector3;

struct _Matrix2
{
    gdouble coeff[2][2];
};

struct _Matrix3
{
    gdouble coeff[3][3];
};

struct _Matrix4
{
    gdouble coeff[4][4];
};

struct _Vector3
{
    gdouble coeff[3];
};


void     matrix2_identity        (Matrix2       *matrix);
void     matrix2_mult            (const Matrix2 *matrix1,
				  Matrix2       *matrix2);

void matrix3_transpose (Matrix3 *matrix);

void     matrix3_identity        (Matrix3       *matrix);
void     matrix3_mult            (const Matrix3 *matrix1,
				  Matrix3       *matrix2);
void     matrix3_translate       (Matrix3       *matrix,
				  gdouble        x,
				  gdouble        y);
void     matrix3_scale           (Matrix3       *matrix,
				  gdouble        x,
				  gdouble        y);
void     matrix3_rotate          (Matrix3       *matrix,
				  gdouble        theta);
void     matrix3_xshear          (Matrix3       *matrix,
				  gdouble        amount);
void     matrix3_yshear          (Matrix3       *matrix,
				  gdouble        amount);
void     matrix3_affine          (Matrix3       *matrix,
				  gdouble        a,
				  gdouble        b,
				  gdouble        c,
				  gdouble        d,
				  gdouble        e,
				  gdouble        f);
gdouble  matrix3_determinant     (const Matrix3 *matrix);
gboolean matrix3_invert          (Matrix3       *matrix);
gboolean matrix3_is_diagonal     (const Matrix3 *matrix);
gboolean matrix3_is_identity     (const Matrix3 *matrix);
gboolean matrix3_is_simple       (const Matrix3 *matrix);
void     matrix3_multiply_vector (const Matrix3 *A,
				  const Vector3 *x,
				  Vector3	*result);
void     matrix3_transform_point (const Matrix3 *matrix,
				  gdouble        x,
				  gdouble        y,
				  gdouble       *newx,
				  gdouble       *newy);

void     matrix4_to_deg          (const Matrix4 *matrix,
				  gdouble       *a,
				  gdouble       *b,
				  gdouble       *c);
void    transform_matrix_perspective (gint         x1,
				      gint         y1,
				      gint         x2,
				      gint         y2,
				      gdouble      tx1,
				      gdouble      ty1,
				      gdouble      tx2,
				      gdouble      ty2,
				      gdouble      tx3,
				      gdouble      ty3,
				      gdouble      tx4,
				      gdouble      ty4,
				      Matrix3 *result);

G_END_DECLS

#endif /* __MATRIX_H__ */
