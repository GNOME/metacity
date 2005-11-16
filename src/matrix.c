/* This file is from The Gimp */

/* LIBGIMP - The GIMP Library
 * Copyright (C) 1995-1997 Peter Mattis and Spencer Kimball
 *
 * matrix.c
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
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include <glib.h>
#include "matrix.h"
#include <math.h>

#define EPSILON 1e-6


/**
 * matrix2_identity:
 * @matrix: A matrix.
 *
 * Sets the matrix to the identity matrix.
 */
void
matrix2_identity (Matrix2 *matrix)
{
    static const Matrix2 identity = { { { 1.0, 0.0 },
					{ 0.0, 1.0 } } };
    
    *matrix = identity;
}

/**
 * matrix2_mult:
 * @matrix1: The first input matrix.
 * @matrix2: The second input matrix which will be overwritten by the result.
 *
 * Multiplies two matrices and puts the result into the second one.
 */
void
matrix2_mult (const Matrix2 *matrix1,
	      Matrix2       *matrix2)
{
    Matrix2  tmp;
    
    tmp.coeff[0][0] = (matrix1->coeff[0][0] * matrix2->coeff[0][0] +
		       matrix1->coeff[0][1] * matrix2->coeff[1][0]);
    tmp.coeff[0][1] = (matrix1->coeff[0][0] * matrix2->coeff[0][1] +
		       matrix1->coeff[0][1] * matrix2->coeff[1][1]);
    tmp.coeff[1][0] = (matrix1->coeff[1][0] * matrix2->coeff[0][0] +
		       matrix1->coeff[1][1] * matrix2->coeff[1][0]);
    tmp.coeff[1][1] = (matrix1->coeff[1][0] * matrix2->coeff[0][1] +
		       matrix1->coeff[1][1] * matrix2->coeff[1][1]);
    
    *matrix2 = tmp;
}

/**
 * matrix3_identity:
 * @matrix: A matrix.
 *
 * Sets the matrix to the identity matrix.
 */
void
matrix3_identity (Matrix3 *matrix)
{
    static const Matrix3 identity = { { { 1.0, 0.0, 0.0 },
					{ 0.0, 1.0, 0.0 },
					{ 0.0, 0.0, 1.0 } } };
    
    *matrix = identity;
}
/**
 * matrix3_mult:
 * @matrix1: The first input matrix.
 * @matrix2: The second input matrix which will be overwritten by the result.
 *
 * Multiplies two matrices and puts the result into the second one.
 */
void
matrix3_mult (const Matrix3 *matrix1,
	      Matrix3       *matrix2)
{
    gint         i, j;
    Matrix3  tmp;
    gdouble      t1, t2, t3;
    
    for (i = 0; i < 3; i++)
    {
	t1 = matrix1->coeff[i][0];
	t2 = matrix1->coeff[i][1];
	t3 = matrix1->coeff[i][2];
	
	for (j = 0; j < 3; j++)
        {
	    tmp.coeff[i][j]  = t1 * matrix2->coeff[0][j];
	    tmp.coeff[i][j] += t2 * matrix2->coeff[1][j];
	    tmp.coeff[i][j] += t3 * matrix2->coeff[2][j];
        }
    }
    
    *matrix2 = tmp;
}

/**
 * matrix3_translate:
 * @matrix: The matrix that is to be translated.
 * @x: Translation in X direction.
 * @y: Translation in Y direction.
 *
 * Translates the matrix by x and y.
 */
void
matrix3_translate (Matrix3 *matrix,
		   gdouble      x,
		   gdouble      y)
{
    gdouble g, h, i;
    
    g = matrix->coeff[2][0];
    h = matrix->coeff[2][1];
    i = matrix->coeff[2][2];
    
    matrix->coeff[0][0] += x * g;
    matrix->coeff[0][1] += x * h;
    matrix->coeff[0][2] += x * i;
    matrix->coeff[1][0] += y * g;
    matrix->coeff[1][1] += y * h;
    matrix->coeff[1][2] += y * i;
}

void
matrix3_transpose (Matrix3 *matrix)
{
    int i, j;
    for (i = 0; i < 3; ++i)
	for (j = 0; j < 3; ++j)
	{
	    int tmp;
	    tmp = matrix->coeff[i][j];
	    matrix->coeff[i][j] = matrix->coeff[j][i];
	    matrix->coeff[j][i] = tmp;
	}
}

/**
 * matrix3_scale:
 * @matrix: The matrix that is to be scaled.
 * @x: X scale factor.
 * @y: Y scale factor.
 *
 * Scales the matrix by x and y
 */
void
matrix3_scale (Matrix3 *matrix,
	       gdouble      x,
	       gdouble      y)
{
    matrix->coeff[0][0] *= x;
    matrix->coeff[0][1] *= x;
    matrix->coeff[0][2] *= x;
    
    matrix->coeff[1][0] *= y;
    matrix->coeff[1][1] *= y;
    matrix->coeff[1][2] *= y;
}

/**
 * matrix3_rotate:
 * @matrix: The matrix that is to be rotated.
 * @theta: The angle of rotation (in radians).
 *
 * Rotates the matrix by theta degrees.
 */
void
matrix3_rotate (Matrix3 *matrix,
		gdouble      theta)
{
    gdouble t1, t2;
    gdouble cost, sint;
    
    cost = cos (theta);
    sint = sin (theta);
    
    t1 = matrix->coeff[0][0];
    t2 = matrix->coeff[1][0];
    matrix->coeff[0][0] = cost * t1 - sint * t2;
    matrix->coeff[1][0] = sint * t1 + cost * t2;
    
    t1 = matrix->coeff[0][1];
    t2 = matrix->coeff[1][1];
    matrix->coeff[0][1] = cost * t1 - sint * t2;
    matrix->coeff[1][1] = sint * t1 + cost * t2;
    
    t1 = matrix->coeff[0][2];
    t2 = matrix->coeff[1][2];
    matrix->coeff[0][2] = cost * t1 - sint * t2;
    matrix->coeff[1][2] = sint * t1 + cost * t2;
}

/**
 * matrix3_xshear:
 * @matrix: The matrix that is to be sheared.
 * @amount: X shear amount.
 *
 * Shears the matrix in the X direction.
 */
void
matrix3_xshear (Matrix3 *matrix,
		gdouble      amount)
{
    matrix->coeff[0][0] += amount * matrix->coeff[1][0];
    matrix->coeff[0][1] += amount * matrix->coeff[1][1];
    matrix->coeff[0][2] += amount * matrix->coeff[1][2];
}

/**
 * matrix3_yshear:
 * @matrix: The matrix that is to be sheared.
 * @amount: Y shear amount.
 *
 * Shears the matrix in the Y direction.
 */
void
matrix3_yshear (Matrix3 *matrix,
		gdouble      amount)
{
    matrix->coeff[1][0] += amount * matrix->coeff[0][0];
    matrix->coeff[1][1] += amount * matrix->coeff[0][1];
    matrix->coeff[1][2] += amount * matrix->coeff[0][2];
}


/**
 * matrix3_affine:
 * @matrix: The input matrix.
 * @a:
 * @b:
 * @c:
 * @d:
 * @e:
 * @f:
 *
 * Applies the affine transformation given by six values to @matrix.
 * The six values form define an affine transformation matrix as
 * illustrated below:
 *
 *  ( a c e )
 *  ( b d f )
 *  ( 0 0 1 )
 **/
void
matrix3_affine (Matrix3 *matrix,
		gdouble      a,
		gdouble      b,
		gdouble      c,
		gdouble      d,
		gdouble      e,
		gdouble      f)
{
    Matrix3 affine;
    
    affine.coeff[0][0] = a;
    affine.coeff[1][0] = b;
    affine.coeff[2][0] = 0.0;
    
    affine.coeff[0][1] = c;
    affine.coeff[1][1] = d;
    affine.coeff[2][1] = 0.0;
    
    affine.coeff[0][2] = e;
    affine.coeff[1][2] = f;
    affine.coeff[2][2] = 1.0;
    
    matrix3_mult (&affine, matrix);
}


/**
 * matrix3_determinant:
 * @matrix: The input matrix.
 *
 * Calculates the determinant of the given matrix.
 *
 * Returns: The determinant.
 */
gdouble
matrix3_determinant (const Matrix3 *matrix)
{
    gdouble determinant;
    
    determinant  = (matrix->coeff[0][0] *
		    (matrix->coeff[1][1] * matrix->coeff[2][2] -
		     matrix->coeff[1][2] * matrix->coeff[2][1]));
    determinant -= (matrix->coeff[1][0] *
		    (matrix->coeff[0][1] * matrix->coeff[2][2] -
		     matrix->coeff[0][2] * matrix->coeff[2][1]));
    determinant += (matrix->coeff[2][0] *
		    (matrix->coeff[0][1] * matrix->coeff[1][2] -
		     matrix->coeff[0][2] * matrix->coeff[1][1]));
    
    return determinant;
}

/**
 * matrix3_invert:
 * @matrix: The matrix that is to be inverted.
 *
 * Inverts the given matrix.
 *
 * Returns FALSE if the matrix is not invertible
 */
gboolean
matrix3_invert (Matrix3 *matrix)
{
    Matrix3 inv;
    gdouble     det;
    
    det = matrix3_determinant (matrix);
    
    if (det == 0.0)
	return FALSE;
    
    det = 1.0 / det;
    
    inv.coeff[0][0] =   (matrix->coeff[1][1] * matrix->coeff[2][2] -
			 matrix->coeff[1][2] * matrix->coeff[2][1]) * det;
    
    inv.coeff[1][0] = - (matrix->coeff[1][0] * matrix->coeff[2][2] -
			 matrix->coeff[1][2] * matrix->coeff[2][0]) * det;
    
    inv.coeff[2][0] =   (matrix->coeff[1][0] * matrix->coeff[2][1] -
			 matrix->coeff[1][1] * matrix->coeff[2][0]) * det;
    
    inv.coeff[0][1] = - (matrix->coeff[0][1] * matrix->coeff[2][2] -
			 matrix->coeff[0][2] * matrix->coeff[2][1]) * det;
    
    inv.coeff[1][1] =   (matrix->coeff[0][0] * matrix->coeff[2][2] -
			 matrix->coeff[0][2] * matrix->coeff[2][0]) * det;
    
    inv.coeff[2][1] = - (matrix->coeff[0][0] * matrix->coeff[2][1] -
			 matrix->coeff[0][1] * matrix->coeff[2][0]) * det;
    
    inv.coeff[0][2] =   (matrix->coeff[0][1] * matrix->coeff[1][2] -
			 matrix->coeff[0][2] * matrix->coeff[1][1]) * det;
    
    inv.coeff[1][2] = - (matrix->coeff[0][0] * matrix->coeff[1][2] -
			 matrix->coeff[0][2] * matrix->coeff[1][0]) * det;
    
    inv.coeff[2][2] =   (matrix->coeff[0][0] * matrix->coeff[1][1] -
			 matrix->coeff[0][1] * matrix->coeff[1][0]) * det;
    
    *matrix = inv;

    return TRUE;
}


/*  functions to test for matrix properties  */


/**
 * matrix3_is_diagonal:
 * @matrix: The matrix that is to be tested.
 *
 * Checks if the given matrix is diagonal.
 *
 * Returns: TRUE if the matrix is diagonal.
 */
gboolean
matrix3_is_diagonal (const Matrix3 *matrix)
{
    gint i, j;
    
    for (i = 0; i < 3; i++)
    {
	for (j = 0; j < 3; j++)
        {
	    if (i != j && fabs (matrix->coeff[i][j]) > EPSILON)
		return FALSE;
        }
    }
    
    return TRUE;
}

/**
 * matrix3_is_identity:
 * @matrix: The matrix that is to be tested.
 *
 * Checks if the given matrix is the identity matrix.
 *
 * Returns: TRUE if the matrix is the identity matrix.
 */
gboolean
matrix3_is_identity (const Matrix3 *matrix)
{
    gint i, j;
    
    for (i = 0; i < 3; i++)
    {
	for (j = 0; j < 3; j++)
        {
	    if (i == j)
            {
		if (fabs (matrix->coeff[i][j] - 1.0) > EPSILON)
		    return FALSE;
            }
	    else
            {
		if (fabs (matrix->coeff[i][j]) > EPSILON)
		    return FALSE;
            }
        }
    }
    
    return TRUE;
}

/*  Check if we'll need to interpolate when applying this matrix.
    This function returns TRUE if all entries of the upper left
    2x2 matrix are either 0 or 1
*/


/**
 * matrix3_is_simple:
 * @matrix: The matrix that is to be tested.
 *
 * Checks if we'll need to interpolate when applying this matrix as
 * a transformation.
 *
 * Returns: TRUE if all entries of the upper left 2x2 matrix are either
 * 0 or 1
 */
gboolean
matrix3_is_simple (const Matrix3 *matrix)
{
    gdouble absm;
    gint    i, j;
    
    for (i = 0; i < 2; i++)
    {
	for (j = 0; j < 2; j++)
        {
	    absm = fabs (matrix->coeff[i][j]);
	    if (absm > EPSILON && fabs (absm - 1.0) > EPSILON)
		return FALSE;
        }
    }
    
    return TRUE;
}

/**
 * matrix4_to_deg:
 * @matrix:
 * @a:
 * @b:
 * @c:
 *
 *
 **/
void
matrix4_to_deg (const Matrix4 *matrix,
		gdouble           *a,
		gdouble           *b,
		gdouble           *c)
{
    *a = 180 * (asin (matrix->coeff[1][0]) / G_PI_2);
    *b = 180 * (asin (matrix->coeff[2][0]) / G_PI_2);
    *c = 180 * (asin (matrix->coeff[2][1]) / G_PI_2);
}

void
transform_matrix_perspective (gint         x1,
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
			      Matrix3 *result)
{
    Matrix3 matrix;
    gdouble     scalex;
    gdouble     scaley;
    
    scalex = scaley = 1.0;
    
    if ((x2 - x1) > 0)
	scalex = 1.0 / (gdouble) (x2 - x1);
    
    if ((y2 - y1) > 0)
	scaley = 1.0 / (gdouble) (y2 - y1);
    
    /* Determine the perspective transform that maps from
     * the unit cube to the transformed coordinates
     */
    {
	gdouble dx1, dx2, dx3, dy1, dy2, dy3;
	
	dx1 = tx2 - tx4;
	dx2 = tx3 - tx4;
	dx3 = tx1 - tx2 + tx4 - tx3;
	
	dy1 = ty2 - ty4;
	dy2 = ty3 - ty4;
	dy3 = ty1 - ty2 + ty4 - ty3;
	
	/*  Is the mapping affine?  */
	if ((dx3 == 0.0) && (dy3 == 0.0))
	{
	    matrix.coeff[0][0] = tx2 - tx1;
	    matrix.coeff[0][1] = tx4 - tx2;
	    matrix.coeff[0][2] = tx1;
	    matrix.coeff[1][0] = ty2 - ty1;
	    matrix.coeff[1][1] = ty4 - ty2;
	    matrix.coeff[1][2] = ty1;
	    matrix.coeff[2][0] = 0.0;
	    matrix.coeff[2][1] = 0.0;
	}
	else
	{
	    gdouble det1, det2;
	    
	    det1 = dx3 * dy2 - dy3 * dx2;
	    det2 = dx1 * dy2 - dy1 * dx2;
	    
	    if (det1 == 0.0 && det2 == 0.0)
		matrix.coeff[2][0] = 1.0;
	    else
		matrix.coeff[2][0] = det1 / det2;
	    
	    det1 = dx1 * dy3 - dy1 * dx3;
	    
	    if (det1 == 0.0 && det2 == 0.0)
		matrix.coeff[2][1] = 1.0;
	    else
		matrix.coeff[2][1] = det1 / det2;
	    
	    matrix.coeff[0][0] = tx2 - tx1 + matrix.coeff[2][0] * tx2;
	    matrix.coeff[0][1] = tx3 - tx1 + matrix.coeff[2][1] * tx3;
	    matrix.coeff[0][2] = tx1;
	    
	    matrix.coeff[1][0] = ty2 - ty1 + matrix.coeff[2][0] * ty2;
	    matrix.coeff[1][1] = ty3 - ty1 + matrix.coeff[2][1] * ty3;
	    matrix.coeff[1][2] = ty1;
	}
	
	matrix.coeff[2][2] = 1.0;
    }
    
    matrix3_identity  (result);
    matrix3_translate (result, -x1, -y1);
    matrix3_scale     (result, scalex, scaley);
    matrix3_mult      (&matrix, result);
}

void
matrix3_multiply_vector (const Matrix3 *A,
			 const Vector3 *v,
			 Vector3	*result)
{
    result->coeff[0] =
	v->coeff[0] * A->coeff[0][0] +
	v->coeff[1] * A->coeff[1][0] +
	v->coeff[2] * A->coeff[2][0];
    result->coeff[1] =
	v->coeff[0] * A->coeff[0][1] +
	v->coeff[1] * A->coeff[1][1] +
	v->coeff[2] * A->coeff[2][1];
    result->coeff[2] = 
	v->coeff[0] * A->coeff[0][2] +
	v->coeff[1] * A->coeff[1][2] +
	v->coeff[2] * A->coeff[2][2];
}

#if 0
void
solve_3x3 (const Matrix3 *A,
	   Vector3 *x,
	   const Vector3 *b)
{
    Matrix3 inv = *A;
    if (matrix3_invert (&inv))
    {
	matrix3_multiply_vector (&inv, b, x);
	return TRUE;
    }
    else
    {
	return FALSE;
    }
}
#endif
