/*
 * Copyright 2006 The Android Open Source Project
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "SkMatrix.h"
#include "SkFloatBits.h"
#include "SkOnce.h"
#include "SkString.h"

#define kMatrix22Elem   SK_Scalar1

static inline float SkDoubleToFloat(double x) {
    return static_cast<float>(x);
}

/*      [scale-x    skew-x      trans-x]   [X]   [X']
        [skew-y     scale-y     trans-y] * [Y] = [Y']
        [persp-0    persp-1     persp-2]   [1]   [1 ]
*/

void SkMatrix::reset() {
    fMat[kMScaleX] = fMat[kMScaleY] = SK_Scalar1;
    fMat[kMSkewX]  = fMat[kMSkewY] =
    fMat[kMTransX] = fMat[kMTransY] =
    fMat[kMPersp0] = fMat[kMPersp1] = 0;
    fMat[kMPersp2] = kMatrix22Elem;

    this->setTypeMask(kIdentity_Mask | kRectStaysRect_Mask);
}

// this guy aligns with the masks, so we can compute a mask from a varaible 0/1
enum {
    kTranslate_Shift,
    kScale_Shift,
    kAffine_Shift,
    kPerspective_Shift,
    kRectStaysRect_Shift
};

static const int32_t kScalar1Int = 0x3f800000;

uint8_t SkMatrix::computePerspectiveTypeMask() const {
    // Benchmarking suggests that replacing this set of SkScalarAs2sCompliment
    // is a win, but replacing those below is not. We don't yet understand
    // that result.
    if (fMat[kMPersp0] != 0 || fMat[kMPersp1] != 0 ||
        fMat[kMPersp2] != kMatrix22Elem) {
        // If this is a perspective transform, we return true for all other
        // transform flags - this does not disable any optimizations, respects
        // the rule that the type mask must be conservative, and speeds up
        // type mask computation.
        return SkToU8(kORableMasks);
    }

    return SkToU8(kOnlyPerspectiveValid_Mask | kUnknown_Mask);
}

uint8_t SkMatrix::computeTypeMask() const {
    unsigned mask = 0;

    if (fMat[kMPersp0] != 0 || fMat[kMPersp1] != 0 ||
        fMat[kMPersp2] != kMatrix22Elem) {
        // Once it is determined that that this is a perspective transform,
        // all other flags are moot as far as optimizations are concerned.
        return SkToU8(kORableMasks);
    }

    if (fMat[kMTransX] != 0 || fMat[kMTransY] != 0) {
        mask |= kTranslate_Mask;
    }

    int m00 = SkScalarAs2sCompliment(fMat[SkMatrix::kMScaleX]);
    int m01 = SkScalarAs2sCompliment(fMat[SkMatrix::kMSkewX]);
    int m10 = SkScalarAs2sCompliment(fMat[SkMatrix::kMSkewY]);
    int m11 = SkScalarAs2sCompliment(fMat[SkMatrix::kMScaleY]);

    if (m01 | m10) {
        // The skew components may be scale-inducing, unless we are dealing
        // with a pure rotation.  Testing for a pure rotation is expensive,
        // so we opt for being conservative by always setting the scale bit.
        // along with affine.
        // By doing this, we are also ensuring that matrices have the same
        // type masks as their inverses.
        mask |= kAffine_Mask | kScale_Mask;

        // For rectStaysRect, in the affine case, we only need check that
        // the primary diagonal is all zeros and that the secondary diagonal
        // is all non-zero.

        // map non-zero to 1
        m01 = m01 != 0;
        m10 = m10 != 0;

        int dp0 = 0 == (m00 | m11) ;  // true if both are 0
        int ds1 = m01 & m10;        // true if both are 1

        mask |= (dp0 & ds1) << kRectStaysRect_Shift;
    } else {
        // Only test for scale explicitly if not affine, since affine sets the
        // scale bit.
        if ((m00 - kScalar1Int) | (m11 - kScalar1Int)) {
            mask |= kScale_Mask;
        }

        // Not affine, therefore we already know secondary diagonal is
        // all zeros, so we just need to check that primary diagonal is
        // all non-zero.

        // map non-zero to 1
        m00 = m00 != 0;
        m11 = m11 != 0;

        // record if the (p)rimary diagonal is all non-zero
        mask |= (m00 & m11) << kRectStaysRect_Shift;
    }

    return SkToU8(mask);
}

///////////////////////////////////////////////////////////////////////////////

bool operator==(const SkMatrix& a, const SkMatrix& b) {
    const SkScalar* SK_RESTRICT ma = a.fMat;
    const SkScalar* SK_RESTRICT mb = b.fMat;

    return  ma[0] == mb[0] && ma[1] == mb[1] && ma[2] == mb[2] &&
            ma[3] == mb[3] && ma[4] == mb[4] && ma[5] == mb[5] &&
            ma[6] == mb[6] && ma[7] == mb[7] && ma[8] == mb[8];
}

///////////////////////////////////////////////////////////////////////////////

// helper function to determine if upper-left 2x2 of matrix is degenerate
static inline bool is_degenerate_2x2(SkScalar scaleX, SkScalar skewX,
                                     SkScalar skewY,  SkScalar scaleY) {
    SkScalar perp_dot = scaleX*scaleY - skewX*skewY;
    return SkScalarNearlyZero(perp_dot, SK_ScalarNearlyZero*SK_ScalarNearlyZero);
}

///////////////////////////////////////////////////////////////////////////////

bool SkMatrix::isSimilarity(SkScalar tol) const {
    // if identity or translate matrix
    TypeMask mask = this->getType();
    if (mask <= kTranslate_Mask) {
        return true;
    }
    if (mask & kPerspective_Mask) {
        return false;
    }

    SkScalar mx = fMat[kMScaleX];
    SkScalar my = fMat[kMScaleY];
    // if no skew, can just compare scale factors
    if (!(mask & kAffine_Mask)) {
        return !SkScalarNearlyZero(mx) && SkScalarNearlyEqual(SkScalarAbs(mx), SkScalarAbs(my));
    }
    SkScalar sx = fMat[kMSkewX];
    SkScalar sy = fMat[kMSkewY];

    if (is_degenerate_2x2(mx, sx, sy, my)) {
        return false;
    }

    // it has scales and skews, but it could also be rotation, check it out.
    SkVector vec[2];
    vec[0].set(mx, sx);
    vec[1].set(sy, my);

    return SkScalarNearlyZero(vec[0].dot(vec[1]), SkScalarSquare(tol)) &&
           SkScalarNearlyEqual(vec[0].lengthSqd(), vec[1].lengthSqd(),
                               SkScalarSquare(tol));
}

bool SkMatrix::preservesRightAngles(SkScalar tol) const {
    TypeMask mask = this->getType();

    if (mask <= (SkMatrix::kTranslate_Mask | SkMatrix::kScale_Mask)) {
        // identity, translate and/or scale
        return true;
    }
    if (mask & kPerspective_Mask) {
        return false;
    }

    SkASSERT(mask & kAffine_Mask);

    SkScalar mx = fMat[kMScaleX];
    SkScalar my = fMat[kMScaleY];
    SkScalar sx = fMat[kMSkewX];
    SkScalar sy = fMat[kMSkewY];

    if (is_degenerate_2x2(mx, sx, sy, my)) {
        return false;
    }

    // it has scales and skews, but it could also be rotation, check it out.
    SkVector vec[2];
    vec[0].set(mx, sx);
    vec[1].set(sy, my);

    return SkScalarNearlyZero(vec[0].dot(vec[1]), SkScalarSquare(tol)) &&
           SkScalarNearlyEqual(vec[0].lengthSqd(), vec[1].lengthSqd(),
                               SkScalarSquare(tol));
}

///////////////////////////////////////////////////////////////////////////////

void SkMatrix::setTranslate(SkScalar dx, SkScalar dy) {
    if (dx || dy) {
        fMat[kMTransX] = dx;
        fMat[kMTransY] = dy;

        fMat[kMScaleX] = fMat[kMScaleY] = SK_Scalar1;
        fMat[kMSkewX]  = fMat[kMSkewY] =
        fMat[kMPersp0] = fMat[kMPersp1] = 0;
        fMat[kMPersp2] = kMatrix22Elem;

        this->setTypeMask(kTranslate_Mask | kRectStaysRect_Mask);
    } else {
        this->reset();
    }
}

bool SkMatrix::preTranslate(SkScalar dx, SkScalar dy) {
    if (this->hasPerspective()) {
        SkMatrix    m;
        m.setTranslate(dx, dy);
        return this->preConcat(m);
    }

    if (dx || dy) {
        fMat[kMTransX] += SkScalarMul(fMat[kMScaleX], dx) +
                          SkScalarMul(fMat[kMSkewX], dy);
        fMat[kMTransY] += SkScalarMul(fMat[kMSkewY], dx) +
                          SkScalarMul(fMat[kMScaleY], dy);

        this->setTypeMask(kUnknown_Mask | kOnlyPerspectiveValid_Mask);
    }
    return true;
}

bool SkMatrix::postTranslate(SkScalar dx, SkScalar dy) {
    if (this->hasPerspective()) {
        SkMatrix    m;
        m.setTranslate(dx, dy);
        return this->postConcat(m);
    }

    if (dx || dy) {
        fMat[kMTransX] += dx;
        fMat[kMTransY] += dy;
        this->setTypeMask(kUnknown_Mask | kOnlyPerspectiveValid_Mask);
    }
    return true;
}

///////////////////////////////////////////////////////////////////////////////

void SkMatrix::setScale(SkScalar sx, SkScalar sy, SkScalar px, SkScalar py) {
    if (SK_Scalar1 == sx && SK_Scalar1 == sy) {
        this->reset();
    } else {
        fMat[kMScaleX] = sx;
        fMat[kMScaleY] = sy;
        fMat[kMTransX] = px - SkScalarMul(sx, px);
        fMat[kMTransY] = py - SkScalarMul(sy, py);
        fMat[kMPersp2] = kMatrix22Elem;

        fMat[kMSkewX]  = fMat[kMSkewY] =
        fMat[kMPersp0] = fMat[kMPersp1] = 0;

        this->setTypeMask(kScale_Mask | kTranslate_Mask | kRectStaysRect_Mask);
    }
}

void SkMatrix::setScale(SkScalar sx, SkScalar sy) {
    if (SK_Scalar1 == sx && SK_Scalar1 == sy) {
        this->reset();
    } else {
        fMat[kMScaleX] = sx;
        fMat[kMScaleY] = sy;
        fMat[kMPersp2] = kMatrix22Elem;

        fMat[kMTransX] = fMat[kMTransY] =
        fMat[kMSkewX]  = fMat[kMSkewY] =
        fMat[kMPersp0] = fMat[kMPersp1] = 0;

        this->setTypeMask(kScale_Mask | kRectStaysRect_Mask);
    }
}

bool SkMatrix::setIDiv(int divx, int divy) {
    if (!divx || !divy) {
        return false;
    }
    this->setScale(SK_Scalar1 / divx, SK_Scalar1 / divy);
    return true;
}

bool SkMatrix::preScale(SkScalar sx, SkScalar sy, SkScalar px, SkScalar py) {
    SkMatrix    m;
    m.setScale(sx, sy, px, py);
    return this->preConcat(m);
}

bool SkMatrix::preScale(SkScalar sx, SkScalar sy) {
    if (SK_Scalar1 == sx && SK_Scalar1 == sy) {
        return true;
    }

    // the assumption is that these multiplies are very cheap, and that
    // a full concat and/or just computing the matrix type is more expensive.
    // Also, the fixed-point case checks for overflow, but the float doesn't,
    // so we can get away with these blind multiplies.

    fMat[kMScaleX] = SkScalarMul(fMat[kMScaleX], sx);
    fMat[kMSkewY] = SkScalarMul(fMat[kMSkewY],   sx);
    fMat[kMPersp0] = SkScalarMul(fMat[kMPersp0], sx);

    fMat[kMSkewX] = SkScalarMul(fMat[kMSkewX],   sy);
    fMat[kMScaleY] = SkScalarMul(fMat[kMScaleY], sy);
    fMat[kMPersp1] = SkScalarMul(fMat[kMPersp1], sy);

    this->orTypeMask(kScale_Mask);
    return true;
}

bool SkMatrix::postScale(SkScalar sx, SkScalar sy, SkScalar px, SkScalar py) {
    if (SK_Scalar1 == sx && SK_Scalar1 == sy) {
        return true;
    }
    SkMatrix    m;
    m.setScale(sx, sy, px, py);
    return this->postConcat(m);
}

bool SkMatrix::postScale(SkScalar sx, SkScalar sy) {
    if (SK_Scalar1 == sx && SK_Scalar1 == sy) {
        return true;
    }
    SkMatrix    m;
    m.setScale(sx, sy);
    return this->postConcat(m);
}

// this guy perhaps can go away, if we have a fract/high-precision way to
// scale matrices
bool SkMatrix::postIDiv(int divx, int divy) {
    if (divx == 0 || divy == 0) {
        return false;
    }

    const float invX = 1.f / divx;
    const float invY = 1.f / divy;

    fMat[kMScaleX] *= invX;
    fMat[kMSkewX]  *= invX;
    fMat[kMTransX] *= invX;

    fMat[kMScaleY] *= invY;
    fMat[kMSkewY]  *= invY;
    fMat[kMTransY] *= invY;

    this->setTypeMask(kUnknown_Mask);
    return true;
}

////////////////////////////////////////////////////////////////////////////////////

void SkMatrix::setSinCos(SkScalar sinV, SkScalar cosV,
                         SkScalar px, SkScalar py) {
    const SkScalar oneMinusCosV = SK_Scalar1 - cosV;

    fMat[kMScaleX]  = cosV;
    fMat[kMSkewX]   = -sinV;
    fMat[kMTransX]  = SkScalarMul(sinV, py) + SkScalarMul(oneMinusCosV, px);

    fMat[kMSkewY]   = sinV;
    fMat[kMScaleY]  = cosV;
    fMat[kMTransY]  = SkScalarMul(-sinV, px) + SkScalarMul(oneMinusCosV, py);

    fMat[kMPersp0] = fMat[kMPersp1] = 0;
    fMat[kMPersp2] = kMatrix22Elem;

    this->setTypeMask(kUnknown_Mask | kOnlyPerspectiveValid_Mask);
}

void SkMatrix::setSinCos(SkScalar sinV, SkScalar cosV) {
    fMat[kMScaleX]  = cosV;
    fMat[kMSkewX]   = -sinV;
    fMat[kMTransX]  = 0;

    fMat[kMSkewY]   = sinV;
    fMat[kMScaleY]  = cosV;
    fMat[kMTransY]  = 0;

    fMat[kMPersp0] = fMat[kMPersp1] = 0;
    fMat[kMPersp2] = kMatrix22Elem;

    this->setTypeMask(kUnknown_Mask | kOnlyPerspectiveValid_Mask);
}

void SkMatrix::setRotate(SkScalar degrees, SkScalar px, SkScalar py) {
    SkScalar sinV, cosV;
    sinV = SkScalarSinCos(SkDegreesToRadians(degrees), &cosV);
    this->setSinCos(sinV, cosV, px, py);
}

void SkMatrix::setRotate(SkScalar degrees) {
    SkScalar sinV, cosV;
    sinV = SkScalarSinCos(SkDegreesToRadians(degrees), &cosV);
    this->setSinCos(sinV, cosV);
}

bool SkMatrix::preRotate(SkScalar degrees, SkScalar px, SkScalar py) {
    SkMatrix    m;
    m.setRotate(degrees, px, py);
    return this->preConcat(m);
}

bool SkMatrix::preRotate(SkScalar degrees) {
    SkMatrix    m;
    m.setRotate(degrees);
    return this->preConcat(m);
}

bool SkMatrix::postRotate(SkScalar degrees, SkScalar px, SkScalar py) {
    SkMatrix    m;
    m.setRotate(degrees, px, py);
    return this->postConcat(m);
}

bool SkMatrix::postRotate(SkScalar degrees) {
    SkMatrix    m;
    m.setRotate(degrees);
    return this->postConcat(m);
}

////////////////////////////////////////////////////////////////////////////////////

void SkMatrix::setSkew(SkScalar sx, SkScalar sy, SkScalar px, SkScalar py) {
    fMat[kMScaleX]  = SK_Scalar1;
    fMat[kMSkewX]   = sx;
    fMat[kMTransX]  = SkScalarMul(-sx, py);

    fMat[kMSkewY]   = sy;
    fMat[kMScaleY]  = SK_Scalar1;
    fMat[kMTransY]  = SkScalarMul(-sy, px);

    fMat[kMPersp0] = fMat[kMPersp1] = 0;
    fMat[kMPersp2] = kMatrix22Elem;

    this->setTypeMask(kUnknown_Mask | kOnlyPerspectiveValid_Mask);
}

void SkMatrix::setSkew(SkScalar sx, SkScalar sy) {
    fMat[kMScaleX]  = SK_Scalar1;
    fMat[kMSkewX]   = sx;
    fMat[kMTransX]  = 0;

    fMat[kMSkewY]   = sy;
    fMat[kMScaleY]  = SK_Scalar1;
    fMat[kMTransY]  = 0;

    fMat[kMPersp0] = fMat[kMPersp1] = 0;
    fMat[kMPersp2] = kMatrix22Elem;

    this->setTypeMask(kUnknown_Mask | kOnlyPerspectiveValid_Mask);
}

bool SkMatrix::preSkew(SkScalar sx, SkScalar sy, SkScalar px, SkScalar py) {
    SkMatrix    m;
    m.setSkew(sx, sy, px, py);
    return this->preConcat(m);
}

bool SkMatrix::preSkew(SkScalar sx, SkScalar sy) {
    SkMatrix    m;
    m.setSkew(sx, sy);
    return this->preConcat(m);
}

bool SkMatrix::postSkew(SkScalar sx, SkScalar sy, SkScalar px, SkScalar py) {
    SkMatrix    m;
    m.setSkew(sx, sy, px, py);
    return this->postConcat(m);
}

bool SkMatrix::postSkew(SkScalar sx, SkScalar sy) {
    SkMatrix    m;
    m.setSkew(sx, sy);
    return this->postConcat(m);
}

///////////////////////////////////////////////////////////////////////////////

bool SkMatrix::setRectToRect(const SkRect& src, const SkRect& dst,
                             ScaleToFit align)
{
    if (src.isEmpty()) {
        this->reset();
        return false;
    }

    if (dst.isEmpty()) {
        sk_bzero(fMat, 8 * sizeof(SkScalar));
        this->setTypeMask(kScale_Mask | kRectStaysRect_Mask);
    } else {
        SkScalar    tx, sx = SkScalarDiv(dst.width(), src.width());
        SkScalar    ty, sy = SkScalarDiv(dst.height(), src.height());
        bool        xLarger = false;

        if (align != kFill_ScaleToFit) {
            if (sx > sy) {
                xLarger = true;
                sx = sy;
            } else {
                sy = sx;
            }
        }

        tx = dst.fLeft - SkScalarMul(src.fLeft, sx);
        ty = dst.fTop - SkScalarMul(src.fTop, sy);
        if (align == kCenter_ScaleToFit || align == kEnd_ScaleToFit) {
            SkScalar diff;

            if (xLarger) {
                diff = dst.width() - SkScalarMul(src.width(), sy);
            } else {
                diff = dst.height() - SkScalarMul(src.height(), sy);
            }

            if (align == kCenter_ScaleToFit) {
                diff = SkScalarHalf(diff);
            }

            if (xLarger) {
                tx += diff;
            } else {
                ty += diff;
            }
        }

        fMat[kMScaleX] = sx;
        fMat[kMScaleY] = sy;
        fMat[kMTransX] = tx;
        fMat[kMTransY] = ty;
        fMat[kMSkewX]  = fMat[kMSkewY] =
        fMat[kMPersp0] = fMat[kMPersp1] = 0;

        unsigned mask = kRectStaysRect_Mask;
        if (sx != SK_Scalar1 || sy != SK_Scalar1) {
            mask |= kScale_Mask;
        }
        if (tx || ty) {
            mask |= kTranslate_Mask;
        }
        this->setTypeMask(mask);
    }
    // shared cleanup
    fMat[kMPersp2] = kMatrix22Elem;
    return true;
}

///////////////////////////////////////////////////////////////////////////////

static inline int fixmuladdmul(float a, float b, float c, float d,
                               float* result) {
    *result = SkDoubleToFloat((double)a * b + (double)c * d);
    return true;
}

static inline bool rowcol3(const float row[], const float col[],
                           float* result) {
    *result = row[0] * col[0] + row[1] * col[3] + row[2] * col[6];
    return true;
}

static inline int negifaddoverflows(float& result, float a, float b) {
    result = a + b;
    return 0;
}

static void normalize_perspective(SkScalar mat[9]) {
    if (SkScalarAbs(mat[SkMatrix::kMPersp2]) > kMatrix22Elem) {
        for (int i = 0; i < 9; i++)
            mat[i] = SkScalarHalf(mat[i]);
    }
}

bool SkMatrix::setConcat(const SkMatrix& a, const SkMatrix& b) {
    TypeMask aType = a.getPerspectiveTypeMaskOnly();
    TypeMask bType = b.getPerspectiveTypeMaskOnly();

    if (a.isTriviallyIdentity()) {
        *this = b;
    } else if (b.isTriviallyIdentity()) {
        *this = a;
    } else {
        SkMatrix tmp;

        if ((aType | bType) & kPerspective_Mask) {
            if (!rowcol3(&a.fMat[0], &b.fMat[0], &tmp.fMat[kMScaleX])) {
                return false;
            }
            if (!rowcol3(&a.fMat[0], &b.fMat[1], &tmp.fMat[kMSkewX])) {
                return false;
            }
            if (!rowcol3(&a.fMat[0], &b.fMat[2], &tmp.fMat[kMTransX])) {
                return false;
            }

            if (!rowcol3(&a.fMat[3], &b.fMat[0], &tmp.fMat[kMSkewY])) {
                return false;
            }
            if (!rowcol3(&a.fMat[3], &b.fMat[1], &tmp.fMat[kMScaleY])) {
                return false;
            }
            if (!rowcol3(&a.fMat[3], &b.fMat[2], &tmp.fMat[kMTransY])) {
                return false;
            }

            if (!rowcol3(&a.fMat[6], &b.fMat[0], &tmp.fMat[kMPersp0])) {
                return false;
            }
            if (!rowcol3(&a.fMat[6], &b.fMat[1], &tmp.fMat[kMPersp1])) {
                return false;
            }
            if (!rowcol3(&a.fMat[6], &b.fMat[2], &tmp.fMat[kMPersp2])) {
                return false;
            }

            normalize_perspective(tmp.fMat);
            tmp.setTypeMask(kUnknown_Mask);
        } else {    // not perspective
            if (!fixmuladdmul(a.fMat[kMScaleX], b.fMat[kMScaleX],
                    a.fMat[kMSkewX], b.fMat[kMSkewY], &tmp.fMat[kMScaleX])) {
                return false;
            }
            if (!fixmuladdmul(a.fMat[kMScaleX], b.fMat[kMSkewX],
                      a.fMat[kMSkewX], b.fMat[kMScaleY], &tmp.fMat[kMSkewX])) {
                return false;
            }
            if (!fixmuladdmul(a.fMat[kMScaleX], b.fMat[kMTransX],
                      a.fMat[kMSkewX], b.fMat[kMTransY], &tmp.fMat[kMTransX])) {
                return false;
            }
            if (negifaddoverflows(tmp.fMat[kMTransX], tmp.fMat[kMTransX],
                                  a.fMat[kMTransX]) < 0) {
                return false;
            }

            if (!fixmuladdmul(a.fMat[kMSkewY], b.fMat[kMScaleX],
                      a.fMat[kMScaleY], b.fMat[kMSkewY], &tmp.fMat[kMSkewY])) {
                return false;
            }
            if (!fixmuladdmul(a.fMat[kMSkewY], b.fMat[kMSkewX],
                    a.fMat[kMScaleY], b.fMat[kMScaleY], &tmp.fMat[kMScaleY])) {
                return false;
            }
            if (!fixmuladdmul(a.fMat[kMSkewY], b.fMat[kMTransX],
                     a.fMat[kMScaleY], b.fMat[kMTransY], &tmp.fMat[kMTransY])) {
                return false;
            }
            if (negifaddoverflows(tmp.fMat[kMTransY], tmp.fMat[kMTransY],
                                  a.fMat[kMTransY]) < 0) {
                return false;
            }

            tmp.fMat[kMPersp0] = tmp.fMat[kMPersp1] = 0;
            tmp.fMat[kMPersp2] = kMatrix22Elem;
            //SkDebugf("Concat mat non-persp type: %d\n", tmp.getType());
            //SkASSERT(!(tmp.getType() & kPerspective_Mask));
            tmp.setTypeMask(kUnknown_Mask | kOnlyPerspectiveValid_Mask);
        }
        *this = tmp;
    }
    return true;
}

bool SkMatrix::preConcat(const SkMatrix& mat) {
    // check for identity first, so we don't do a needless copy of ourselves
    // to ourselves inside setConcat()
    return mat.isIdentity() || this->setConcat(*this, mat);
}

bool SkMatrix::postConcat(const SkMatrix& mat) {
    // check for identity first, so we don't do a needless copy of ourselves
    // to ourselves inside setConcat()
    return mat.isIdentity() || this->setConcat(mat, *this);
}

///////////////////////////////////////////////////////////////////////////////

/*  Matrix inversion is very expensive, but also the place where keeping
    precision may be most important (here and matrix concat). Hence to avoid
    bitmap blitting artifacts when walking the inverse, we use doubles for
    the intermediate math, even though we know that is more expensive.
 */

typedef double SkDetScalar;
#define SkPerspMul(a, b)            SkScalarMul(a, b)
#define SkScalarMulShift(a, b, s)   SkDoubleToFloat((a) * (b))
static double sk_inv_determinant(const float mat[9], int isPerspective,
                                int* /* (only used in Fixed case) */) {
    double det;

    if (isPerspective) {
        det =   mat[SkMatrix::kMScaleX] * ((double)mat[SkMatrix::kMScaleY] * mat[SkMatrix::kMPersp2] - (double)mat[SkMatrix::kMTransY] * mat[SkMatrix::kMPersp1]) +
                mat[SkMatrix::kMSkewX] * ((double)mat[SkMatrix::kMTransY] * mat[SkMatrix::kMPersp0] - (double)mat[SkMatrix::kMSkewY] * mat[SkMatrix::kMPersp2]) +
                mat[SkMatrix::kMTransX] * ((double)mat[SkMatrix::kMSkewY] * mat[SkMatrix::kMPersp1] - (double)mat[SkMatrix::kMScaleY] * mat[SkMatrix::kMPersp0]);
    } else {
        det =   (double)mat[SkMatrix::kMScaleX] * mat[SkMatrix::kMScaleY] - (double)mat[SkMatrix::kMSkewX] * mat[SkMatrix::kMSkewY];
    }

    // Since the determinant is on the order of the cube of the matrix members,
    // compare to the cube of the default nearly-zero constant (although an
    // estimate of the condition number would be better if it wasn't so expensive).
    if (SkScalarNearlyZero((float)det, SK_ScalarNearlyZero * SK_ScalarNearlyZero * SK_ScalarNearlyZero)) {
        return 0;
    }
    return 1.0 / det;
}
// we declar a,b,c,d to all be doubles, because we want to perform
// double-precision muls and subtract, even though the original values are
// from the matrix, which are floats.
static float inline mul_diff_scale(double a, double b, double c, double d,
                                   double scale) {
    return SkDoubleToFloat((a * b - c * d) * scale);
}

void SkMatrix::SetAffineIdentity(SkScalar affine[6]) {
    affine[kAScaleX] = SK_Scalar1;
    affine[kASkewY] = 0;
    affine[kASkewX] = 0;
    affine[kAScaleY] = SK_Scalar1;
    affine[kATransX] = 0;
    affine[kATransY] = 0;
}

bool SkMatrix::asAffine(SkScalar affine[6]) const {
    if (this->hasPerspective()) {
        return false;
    }
    if (affine) {
        affine[kAScaleX] = this->fMat[kMScaleX];
        affine[kASkewY] = this->fMat[kMSkewY];
        affine[kASkewX] = this->fMat[kMSkewX];
        affine[kAScaleY] = this->fMat[kMScaleY];
        affine[kATransX] = this->fMat[kMTransX];
        affine[kATransY] = this->fMat[kMTransY];
    }
    return true;
}

bool SkMatrix::invertNonIdentity(SkMatrix* inv) const {
    SkASSERT(!this->isIdentity());

    TypeMask mask = this->getType();

    if (0 == (mask & ~(kScale_Mask | kTranslate_Mask))) {
        bool invertible = true;
        if (inv) {
            if (mask & kScale_Mask) {
                SkScalar invX = fMat[kMScaleX];
                SkScalar invY = fMat[kMScaleY];
                if (0 == invX || 0 == invY) {
                    return false;
                }
                invX = SkScalarInvert(invX);
                invY = SkScalarInvert(invY);

                // Must be careful when writing to inv, since it may be the
                // same memory as this.

                inv->fMat[kMSkewX] = inv->fMat[kMSkewY] =
                inv->fMat[kMPersp0] = inv->fMat[kMPersp1] = 0;

                inv->fMat[kMScaleX] = invX;
                inv->fMat[kMScaleY] = invY;
                inv->fMat[kMPersp2] = kMatrix22Elem;
                inv->fMat[kMTransX] = -SkScalarMul(fMat[kMTransX], invX);
                inv->fMat[kMTransY] = -SkScalarMul(fMat[kMTransY], invY);

                inv->setTypeMask(mask | kRectStaysRect_Mask);
            } else {
                // translate only
                inv->setTranslate(-fMat[kMTransX], -fMat[kMTransY]);
            }
        } else {    // inv is NULL, just check if we're invertible
            if (!fMat[kMScaleX] || !fMat[kMScaleY]) {
                invertible = false;
            }
        }
        return invertible;
    }

    int         isPersp = mask & kPerspective_Mask;
    int         shift;
    SkDetScalar scale = sk_inv_determinant(fMat, isPersp, &shift);

    if (scale == 0) { // underflow
        return false;
    }

    if (inv) {
        SkMatrix tmp;
        if (inv == this) {
            inv = &tmp;
        }

        if (isPersp) {
            shift = 61 - shift;
            inv->fMat[kMScaleX] = SkScalarMulShift(SkPerspMul(fMat[kMScaleY], fMat[kMPersp2]) - SkPerspMul(fMat[kMTransY], fMat[kMPersp1]), scale, shift);
            inv->fMat[kMSkewX]  = SkScalarMulShift(SkPerspMul(fMat[kMTransX], fMat[kMPersp1]) - SkPerspMul(fMat[kMSkewX],  fMat[kMPersp2]), scale, shift);
            inv->fMat[kMTransX] = SkScalarMulShift(SkScalarMul(fMat[kMSkewX], fMat[kMTransY]) - SkScalarMul(fMat[kMTransX], fMat[kMScaleY]), scale, shift);

            inv->fMat[kMSkewY]  = SkScalarMulShift(SkPerspMul(fMat[kMTransY], fMat[kMPersp0]) - SkPerspMul(fMat[kMSkewY],   fMat[kMPersp2]), scale, shift);
            inv->fMat[kMScaleY] = SkScalarMulShift(SkPerspMul(fMat[kMScaleX], fMat[kMPersp2]) - SkPerspMul(fMat[kMTransX],  fMat[kMPersp0]), scale, shift);
            inv->fMat[kMTransY] = SkScalarMulShift(SkScalarMul(fMat[kMTransX], fMat[kMSkewY]) - SkScalarMul(fMat[kMScaleX], fMat[kMTransY]), scale, shift);

            inv->fMat[kMPersp0] = SkScalarMulShift(SkScalarMul(fMat[kMSkewY], fMat[kMPersp1]) - SkScalarMul(fMat[kMScaleY], fMat[kMPersp0]), scale, shift);
            inv->fMat[kMPersp1] = SkScalarMulShift(SkScalarMul(fMat[kMSkewX], fMat[kMPersp0]) - SkScalarMul(fMat[kMScaleX], fMat[kMPersp1]), scale, shift);
            inv->fMat[kMPersp2] = SkScalarMulShift(SkScalarMul(fMat[kMScaleX], fMat[kMScaleY]) - SkScalarMul(fMat[kMSkewX], fMat[kMSkewY]), scale, shift);
        } else {   // not perspective
            inv->fMat[kMScaleX] = SkDoubleToFloat(fMat[kMScaleY] * scale);
            inv->fMat[kMSkewX] = SkDoubleToFloat(-fMat[kMSkewX] * scale);
            inv->fMat[kMTransX] = mul_diff_scale(fMat[kMSkewX], fMat[kMTransY],
                                     fMat[kMScaleY], fMat[kMTransX], scale);

            inv->fMat[kMSkewY] = SkDoubleToFloat(-fMat[kMSkewY] * scale);
            inv->fMat[kMScaleY] = SkDoubleToFloat(fMat[kMScaleX] * scale);
            inv->fMat[kMTransY] = mul_diff_scale(fMat[kMSkewY], fMat[kMTransX],
                                        fMat[kMScaleX], fMat[kMTransY], scale);

            inv->fMat[kMPersp0] = 0;
            inv->fMat[kMPersp1] = 0;
            inv->fMat[kMPersp2] = kMatrix22Elem;

        }

        inv->setTypeMask(fTypeMask);

        if (inv == &tmp) {
            *(SkMatrix*)this = tmp;
        }
    }
    return true;
}

///////////////////////////////////////////////////////////////////////////////

void SkMatrix::Identity_pts(const SkMatrix& m, SkPoint dst[],
                            const SkPoint src[], int count) {
    SkASSERT(m.getType() == 0);

    if (dst != src && count > 0)
        memcpy(dst, src, count * sizeof(SkPoint));
}

void SkMatrix::Trans_pts(const SkMatrix& m, SkPoint dst[],
                         const SkPoint src[], int count) {
    SkASSERT(m.getType() == kTranslate_Mask);

    if (count > 0) {
        SkScalar tx = m.fMat[kMTransX];
        SkScalar ty = m.fMat[kMTransY];
        do {
            dst->fY = src->fY + ty;
            dst->fX = src->fX + tx;
            src += 1;
            dst += 1;
        } while (--count);
    }
}

void SkMatrix::Scale_pts(const SkMatrix& m, SkPoint dst[],
                         const SkPoint src[], int count) {
    SkASSERT(m.getType() == kScale_Mask);

    if (count > 0) {
        SkScalar mx = m.fMat[kMScaleX];
        SkScalar my = m.fMat[kMScaleY];
        do {
            dst->fY = SkScalarMul(src->fY, my);
            dst->fX = SkScalarMul(src->fX, mx);
            src += 1;
            dst += 1;
        } while (--count);
    }
}

void SkMatrix::ScaleTrans_pts(const SkMatrix& m, SkPoint dst[],
                              const SkPoint src[], int count) {
    SkASSERT(m.getType() == (kScale_Mask | kTranslate_Mask));

    if (count > 0) {
        SkScalar mx = m.fMat[kMScaleX];
        SkScalar my = m.fMat[kMScaleY];
        SkScalar tx = m.fMat[kMTransX];
        SkScalar ty = m.fMat[kMTransY];
        do {
            dst->fY = SkScalarMulAdd(src->fY, my, ty);
            dst->fX = SkScalarMulAdd(src->fX, mx, tx);
            src += 1;
            dst += 1;
        } while (--count);
    }
}

void SkMatrix::Rot_pts(const SkMatrix& m, SkPoint dst[],
                       const SkPoint src[], int count) {
    SkASSERT((m.getType() & (kPerspective_Mask | kTranslate_Mask)) == 0);

    if (count > 0) {
        SkScalar mx = m.fMat[kMScaleX];
        SkScalar my = m.fMat[kMScaleY];
        SkScalar kx = m.fMat[kMSkewX];
        SkScalar ky = m.fMat[kMSkewY];
        do {
            SkScalar sy = src->fY;
            SkScalar sx = src->fX;
            src += 1;
            dst->fY = SkScalarMul(sx, ky) + SkScalarMul(sy, my);
            dst->fX = SkScalarMul(sx, mx) + SkScalarMul(sy, kx);
            dst += 1;
        } while (--count);
    }
}

void SkMatrix::RotTrans_pts(const SkMatrix& m, SkPoint dst[],
                            const SkPoint src[], int count) {
    SkASSERT(!m.hasPerspective());

    if (count > 0) {
        SkScalar mx = m.fMat[kMScaleX];
        SkScalar my = m.fMat[kMScaleY];
        SkScalar kx = m.fMat[kMSkewX];
        SkScalar ky = m.fMat[kMSkewY];
        SkScalar tx = m.fMat[kMTransX];
        SkScalar ty = m.fMat[kMTransY];
        do {
            SkScalar sy = src->fY;
            SkScalar sx = src->fX;
            src += 1;
            dst->fY = SkScalarMul(sx, ky) + SkScalarMulAdd(sy, my, ty);
            dst->fX = SkScalarMul(sx, mx) + SkScalarMulAdd(sy, kx, tx);
            dst += 1;
        } while (--count);
    }
}

void SkMatrix::Persp_pts(const SkMatrix& m, SkPoint dst[],
                         const SkPoint src[], int count) {
    SkASSERT(m.hasPerspective());

    if (count > 0) {
        do {
            SkScalar sy = src->fY;
            SkScalar sx = src->fX;
            src += 1;

            SkScalar x = SkScalarMul(sx, m.fMat[kMScaleX]) +
                         SkScalarMul(sy, m.fMat[kMSkewX]) + m.fMat[kMTransX];
            SkScalar y = SkScalarMul(sx, m.fMat[kMSkewY]) +
                         SkScalarMul(sy, m.fMat[kMScaleY]) + m.fMat[kMTransY];
            SkScalar z = SkScalarMul(sx, m.fMat[kMPersp0]) +
                        SkScalarMulAdd(sy, m.fMat[kMPersp1], m.fMat[kMPersp2]);
            if (z) {
                z = SkScalarFastInvert(z);
            }

            dst->fY = SkScalarMul(y, z);
            dst->fX = SkScalarMul(x, z);
            dst += 1;
        } while (--count);
    }
}

const SkMatrix::MapPtsProc SkMatrix::gMapPtsProcs[] = {
    SkMatrix::Identity_pts, SkMatrix::Trans_pts,
    SkMatrix::Scale_pts,    SkMatrix::ScaleTrans_pts,
    SkMatrix::Rot_pts,      SkMatrix::RotTrans_pts,
    SkMatrix::Rot_pts,      SkMatrix::RotTrans_pts,
    // repeat the persp proc 8 times
    SkMatrix::Persp_pts,    SkMatrix::Persp_pts,
    SkMatrix::Persp_pts,    SkMatrix::Persp_pts,
    SkMatrix::Persp_pts,    SkMatrix::Persp_pts,
    SkMatrix::Persp_pts,    SkMatrix::Persp_pts
};

void SkMatrix::mapPoints(SkPoint dst[], const SkPoint src[], int count) const {
    SkASSERT((dst && src && count > 0) || 0 == count);
    // no partial overlap
    SkASSERT(src == dst || &dst[count] <= &src[0] || &src[count] <= &dst[0]);

    this->getMapPtsProc()(*this, dst, src, count);
}

///////////////////////////////////////////////////////////////////////////////

void SkMatrix::mapHomogeneousPoints(SkScalar dst[], const SkScalar src[], int count) const {
    SkASSERT((dst && src && count > 0) || 0 == count);
    // no partial overlap
    SkASSERT(src == dst || SkAbs32((int32_t)(src - dst)) >= 3*count);

    if (count > 0) {
        if (this->isIdentity()) {
            memcpy(dst, src, 3*count*sizeof(SkScalar));
            return;
        }
        do {
            SkScalar sx = src[0];
            SkScalar sy = src[1];
            SkScalar sw = src[2];
            src += 3;

            SkScalar x = SkScalarMul(sx, fMat[kMScaleX]) +
                         SkScalarMul(sy, fMat[kMSkewX]) +
                         SkScalarMul(sw, fMat[kMTransX]);
            SkScalar y = SkScalarMul(sx, fMat[kMSkewY]) +
                         SkScalarMul(sy, fMat[kMScaleY]) +
                         SkScalarMul(sw, fMat[kMTransY]);
            SkScalar w = SkScalarMul(sx, fMat[kMPersp0]) +
                         SkScalarMul(sy, fMat[kMPersp1]) +
                         SkScalarMul(sw, fMat[kMPersp2]);

            dst[0] = x;
            dst[1] = y;
            dst[2] = w;
            dst += 3;
        } while (--count);
    }
}

///////////////////////////////////////////////////////////////////////////////

void SkMatrix::mapVectors(SkPoint dst[], const SkPoint src[], int count) const {
    if (this->hasPerspective()) {
        SkPoint origin;

        MapXYProc proc = this->getMapXYProc();
        proc(*this, 0, 0, &origin);

        for (int i = count - 1; i >= 0; --i) {
            SkPoint tmp;

            proc(*this, src[i].fX, src[i].fY, &tmp);
            dst[i].set(tmp.fX - origin.fX, tmp.fY - origin.fY);
        }
    } else {
        SkMatrix tmp = *this;

        tmp.fMat[kMTransX] = tmp.fMat[kMTransY] = 0;
        tmp.clearTypeMask(kTranslate_Mask);
        tmp.mapPoints(dst, src, count);
    }
}

bool SkMatrix::mapRect(SkRect* dst, const SkRect& src) const {
    SkASSERT(dst && &src);

    if (this->rectStaysRect()) {
        this->mapPoints((SkPoint*)dst, (const SkPoint*)&src, 2);
        dst->sort();
        return true;
    } else {
        SkPoint quad[4];

        src.toQuad(quad);
        this->mapPoints(quad, quad, 4);
        dst->set(quad, 4);
        return false;
    }
}

SkScalar SkMatrix::mapRadius(SkScalar radius) const {
    SkVector    vec[2];

    vec[0].set(radius, 0);
    vec[1].set(0, radius);
    this->mapVectors(vec, 2);

    SkScalar d0 = vec[0].length();
    SkScalar d1 = vec[1].length();

    // return geometric mean
    return SkScalarSqrt(d0 * d1);
}

///////////////////////////////////////////////////////////////////////////////

void SkMatrix::Persp_xy(const SkMatrix& m, SkScalar sx, SkScalar sy,
                        SkPoint* pt) {
    SkASSERT(m.hasPerspective());

    SkScalar x = SkScalarMul(sx, m.fMat[kMScaleX]) +
                 SkScalarMul(sy, m.fMat[kMSkewX]) + m.fMat[kMTransX];
    SkScalar y = SkScalarMul(sx, m.fMat[kMSkewY]) +
                 SkScalarMul(sy, m.fMat[kMScaleY]) + m.fMat[kMTransY];
    SkScalar z = SkScalarMul(sx, m.fMat[kMPersp0]) +
                 SkScalarMul(sy, m.fMat[kMPersp1]) + m.fMat[kMPersp2];
    if (z) {
        z = SkScalarFastInvert(z);
    }
    pt->fX = SkScalarMul(x, z);
    pt->fY = SkScalarMul(y, z);
}

void SkMatrix::RotTrans_xy(const SkMatrix& m, SkScalar sx, SkScalar sy,
                           SkPoint* pt) {
    SkASSERT((m.getType() & (kAffine_Mask | kPerspective_Mask)) == kAffine_Mask);

    pt->fX = SkScalarMul(sx, m.fMat[kMScaleX]) +
             SkScalarMulAdd(sy, m.fMat[kMSkewX], m.fMat[kMTransX]);
    pt->fY = SkScalarMul(sx, m.fMat[kMSkewY]) +
             SkScalarMulAdd(sy, m.fMat[kMScaleY], m.fMat[kMTransY]);
}

void SkMatrix::Rot_xy(const SkMatrix& m, SkScalar sx, SkScalar sy,
                      SkPoint* pt) {
    SkASSERT((m.getType() & (kAffine_Mask | kPerspective_Mask))== kAffine_Mask);
    SkASSERT(0 == m.fMat[kMTransX]);
    SkASSERT(0 == m.fMat[kMTransY]);

    pt->fX = SkScalarMul(sx, m.fMat[kMScaleX]) +
             SkScalarMulAdd(sy, m.fMat[kMSkewX], m.fMat[kMTransX]);
    pt->fY = SkScalarMul(sx, m.fMat[kMSkewY]) +
             SkScalarMulAdd(sy, m.fMat[kMScaleY], m.fMat[kMTransY]);
}

void SkMatrix::ScaleTrans_xy(const SkMatrix& m, SkScalar sx, SkScalar sy,
                             SkPoint* pt) {
    SkASSERT((m.getType() & (kScale_Mask | kAffine_Mask | kPerspective_Mask))
             == kScale_Mask);

    pt->fX = SkScalarMulAdd(sx, m.fMat[kMScaleX], m.fMat[kMTransX]);
    pt->fY = SkScalarMulAdd(sy, m.fMat[kMScaleY], m.fMat[kMTransY]);
}

void SkMatrix::Scale_xy(const SkMatrix& m, SkScalar sx, SkScalar sy,
                        SkPoint* pt) {
    SkASSERT((m.getType() & (kScale_Mask | kAffine_Mask | kPerspective_Mask))
             == kScale_Mask);
    SkASSERT(0 == m.fMat[kMTransX]);
    SkASSERT(0 == m.fMat[kMTransY]);

    pt->fX = SkScalarMul(sx, m.fMat[kMScaleX]);
    pt->fY = SkScalarMul(sy, m.fMat[kMScaleY]);
}

void SkMatrix::Trans_xy(const SkMatrix& m, SkScalar sx, SkScalar sy,
                        SkPoint* pt) {
    SkASSERT(m.getType() == kTranslate_Mask);

    pt->fX = sx + m.fMat[kMTransX];
    pt->fY = sy + m.fMat[kMTransY];
}

void SkMatrix::Identity_xy(const SkMatrix& m, SkScalar sx, SkScalar sy,
                           SkPoint* pt) {
    SkASSERT(0 == m.getType());

    pt->fX = sx;
    pt->fY = sy;
}

const SkMatrix::MapXYProc SkMatrix::gMapXYProcs[] = {
    SkMatrix::Identity_xy, SkMatrix::Trans_xy,
    SkMatrix::Scale_xy,    SkMatrix::ScaleTrans_xy,
    SkMatrix::Rot_xy,      SkMatrix::RotTrans_xy,
    SkMatrix::Rot_xy,      SkMatrix::RotTrans_xy,
    // repeat the persp proc 8 times
    SkMatrix::Persp_xy,    SkMatrix::Persp_xy,
    SkMatrix::Persp_xy,    SkMatrix::Persp_xy,
    SkMatrix::Persp_xy,    SkMatrix::Persp_xy,
    SkMatrix::Persp_xy,    SkMatrix::Persp_xy
};

///////////////////////////////////////////////////////////////////////////////

// if its nearly zero (just made up 26, perhaps it should be bigger or smaller)
#define PerspNearlyZero(x)  SkScalarNearlyZero(x, (1.0f / (1 << 26)))

bool SkMatrix::fixedStepInX(SkScalar y, SkFixed* stepX, SkFixed* stepY) const {
    if (PerspNearlyZero(fMat[kMPersp0])) {
        if (stepX || stepY) {
            if (PerspNearlyZero(fMat[kMPersp1]) &&
                    PerspNearlyZero(fMat[kMPersp2] - kMatrix22Elem)) {
                if (stepX) {
                    *stepX = SkScalarToFixed(fMat[kMScaleX]);
                }
                if (stepY) {
                    *stepY = SkScalarToFixed(fMat[kMSkewY]);
                }
            } else {
                SkScalar z = y * fMat[kMPersp1] + fMat[kMPersp2];
                if (stepX) {
                    *stepX = SkScalarToFixed(SkScalarDiv(fMat[kMScaleX], z));
                }
                if (stepY) {
                    *stepY = SkScalarToFixed(SkScalarDiv(fMat[kMSkewY], z));
                }
            }
        }
        return true;
    }
    return false;
}

///////////////////////////////////////////////////////////////////////////////

#include "SkPerspIter.h"

SkPerspIter::SkPerspIter(const SkMatrix& m, SkScalar x0, SkScalar y0, int count)
        : fMatrix(m), fSX(x0), fSY(y0), fCount(count) {
    SkPoint pt;

    SkMatrix::Persp_xy(m, x0, y0, &pt);
    fX = SkScalarToFixed(pt.fX);
    fY = SkScalarToFixed(pt.fY);
}

int SkPerspIter::next() {
    int n = fCount;

    if (0 == n) {
        return 0;
    }
    SkPoint pt;
    SkFixed x = fX;
    SkFixed y = fY;
    SkFixed dx, dy;

    if (n >= kCount) {
        n = kCount;
        fSX += SkIntToScalar(kCount);
        SkMatrix::Persp_xy(fMatrix, fSX, fSY, &pt);
        fX = SkScalarToFixed(pt.fX);
        fY = SkScalarToFixed(pt.fY);
        dx = (fX - x) >> kShift;
        dy = (fY - y) >> kShift;
    } else {
        fSX += SkIntToScalar(n);
        SkMatrix::Persp_xy(fMatrix, fSX, fSY, &pt);
        fX = SkScalarToFixed(pt.fX);
        fY = SkScalarToFixed(pt.fY);
        dx = (fX - x) / n;
        dy = (fY - y) / n;
    }

    SkFixed* p = fStorage;
    for (int i = 0; i < n; i++) {
        *p++ = x; x += dx;
        *p++ = y; y += dy;
    }

    fCount -= n;
    return n;
}

///////////////////////////////////////////////////////////////////////////////

static inline bool checkForZero(float x) {
    return x*x == 0;
}

static inline bool poly_to_point(SkPoint* pt, const SkPoint poly[], int count) {
    float   x = 1, y = 1;
    SkPoint pt1, pt2;

    if (count > 1) {
        pt1.fX = poly[1].fX - poly[0].fX;
        pt1.fY = poly[1].fY - poly[0].fY;
        y = SkPoint::Length(pt1.fX, pt1.fY);
        if (checkForZero(y)) {
            return false;
        }
        switch (count) {
            case 2:
                break;
            case 3:
                pt2.fX = poly[0].fY - poly[2].fY;
                pt2.fY = poly[2].fX - poly[0].fX;
                goto CALC_X;
            default:
                pt2.fX = poly[0].fY - poly[3].fY;
                pt2.fY = poly[3].fX - poly[0].fX;
            CALC_X:
                x = SkScalarDiv(SkScalarMul(pt1.fX, pt2.fX) +
                                SkScalarMul(pt1.fY, pt2.fY), y);
                break;
        }
    }
    pt->set(x, y);
    return true;
}

bool SkMatrix::Poly2Proc(const SkPoint srcPt[], SkMatrix* dst,
                         const SkPoint& scale) {
    float invScale = 1 / scale.fY;

    dst->fMat[kMScaleX] = (srcPt[1].fY - srcPt[0].fY) * invScale;
    dst->fMat[kMSkewY] = (srcPt[0].fX - srcPt[1].fX) * invScale;
    dst->fMat[kMPersp0] = 0;
    dst->fMat[kMSkewX] = (srcPt[1].fX - srcPt[0].fX) * invScale;
    dst->fMat[kMScaleY] = (srcPt[1].fY - srcPt[0].fY) * invScale;
    dst->fMat[kMPersp1] = 0;
    dst->fMat[kMTransX] = srcPt[0].fX;
    dst->fMat[kMTransY] = srcPt[0].fY;
    dst->fMat[kMPersp2] = 1;
    dst->setTypeMask(kUnknown_Mask);
    return true;
}

bool SkMatrix::Poly3Proc(const SkPoint srcPt[], SkMatrix* dst,
                         const SkPoint& scale) {
    float invScale = 1 / scale.fX;
    dst->fMat[kMScaleX] = (srcPt[2].fX - srcPt[0].fX) * invScale;
    dst->fMat[kMSkewY] = (srcPt[2].fY - srcPt[0].fY) * invScale;
    dst->fMat[kMPersp0] = 0;

    invScale = 1 / scale.fY;
    dst->fMat[kMSkewX] = (srcPt[1].fX - srcPt[0].fX) * invScale;
    dst->fMat[kMScaleY] = (srcPt[1].fY - srcPt[0].fY) * invScale;
    dst->fMat[kMPersp1] = 0;

    dst->fMat[kMTransX] = srcPt[0].fX;
    dst->fMat[kMTransY] = srcPt[0].fY;
    dst->fMat[kMPersp2] = 1;
    dst->setTypeMask(kUnknown_Mask);
    return true;
}

bool SkMatrix::Poly4Proc(const SkPoint srcPt[], SkMatrix* dst,
                         const SkPoint& scale) {
    float   a1, a2;
    float   x0, y0, x1, y1, x2, y2;

    x0 = srcPt[2].fX - srcPt[0].fX;
    y0 = srcPt[2].fY - srcPt[0].fY;
    x1 = srcPt[2].fX - srcPt[1].fX;
    y1 = srcPt[2].fY - srcPt[1].fY;
    x2 = srcPt[2].fX - srcPt[3].fX;
    y2 = srcPt[2].fY - srcPt[3].fY;

    /* check if abs(x2) > abs(y2) */
    if ( x2 > 0 ? y2 > 0 ? x2 > y2 : x2 > -y2 : y2 > 0 ? -x2 > y2 : x2 < y2) {
        float denom = SkScalarMulDiv(x1, y2, x2) - y1;
        if (checkForZero(denom)) {
            return false;
        }
        a1 = SkScalarDiv(SkScalarMulDiv(x0 - x1, y2, x2) - y0 + y1, denom);
    } else {
        float denom = x1 - SkScalarMulDiv(y1, x2, y2);
        if (checkForZero(denom)) {
            return false;
        }
        a1 = SkScalarDiv(x0 - x1 - SkScalarMulDiv(y0 - y1, x2, y2), denom);
    }

    /* check if abs(x1) > abs(y1) */
    if ( x1 > 0 ? y1 > 0 ? x1 > y1 : x1 > -y1 : y1 > 0 ? -x1 > y1 : x1 < y1) {
        float denom = y2 - SkScalarMulDiv(x2, y1, x1);
        if (checkForZero(denom)) {
            return false;
        }
        a2 = SkScalarDiv(y0 - y2 - SkScalarMulDiv(x0 - x2, y1, x1), denom);
    } else {
        float denom = SkScalarMulDiv(y2, x1, y1) - x2;
        if (checkForZero(denom)) {
            return false;
        }
        a2 = SkScalarDiv(SkScalarMulDiv(y0 - y2, x1, y1) - x0 + x2, denom);
    }

    float invScale = 1 / scale.fX;
    dst->fMat[kMScaleX] = SkScalarMul(SkScalarMul(a2, srcPt[3].fX) +
                                      srcPt[3].fX - srcPt[0].fX, invScale);
    dst->fMat[kMSkewY] = SkScalarMul(SkScalarMul(a2, srcPt[3].fY) +
                                     srcPt[3].fY - srcPt[0].fY, invScale);
    dst->fMat[kMPersp0] = SkScalarMul(a2, invScale);
    invScale = 1 / scale.fY;
    dst->fMat[kMSkewX] = SkScalarMul(SkScalarMul(a1, srcPt[1].fX) +
                                     srcPt[1].fX - srcPt[0].fX, invScale);
    dst->fMat[kMScaleY] = SkScalarMul(SkScalarMul(a1, srcPt[1].fY) +
                                      srcPt[1].fY - srcPt[0].fY, invScale);
    dst->fMat[kMPersp1] = SkScalarMul(a1, invScale);
    dst->fMat[kMTransX] = srcPt[0].fX;
    dst->fMat[kMTransY] = srcPt[0].fY;
    dst->fMat[kMPersp2] = 1;
    dst->setTypeMask(kUnknown_Mask);
    return true;
}

typedef bool (*PolyMapProc)(const SkPoint[], SkMatrix*, const SkPoint&);

/*  Taken from Rob Johnson's original sample code in QuickDraw GX
*/
bool SkMatrix::setPolyToPoly(const SkPoint src[], const SkPoint dst[],
                             int count) {
    if ((unsigned)count > 4) {
        SkDebugf("--- SkMatrix::setPolyToPoly count out of range %d\n", count);
        return false;
    }

    if (0 == count) {
        this->reset();
        return true;
    }
    if (1 == count) {
        this->setTranslate(dst[0].fX - src[0].fX, dst[0].fY - src[0].fY);
        return true;
    }

    SkPoint scale;
    if (!poly_to_point(&scale, src, count) ||
            SkScalarNearlyZero(scale.fX) ||
            SkScalarNearlyZero(scale.fY)) {
        return false;
    }

    static const PolyMapProc gPolyMapProcs[] = {
        SkMatrix::Poly2Proc, SkMatrix::Poly3Proc, SkMatrix::Poly4Proc
    };
    PolyMapProc proc = gPolyMapProcs[count - 2];

    SkMatrix tempMap, result;
    tempMap.setTypeMask(kUnknown_Mask);

    if (!proc(src, &tempMap, scale)) {
        return false;
    }
    if (!tempMap.invert(&result)) {
        return false;
    }
    if (!proc(dst, &tempMap, scale)) {
        return false;
    }
    if (!result.setConcat(tempMap, result)) {
        return false;
    }
    *this = result;
    return true;
}

///////////////////////////////////////////////////////////////////////////////

enum MinOrMax {
    kMin_MinOrMax,
    kMax_MinOrMax
};

template <MinOrMax MIN_OR_MAX> SkScalar get_stretch_factor(SkMatrix::TypeMask typeMask,
                                                           const SkScalar m[9]) {
    if (typeMask & SkMatrix::kPerspective_Mask) {
        return -SK_Scalar1;
    }
    if (SkMatrix::kIdentity_Mask == typeMask) {
        return SK_Scalar1;
    }
    if (!(typeMask & SkMatrix::kAffine_Mask)) {
        if (kMin_MinOrMax == MIN_OR_MAX) {
             return SkMinScalar(SkScalarAbs(m[SkMatrix::kMScaleX]),
                                SkScalarAbs(m[SkMatrix::kMScaleY]));
        } else {
             return SkMaxScalar(SkScalarAbs(m[SkMatrix::kMScaleX]),
                                SkScalarAbs(m[SkMatrix::kMScaleY]));
        }
    }
    // ignore the translation part of the matrix, just look at 2x2 portion.
    // compute singular values, take largest or smallest abs value.
    // [a b; b c] = A^T*A
    SkScalar a = SkScalarMul(m[SkMatrix::kMScaleX], m[SkMatrix::kMScaleX]) +
                 SkScalarMul(m[SkMatrix::kMSkewY],  m[SkMatrix::kMSkewY]);
    SkScalar b = SkScalarMul(m[SkMatrix::kMScaleX], m[SkMatrix::kMSkewX]) +
                 SkScalarMul(m[SkMatrix::kMScaleY], m[SkMatrix::kMSkewY]);
    SkScalar c = SkScalarMul(m[SkMatrix::kMSkewX],  m[SkMatrix::kMSkewX]) +
                 SkScalarMul(m[SkMatrix::kMScaleY], m[SkMatrix::kMScaleY]);
    // eigenvalues of A^T*A are the squared singular values of A.
    // characteristic equation is det((A^T*A) - l*I) = 0
    // l^2 - (a + c)l + (ac-b^2)
    // solve using quadratic equation (divisor is non-zero since l^2 has 1 coeff
    // and roots are guaranteed to be pos and real).
    SkScalar chosenRoot;
    SkScalar bSqd = SkScalarMul(b,b);
    // if upper left 2x2 is orthogonal save some math
    if (bSqd <= SK_ScalarNearlyZero*SK_ScalarNearlyZero) {
        if (kMin_MinOrMax == MIN_OR_MAX) {
            chosenRoot = SkMinScalar(a, c);
        } else {
            chosenRoot = SkMaxScalar(a, c);
        }
    } else {
        SkScalar aminusc = a - c;
        SkScalar apluscdiv2 = SkScalarHalf(a + c);
        SkScalar x = SkScalarHalf(SkScalarSqrt(SkScalarMul(aminusc, aminusc) + 4 * bSqd));
        if (kMin_MinOrMax == MIN_OR_MAX) {
            chosenRoot = apluscdiv2 - x;
        } else {
            chosenRoot = apluscdiv2 + x;
        }
    }
    SkASSERT(chosenRoot >= 0);
    return SkScalarSqrt(chosenRoot);
}

SkScalar SkMatrix::getMinStretch() const {
    return get_stretch_factor<kMin_MinOrMax>(this->getType(), fMat);
}

SkScalar SkMatrix::getMaxStretch() const {
    return get_stretch_factor<kMax_MinOrMax>(this->getType(), fMat);
}

static void reset_identity_matrix(SkMatrix* identity) {
    identity->reset();
}

const SkMatrix& SkMatrix::I() {
    // If you can use C++11 now, you might consider replacing this with a constexpr constructor.
    static SkMatrix gIdentity;
    SK_DECLARE_STATIC_ONCE(once);
    SkOnce(&once, reset_identity_matrix, &gIdentity);
    return gIdentity;
}

const SkMatrix& SkMatrix::InvalidMatrix() {
    static SkMatrix gInvalid;
    static bool gOnce;
    if (!gOnce) {
        gInvalid.setAll(SK_ScalarMax, SK_ScalarMax, SK_ScalarMax,
                        SK_ScalarMax, SK_ScalarMax, SK_ScalarMax,
                        SK_ScalarMax, SK_ScalarMax, SK_ScalarMax);
        gInvalid.getType(); // force the type to be computed
        gOnce = true;
    }
    return gInvalid;
}

///////////////////////////////////////////////////////////////////////////////

size_t SkMatrix::writeToMemory(void* buffer) const {
    // TODO write less for simple matrices
    static const size_t sizeInMemory = 9 * sizeof(SkScalar);
    if (buffer) {
        memcpy(buffer, fMat, sizeInMemory);
    }
    return sizeInMemory;
}

size_t SkMatrix::readFromMemory(const void* buffer, size_t length) {
    static const size_t sizeInMemory = 9 * sizeof(SkScalar);
    if (length < sizeInMemory) {
        return 0;
    }
    if (buffer) {
        memcpy(fMat, buffer, sizeInMemory);
        this->setTypeMask(kUnknown_Mask);
    }
    return sizeInMemory;
}

#ifdef SK_DEVELOPER
void SkMatrix::dump() const {
    SkString str;
    this->toString(&str);
    SkDebugf("%s\n", str.c_str());
}

void SkMatrix::toString(SkString* str) const {
    str->appendf("[%8.4f %8.4f %8.4f][%8.4f %8.4f %8.4f][%8.4f %8.4f %8.4f]",
             fMat[0], fMat[1], fMat[2], fMat[3], fMat[4], fMat[5],
             fMat[6], fMat[7], fMat[8]);
}
#endif

///////////////////////////////////////////////////////////////////////////////

#include "SkMatrixUtils.h"

bool SkTreatAsSprite(const SkMatrix& mat, int width, int height,
                     unsigned subpixelBits) {
    // quick reject on affine or perspective
    if (mat.getType() & ~(SkMatrix::kScale_Mask | SkMatrix::kTranslate_Mask)) {
        return false;
    }

    // quick success check
    if (!subpixelBits && !(mat.getType() & ~SkMatrix::kTranslate_Mask)) {
        return true;
    }

    // mapRect supports negative scales, so we eliminate those first
    if (mat.getScaleX() < 0 || mat.getScaleY() < 0) {
        return false;
    }

    SkRect dst;
    SkIRect isrc = { 0, 0, width, height };

    {
        SkRect src;
        src.set(isrc);
        mat.mapRect(&dst, src);
    }

    // just apply the translate to isrc
    isrc.offset(SkScalarRoundToInt(mat.getTranslateX()),
                SkScalarRoundToInt(mat.getTranslateY()));

    if (subpixelBits) {
        isrc.fLeft <<= subpixelBits;
        isrc.fTop <<= subpixelBits;
        isrc.fRight <<= subpixelBits;
        isrc.fBottom <<= subpixelBits;

        const float scale = 1 << subpixelBits;
        dst.fLeft *= scale;
        dst.fTop *= scale;
        dst.fRight *= scale;
        dst.fBottom *= scale;
    }

    SkIRect idst;
    dst.round(&idst);
    return isrc == idst;
}

// A square matrix M can be decomposed (via polar decomposition) into two matrices --
// an orthogonal matrix Q and a symmetric matrix S. In turn we can decompose S into U*W*U^T,
// where U is another orthogonal matrix and W is a scale matrix. These can be recombined
// to give M = (Q*U)*W*U^T, i.e., the product of two orthogonal matrices and a scale matrix.
//
// The one wrinkle is that traditionally Q may contain a reflection -- the
// calculation has been rejiggered to put that reflection into W.
bool SkDecomposeUpper2x2(const SkMatrix& matrix,
                         SkPoint* rotation1,
                         SkPoint* scale,
                         SkPoint* rotation2) {

    SkScalar A = matrix[SkMatrix::kMScaleX];
    SkScalar B = matrix[SkMatrix::kMSkewX];
    SkScalar C = matrix[SkMatrix::kMSkewY];
    SkScalar D = matrix[SkMatrix::kMScaleY];

    if (is_degenerate_2x2(A, B, C, D)) {
        return false;
    }

    double w1, w2;
    SkScalar cos1, sin1;
    SkScalar cos2, sin2;

    // do polar decomposition (M = Q*S)
    SkScalar cosQ, sinQ;
    double Sa, Sb, Sd;
    // if M is already symmetric (i.e., M = I*S)
    if (SkScalarNearlyEqual(B, C)) {
        cosQ = SK_Scalar1;
        sinQ = 0;

        Sa = A;
        Sb = B;
        Sd = D;
    } else {
        cosQ = A + D;
        sinQ = C - B;
        SkScalar reciplen = SK_Scalar1/SkScalarSqrt(cosQ*cosQ + sinQ*sinQ);
        cosQ *= reciplen;
        sinQ *= reciplen;

        // S = Q^-1*M
        // we don't calc Sc since it's symmetric
        Sa = A*cosQ + C*sinQ;
        Sb = B*cosQ + D*sinQ;
        Sd = -B*sinQ + D*cosQ;
    }

    // Now we need to compute eigenvalues of S (our scale factors)
    // and eigenvectors (bases for our rotation)
    // From this, should be able to reconstruct S as U*W*U^T
    if (SkScalarNearlyZero(SkDoubleToScalar(Sb))) {
        // already diagonalized
        cos1 = SK_Scalar1;
        sin1 = 0;
        w1 = Sa;
        w2 = Sd;
        cos2 = cosQ;
        sin2 = sinQ;
    } else {
        double diff = Sa - Sd;
        double discriminant = sqrt(diff*diff + 4.0*Sb*Sb);
        double trace = Sa + Sd;
        if (diff > 0) {
            w1 = 0.5*(trace + discriminant);
            w2 = 0.5*(trace - discriminant);
        } else {
            w1 = 0.5*(trace - discriminant);
            w2 = 0.5*(trace + discriminant);
        }

        cos1 = SkDoubleToScalar(Sb); sin1 = SkDoubleToScalar(w1 - Sa);
        SkScalar reciplen = SK_Scalar1/SkScalarSqrt(cos1*cos1 + sin1*sin1);
        cos1 *= reciplen;
        sin1 *= reciplen;

        // rotation 2 is composition of Q and U
        cos2 = cos1*cosQ - sin1*sinQ;
        sin2 = sin1*cosQ + cos1*sinQ;

        // rotation 1 is U^T
        sin1 = -sin1;
    }

    if (NULL != scale) {
        scale->fX = SkDoubleToScalar(w1);
        scale->fY = SkDoubleToScalar(w2);
    }
    if (NULL != rotation1) {
        rotation1->fX = cos1;
        rotation1->fY = sin1;
    }
    if (NULL != rotation2) {
        rotation2->fX = cos2;
        rotation2->fY = sin2;
    }

    return true;
}
