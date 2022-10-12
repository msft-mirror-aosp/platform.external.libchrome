// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/geometry/transform.h"

#include <stddef.h>

#include <limits>
#include <ostream>

#include "base/cxx17_backports.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/gfx/geometry/angle_conversions.h"
#include "ui/gfx/geometry/axis_transform2d.h"
#include "ui/gfx/geometry/box_f.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/point3_f.h"
#include "ui/gfx/geometry/quad_f.h"
#include "ui/gfx/geometry/test/geometry_util.h"
#include "ui/gfx/geometry/transform_util.h"
#include "ui/gfx/geometry/vector3d_f.h"

namespace gfx {

namespace {

#define EXPECT_ROW1_EQ(a, b, c, d, transform) \
  EXPECT_FLOAT_EQ((a), (transform).rc(0, 0)); \
  EXPECT_FLOAT_EQ((b), (transform).rc(0, 1)); \
  EXPECT_FLOAT_EQ((c), (transform).rc(0, 2)); \
  EXPECT_FLOAT_EQ((d), (transform).rc(0, 3));

#define EXPECT_ROW2_EQ(a, b, c, d, transform) \
  EXPECT_FLOAT_EQ((a), (transform).rc(1, 0)); \
  EXPECT_FLOAT_EQ((b), (transform).rc(1, 1)); \
  EXPECT_FLOAT_EQ((c), (transform).rc(1, 2)); \
  EXPECT_FLOAT_EQ((d), (transform).rc(1, 3));

#define EXPECT_ROW3_EQ(a, b, c, d, transform) \
  EXPECT_FLOAT_EQ((a), (transform).rc(2, 0)); \
  EXPECT_FLOAT_EQ((b), (transform).rc(2, 1)); \
  EXPECT_FLOAT_EQ((c), (transform).rc(2, 2)); \
  EXPECT_FLOAT_EQ((d), (transform).rc(2, 3));

#define EXPECT_ROW4_EQ(a, b, c, d, transform) \
  EXPECT_FLOAT_EQ((a), (transform).rc(3, 0)); \
  EXPECT_FLOAT_EQ((b), (transform).rc(3, 1)); \
  EXPECT_FLOAT_EQ((c), (transform).rc(3, 2)); \
  EXPECT_FLOAT_EQ((d), (transform).rc(3, 3));

// Checking float values for equality close to zero is not robust using
// EXPECT_FLOAT_EQ (see gtest documentation). So, to verify rotation matrices,
// we must use a looser absolute error threshold in some places.
#define EXPECT_ROW1_NEAR(a, b, c, d, transform, errorThreshold) \
  EXPECT_NEAR((a), (transform).rc(0, 0), (errorThreshold));     \
  EXPECT_NEAR((b), (transform).rc(0, 1), (errorThreshold));     \
  EXPECT_NEAR((c), (transform).rc(0, 2), (errorThreshold));     \
  EXPECT_NEAR((d), (transform).rc(0, 3), (errorThreshold));

#define EXPECT_ROW2_NEAR(a, b, c, d, transform, errorThreshold) \
  EXPECT_NEAR((a), (transform).rc(1, 0), (errorThreshold));     \
  EXPECT_NEAR((b), (transform).rc(1, 1), (errorThreshold));     \
  EXPECT_NEAR((c), (transform).rc(1, 2), (errorThreshold));     \
  EXPECT_NEAR((d), (transform).rc(1, 3), (errorThreshold));

#define EXPECT_ROW3_NEAR(a, b, c, d, transform, errorThreshold) \
  EXPECT_NEAR((a), (transform).rc(2, 0), (errorThreshold));     \
  EXPECT_NEAR((b), (transform).rc(2, 1), (errorThreshold));     \
  EXPECT_NEAR((c), (transform).rc(2, 2), (errorThreshold));     \
  EXPECT_NEAR((d), (transform).rc(2, 3), (errorThreshold));

bool PointsAreNearlyEqual(const PointF& lhs, const PointF& rhs) {
  return lhs.IsWithinDistance(rhs, 0.01f);
}

bool PointsAreNearlyEqual(const Point3F& lhs, const Point3F& rhs) {
  return lhs.SquaredDistanceTo(rhs) < 0.0001f;
}

bool MatricesAreNearlyEqual(const Transform& lhs, const Transform& rhs) {
  float epsilon = 0.0001f;
  for (int row = 0; row < 4; ++row) {
    for (int col = 0; col < 4; ++col) {
      if (std::abs(lhs.rc(row, col) - rhs.rc(row, col)) > epsilon)
        return false;
    }
  }
  return true;
}

void InitializeTestMatrix(Transform* transform) {
  transform->set_rc(0, 0, 10.f);
  transform->set_rc(1, 0, 11.f);
  transform->set_rc(2, 0, 12.f);
  transform->set_rc(3, 0, 13.f);
  transform->set_rc(0, 1, 14.f);
  transform->set_rc(1, 1, 15.f);
  transform->set_rc(2, 1, 16.f);
  transform->set_rc(3, 1, 17.f);
  transform->set_rc(0, 2, 18.f);
  transform->set_rc(1, 2, 19.f);
  transform->set_rc(2, 2, 20.f);
  transform->set_rc(3, 2, 21.f);
  transform->set_rc(0, 3, 22.f);
  transform->set_rc(1, 3, 23.f);
  transform->set_rc(2, 3, 24.f);
  transform->set_rc(3, 3, 25.f);

  // Sanity check
  EXPECT_ROW1_EQ(10.0f, 14.0f, 18.0f, 22.0f, (*transform));
  EXPECT_ROW2_EQ(11.0f, 15.0f, 19.0f, 23.0f, (*transform));
  EXPECT_ROW3_EQ(12.0f, 16.0f, 20.0f, 24.0f, (*transform));
  EXPECT_ROW4_EQ(13.0f, 17.0f, 21.0f, 25.0f, (*transform));
}

void InitializeTestMatrix2(Transform* transform) {
  transform->set_rc(0, 0, 30.f);
  transform->set_rc(1, 0, 31.f);
  transform->set_rc(2, 0, 32.f);
  transform->set_rc(3, 0, 33.f);
  transform->set_rc(0, 1, 34.f);
  transform->set_rc(1, 1, 35.f);
  transform->set_rc(2, 1, 36.f);
  transform->set_rc(3, 1, 37.f);
  transform->set_rc(0, 2, 38.f);
  transform->set_rc(1, 2, 39.f);
  transform->set_rc(2, 2, 40.f);
  transform->set_rc(3, 2, 41.f);
  transform->set_rc(0, 3, 42.f);
  transform->set_rc(1, 3, 43.f);
  transform->set_rc(2, 3, 44.f);
  transform->set_rc(3, 3, 45.f);

  // Sanity check
  EXPECT_ROW1_EQ(30.0f, 34.0f, 38.0f, 42.0f, (*transform));
  EXPECT_ROW2_EQ(31.0f, 35.0f, 39.0f, 43.0f, (*transform));
  EXPECT_ROW3_EQ(32.0f, 36.0f, 40.0f, 44.0f, (*transform));
  EXPECT_ROW4_EQ(33.0f, 37.0f, 41.0f, 45.0f, (*transform));
}

const SkScalar kApproxZero = std::numeric_limits<float>::epsilon();
const SkScalar kApproxOne = 1 - kApproxZero;

void InitializeApproxIdentityMatrix(Transform* transform) {
  transform->set_rc(0, 0, kApproxOne);
  transform->set_rc(0, 1, kApproxZero);
  transform->set_rc(0, 2, kApproxZero);
  transform->set_rc(0, 3, kApproxZero);

  transform->set_rc(1, 0, kApproxZero);
  transform->set_rc(1, 1, kApproxOne);
  transform->set_rc(1, 2, kApproxZero);
  transform->set_rc(1, 3, kApproxZero);

  transform->set_rc(2, 0, kApproxZero);
  transform->set_rc(2, 1, kApproxZero);
  transform->set_rc(2, 2, kApproxOne);
  transform->set_rc(2, 3, kApproxZero);

  transform->set_rc(3, 0, kApproxZero);
  transform->set_rc(3, 1, kApproxZero);
  transform->set_rc(3, 2, kApproxZero);
  transform->set_rc(3, 3, kApproxOne);
}

#define ERROR_THRESHOLD 1e-7
#define LOOSE_ERROR_THRESHOLD 1e-7

TEST(XFormTest, Equality) {
  Transform lhs, interpolated;
  auto rhs = Transform::RowMajor(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14,
                                 15, 16);
  interpolated = lhs;
  for (int i = 0; i <= 100; ++i) {
    for (int row = 0; row < 4; ++row) {
      for (int col = 0; col < 4; ++col) {
        float a = lhs.rc(row, col);
        float b = rhs.rc(row, col);
        float t = i / 100.0f;
        interpolated.set_rc(row, col, a + (b - a) * t);
      }
    }
    if (i == 100) {
      EXPECT_TRUE(rhs == interpolated);
    } else {
      EXPECT_TRUE(rhs != interpolated);
    }
  }
  lhs = Transform();
  rhs = Transform();
  for (int i = 1; i < 100; ++i) {
    lhs.MakeIdentity();
    rhs.MakeIdentity();
    lhs.Translate(i, i);
    rhs.Translate(-i, -i);
    EXPECT_TRUE(lhs != rhs);
    rhs.Translate(2 * i, 2 * i);
    EXPECT_TRUE(lhs == rhs);
  }
}

// This test is to make it easier to understand the order of operations.
TEST(XFormTest, PrePostOperations) {
  auto m1 = Transform::AffineForTesting(1, 2, 3, 4, 5, 6);
  auto m2 = m1;
  m1.Translate(10, 20);
  m2.PreConcat(Transform::MakeTranslation(10, 20));
  EXPECT_EQ(m1, m2);

  m1.PostTranslate(11, 22);
  m2.PostConcat(Transform::MakeTranslation(11, 22));
  EXPECT_EQ(m1, m2);

  m1.Scale(3, 4);
  m2.PreConcat(Transform::MakeScale(3, 4));
  EXPECT_EQ(m1, m2);

  m1.PostScale(5, 6);
  m2.PostConcat(Transform::MakeScale(5, 6));
  EXPECT_EQ(m1, m2);
}

TEST(XFormTest, ConcatTranslate) {
  static const struct TestCase {
    int x1;
    int y1;
    float tx;
    float ty;
    int x2;
    int y2;
  } test_cases[] = {
      {0, 0, 10.0f, 20.0f, 10, 20},
      {0, 0, -10.0f, -20.0f, 0, 0},
      {0, 0, -10.0f, -20.0f, -10, -20},
      {0, 0, std::numeric_limits<float>::quiet_NaN(),
       std::numeric_limits<float>::quiet_NaN(), 10, 20},
  };

  Transform xform;
  for (const auto& value : test_cases) {
    Transform translation;
    translation.Translate(value.tx, value.ty);
    xform = translation * xform;
    Point3F p1 = xform.MapPoint(Point3F(value.x1, value.y1, 0));
    Point3F p2(value.x2, value.y2, 0);
    if (value.tx == value.tx && value.ty == value.ty) {
      EXPECT_TRUE(PointsAreNearlyEqual(p1, p2));
    }
  }
}

TEST(XFormTest, ConcatScale) {
  static const struct TestCase {
    int before;
    float scale;
    int after;
  } test_cases[] = {{1, 10.0f, 10},
                    {1, .1f, 1},
                    {1, 100.0f, 100},
                    {1, -1.0f, -100},
                    {1, std::numeric_limits<float>::quiet_NaN(), 1}};

  Transform xform;
  for (const auto& value : test_cases) {
    Transform scale;
    scale.Scale(value.scale, value.scale);
    xform = scale * xform;
    Point3F p1 = xform.MapPoint(Point3F(value.before, value.before, 0));
    Point3F p2(value.after, value.after, 0);
    if (value.scale == value.scale) {
      EXPECT_TRUE(PointsAreNearlyEqual(p1, p2));
    }
  }
}

TEST(XFormTest, ConcatRotate) {
  static const struct TestCase {
    int x1;
    int y1;
    float degrees;
    int x2;
    int y2;
  } test_cases[] = {{1, 0, 90.0f, 0, 1},
                    {1, 0, -90.0f, 1, 0},
                    {1, 0, 90.0f, 0, 1},
                    {1, 0, 360.0f, 0, 1},
                    {1, 0, 0.0f, 0, 1},
                    {1, 0, std::numeric_limits<float>::quiet_NaN(), 1, 0}};

  Transform xform;
  for (const auto& value : test_cases) {
    Transform rotation;
    rotation.Rotate(value.degrees);
    xform = rotation * xform;
    Point3F p1 = xform.MapPoint(Point3F(value.x1, value.y1, 0));
    Point3F p2(value.x2, value.y2, 0);
    if (value.degrees == value.degrees) {
      EXPECT_TRUE(PointsAreNearlyEqual(p1, p2));
    }
  }
}

TEST(XFormTest, SetTranslate) {
  static const struct TestCase {
    int x1;
    int y1;
    float tx;
    float ty;
    int x2;
    int y2;
  } test_cases[] = {{0, 0, 10.0f, 20.0f, 10, 20},
                    {10, 20, 10.0f, 20.0f, 20, 40},
                    {10, 20, 0.0f, 0.0f, 10, 20},
                    {0, 0, std::numeric_limits<float>::quiet_NaN(),
                     std::numeric_limits<float>::quiet_NaN(), 0, 0}};

  for (const auto& value : test_cases) {
    for (int k = 0; k < 3; ++k) {
      Point3F p0, p1, p2;
      Transform xform;
      switch (k) {
        case 0:
          p1.SetPoint(value.x1, 0, 0);
          p2.SetPoint(value.x2, 0, 0);
          xform.Translate(value.tx, 0.0);
          break;
        case 1:
          p1.SetPoint(0, value.y1, 0);
          p2.SetPoint(0, value.y2, 0);
          xform.Translate(0.0, value.ty);
          break;
        case 2:
          p1.SetPoint(value.x1, value.y1, 0);
          p2.SetPoint(value.x2, value.y2, 0);
          xform.Translate(value.tx, value.ty);
          break;
      }
      p0 = p1;
      p1 = xform.MapPoint(p1);
      if (value.tx == value.tx && value.ty == value.ty) {
        EXPECT_TRUE(PointsAreNearlyEqual(p1, p2));
        const absl::optional<Point3F> transformed_p1 =
            xform.InverseMapPoint(p1);
        ASSERT_TRUE(transformed_p1.has_value());
        EXPECT_TRUE(PointsAreNearlyEqual(transformed_p1.value(), p0));
      }
    }
  }
}

TEST(XFormTest, SetScale) {
  static const struct TestCase {
    int before;
    float s;
    int after;
  } test_cases[] = {
      {1, 10.0f, 10},
      {1, 1.0f, 1},
      {1, 0.0f, 0},
      {0, 10.0f, 0},
      {1, std::numeric_limits<float>::quiet_NaN(), 0},
  };

  for (const auto& value : test_cases) {
    for (int k = 0; k < 3; ++k) {
      Point3F p0, p1, p2;
      Transform xform;
      switch (k) {
        case 0:
          p1.SetPoint(value.before, 0, 0);
          p2.SetPoint(value.after, 0, 0);
          xform.Scale(value.s, 1.0);
          break;
        case 1:
          p1.SetPoint(0, value.before, 0);
          p2.SetPoint(0, value.after, 0);
          xform.Scale(1.0, value.s);
          break;
        case 2:
          p1.SetPoint(value.before, value.before, 0);
          p2.SetPoint(value.after, value.after, 0);
          xform.Scale(value.s, value.s);
          break;
      }
      p0 = p1;
      p1 = xform.MapPoint(p1);
      if (value.s == value.s) {
        EXPECT_TRUE(PointsAreNearlyEqual(p1, p2));
        if (value.s != 0.0f) {
          const absl::optional<Point3F> transformed_p1 =
              xform.InverseMapPoint(p1);
          ASSERT_TRUE(transformed_p1.has_value());
          EXPECT_TRUE(PointsAreNearlyEqual(transformed_p1.value(), p0));
        }
      }
    }
  }
}

TEST(XFormTest, SetRotate) {
  static const struct SetRotateCase {
    int x;
    int y;
    float degree;
    int xprime;
    int yprime;
  } set_rotate_cases[] = {{100, 0, 90.0f, 0, 100},
                          {0, 0, 90.0f, 0, 0},
                          {0, 100, 90.0f, -100, 0},
                          {0, 1, -90.0f, 1, 0},
                          {100, 0, 0.0f, 100, 0},
                          {0, 0, 0.0f, 0, 0},
                          {0, 0, std::numeric_limits<float>::quiet_NaN(), 0, 0},
                          {100, 0, 360.0f, 100, 0}};

  for (const auto& value : set_rotate_cases) {
    Point3F p0;
    Point3F p1(value.x, value.y, 0);
    Point3F p2(value.xprime, value.yprime, 0);
    p0 = p1;
    Transform xform;
    xform.Rotate(value.degree);
    // just want to make sure that we don't crash in the case of NaN.
    if (value.degree == value.degree) {
      p1 = xform.MapPoint(p1);
      EXPECT_TRUE(PointsAreNearlyEqual(p1, p2));
      const absl::optional<Point3F> transformed_p1 = xform.InverseMapPoint(p1);
      ASSERT_TRUE(transformed_p1.has_value());
      EXPECT_TRUE(PointsAreNearlyEqual(transformed_p1.value(), p0));
    }
  }
}

// 2D tests
TEST(XFormTest, ConcatTranslate2D) {
  static const struct TestCase {
    int x1;
    int y1;
    float tx;
    float ty;
    int x2;
    int y2;
  } test_cases[] = {
      {0, 0, 10.0f, 20.0f, 10, 20},
      {0, 0, -10.0f, -20.0f, 0, 0},
      {0, 0, -10.0f, -20.0f, -10, -20},
  };

  Transform xform;
  for (const auto& value : test_cases) {
    Transform translation;
    translation.Translate(value.tx, value.ty);
    xform = translation * xform;
    Point p1 = xform.MapPoint(Point(value.x1, value.y1));
    Point p2(value.x2, value.y2);
    if (value.tx == value.tx && value.ty == value.ty) {
      EXPECT_EQ(p1.x(), p2.x());
      EXPECT_EQ(p1.y(), p2.y());
    }
  }
}

TEST(XFormTest, ConcatScale2D) {
  static const struct TestCase {
    int before;
    float scale;
    int after;
  } test_cases[] = {
      {1, 10.0f, 10},
      {1, .1f, 1},
      {1, 100.0f, 100},
      {1, -1.0f, -100},
  };

  Transform xform;
  for (const auto& value : test_cases) {
    Transform scale;
    scale.Scale(value.scale, value.scale);
    xform = scale * xform;
    Point p1 = xform.MapPoint(Point(value.before, value.before));
    Point p2(value.after, value.after);
    if (value.scale == value.scale) {
      EXPECT_EQ(p1.x(), p2.x());
      EXPECT_EQ(p1.y(), p2.y());
    }
  }
}

TEST(XFormTest, ConcatRotate2D) {
  static const struct TestCase {
    int x1;
    int y1;
    float degrees;
    int x2;
    int y2;
  } test_cases[] = {
      {1, 0, 90.0f, 0, 1},  {1, 0, -90.0f, 1, 0}, {1, 0, 90.0f, 0, 1},
      {1, 0, 360.0f, 0, 1}, {1, 0, 0.0f, 0, 1},
  };

  Transform xform;
  for (const auto& value : test_cases) {
    Transform rotation;
    rotation.Rotate(value.degrees);
    xform = rotation * xform;
    Point p1 = xform.MapPoint(Point(value.x1, value.y1));
    Point p2(value.x2, value.y2);
    if (value.degrees == value.degrees) {
      EXPECT_EQ(p1.x(), p2.x());
      EXPECT_EQ(p1.y(), p2.y());
    }
  }
}

TEST(XFormTest, SetTranslate2D) {
  static const struct TestCase {
    int x1;
    int y1;
    float tx;
    float ty;
    int x2;
    int y2;
  } test_cases[] = {
      {0, 0, 10.0f, 20.0f, 10, 20},
      {10, 20, 10.0f, 20.0f, 20, 40},
      {10, 20, 0.0f, 0.0f, 10, 20},
  };

  for (const auto& value : test_cases) {
    for (int j = -1; j < 2; ++j) {
      for (int k = 0; k < 3; ++k) {
        float epsilon = 0.0001f;
        Point p0, p1, p2;
        Transform xform;
        switch (k) {
          case 0:
            p1.SetPoint(value.x1, 0);
            p2.SetPoint(value.x2, 0);
            xform.Translate(value.tx + j * epsilon, 0.0);
            break;
          case 1:
            p1.SetPoint(0, value.y1);
            p2.SetPoint(0, value.y2);
            xform.Translate(0.0, value.ty + j * epsilon);
            break;
          case 2:
            p1.SetPoint(value.x1, value.y1);
            p2.SetPoint(value.x2, value.y2);
            xform.Translate(value.tx + j * epsilon, value.ty + j * epsilon);
            break;
        }
        p0 = p1;
        p1 = xform.MapPoint(p1);
        if (value.tx == value.tx && value.ty == value.ty) {
          EXPECT_EQ(p1.x(), p2.x());
          EXPECT_EQ(p1.y(), p2.y());
          const absl::optional<Point> transformed_p1 =
              xform.InverseMapPoint(p1);
          ASSERT_TRUE(transformed_p1.has_value());
          EXPECT_EQ(transformed_p1->x(), p0.x());
          EXPECT_EQ(transformed_p1->y(), p0.y());
        }
      }
    }
  }
}

TEST(XFormTest, SetScale2D) {
  static const struct TestCase {
    int before;
    float s;
    int after;
  } test_cases[] = {
      {1, 10.0f, 10},
      {1, 1.0f, 1},
      {1, 0.0f, 0},
      {0, 10.0f, 0},
  };

  for (const auto& value : test_cases) {
    for (int j = -1; j < 2; ++j) {
      for (int k = 0; k < 3; ++k) {
        float epsilon = 0.0001f;
        Point p0, p1, p2;
        Transform xform;
        switch (k) {
          case 0:
            p1.SetPoint(value.before, 0);
            p2.SetPoint(value.after, 0);
            xform.Scale(value.s + j * epsilon, 1.0);
            break;
          case 1:
            p1.SetPoint(0, value.before);
            p2.SetPoint(0, value.after);
            xform.Scale(1.0, value.s + j * epsilon);
            break;
          case 2:
            p1.SetPoint(value.before, value.before);
            p2.SetPoint(value.after, value.after);
            xform.Scale(value.s + j * epsilon, value.s + j * epsilon);
            break;
        }
        p0 = p1;
        p1 = xform.MapPoint(p1);
        if (value.s == value.s) {
          EXPECT_EQ(p1.x(), p2.x());
          EXPECT_EQ(p1.y(), p2.y());
          if (value.s != 0.0f) {
            const absl::optional<Point> transformed_p1 =
                xform.InverseMapPoint(p1);
            ASSERT_TRUE(transformed_p1.has_value());
            EXPECT_EQ(transformed_p1->x(), p0.x());
            EXPECT_EQ(transformed_p1->y(), p0.y());
          }
        }
      }
    }
  }
}

TEST(XFormTest, SetRotate2D) {
  static const struct SetRotateCase {
    int x;
    int y;
    float degree;
    int xprime;
    int yprime;
  } set_rotate_cases[] = {{100, 0, 90.0f, 0, 100},
                          {0, 0, 90.0f, 0, 0},
                          {0, 100, 90.0f, -100, 0},
                          {0, 1, -90.0f, 1, 0},
                          {100, 0, 0.0f, 100, 0},
                          {0, 0, 0.0f, 0, 0},
                          {0, 0, std::numeric_limits<float>::quiet_NaN(), 0, 0},
                          {100, 0, 360.0f, 100, 0}};

  for (const auto& value : set_rotate_cases) {
    for (int j = 1; j >= -1; --j) {
      float epsilon = 0.1f;
      Point pt(value.x, value.y);
      Transform xform;
      // should be invariant to small floating point errors.
      xform.Rotate(value.degree + j * epsilon);
      // just want to make sure that we don't crash in the case of NaN.
      if (value.degree == value.degree) {
        pt = xform.MapPoint(pt);
        EXPECT_EQ(value.xprime, pt.x());
        EXPECT_EQ(value.yprime, pt.y());
        const absl::optional<Point> transformed_pt = xform.InverseMapPoint(pt);
        ASSERT_TRUE(transformed_pt.has_value());
        EXPECT_EQ(transformed_pt->x(), value.x);
        EXPECT_EQ(transformed_pt->y(), value.y);
      }
    }
  }
}

TEST(XFormTest, MapPointWithExtremePerspective) {
  Point3F point(1.f, 1.f, 1.f);
  Transform perspective;
  perspective.ApplyPerspectiveDepth(1.f);
  Point3F transformed = perspective.MapPoint(point);
  EXPECT_EQ(point.ToString(), transformed.ToString());

  perspective.MakeIdentity();
  perspective.ApplyPerspectiveDepth(1.1f);
  transformed = perspective.MapPoint(point);
  EXPECT_FLOAT_EQ(11.f, transformed.x());
  EXPECT_FLOAT_EQ(11.f, transformed.y());
  EXPECT_FLOAT_EQ(11.f, transformed.z());
}

TEST(XFormTest, BlendTranslate) {
  Transform from;
  for (int i = -5; i < 15; ++i) {
    Transform to;
    to.Translate3d(1, 1, 1);
    double t = i / 9.0;
    EXPECT_TRUE(to.Blend(from, t));
    EXPECT_FLOAT_EQ(t, to.rc(0, 3));
    EXPECT_FLOAT_EQ(t, to.rc(1, 3));
    EXPECT_FLOAT_EQ(t, to.rc(2, 3));
  }
}

TEST(XFormTest, BlendRotate) {
  Vector3dF axes[] = {Vector3dF(1, 0, 0), Vector3dF(0, 1, 0),
                      Vector3dF(0, 0, 1), Vector3dF(1, 1, 1)};
  Transform from;
  for (const auto& axis : axes) {
    for (int i = -5; i < 15; ++i) {
      Transform to;
      to.RotateAbout(axis, 90);
      double t = i / 9.0;
      EXPECT_TRUE(to.Blend(from, t));

      Transform expected;
      expected.RotateAbout(axis, 90 * t);

      EXPECT_TRUE(MatricesAreNearlyEqual(expected, to));
    }
  }
}

TEST(XFormTest, CanBlend180DegreeRotation) {
  Vector3dF axes[] = {Vector3dF(1, 0, 0), Vector3dF(0, 1, 0),
                      Vector3dF(0, 0, 1), Vector3dF(1, 1, 1)};
  Transform from;
  for (const auto& axis : axes) {
    for (int i = -5; i < 15; ++i) {
      Transform to;
      to.RotateAbout(axis, 180.0);
      double t = i / 9.0;
      EXPECT_TRUE(to.Blend(from, t));

      // A 180 degree rotation is exactly opposite on the sphere, therefore
      // either great circle arc to it is equivalent (and numerical precision
      // will determine which is closer).  Test both directions.
      Transform expected1;
      expected1.RotateAbout(axis, 180.0 * t);
      Transform expected2;
      expected2.RotateAbout(axis, -180.0 * t);

      EXPECT_TRUE(MatricesAreNearlyEqual(expected1, to) ||
                  MatricesAreNearlyEqual(expected2, to))
          << "axis: " << axis.ToString() << ", i: " << i;
    }
  }
}

TEST(XFormTest, BlendScale) {
  Transform from;
  for (int i = -5; i < 15; ++i) {
    Transform to;
    to.Scale3d(5, 4, 3);
    double s1 = i / 9.0;
    double s2 = 1 - s1;
    EXPECT_TRUE(to.Blend(from, s1));
    EXPECT_FLOAT_EQ(5 * s1 + s2, to.rc(0, 0)) << "i: " << i;
    EXPECT_FLOAT_EQ(4 * s1 + s2, to.rc(1, 1)) << "i: " << i;
    EXPECT_FLOAT_EQ(3 * s1 + s2, to.rc(2, 2)) << "i: " << i;
  }
}

TEST(XFormTest, BlendSkew) {
  Transform from;
  for (int i = 0; i < 2; ++i) {
    Transform to;
    to.Skew(10, 5);
    double t = i;
    Transform expected;
    expected.Skew(t * 10, t * 5);
    EXPECT_TRUE(to.Blend(from, t));
    EXPECT_TRUE(MatricesAreNearlyEqual(expected, to));
  }
}

TEST(XFormTest, ExtrapolateSkew) {
  Transform from;
  for (int i = -1; i < 2; ++i) {
    Transform to;
    to.Skew(20, 0);
    double t = i;
    Transform expected;
    expected.Skew(t * 20, t * 0);
    EXPECT_TRUE(to.Blend(from, t));
    EXPECT_TRUE(MatricesAreNearlyEqual(expected, to));
  }
}

TEST(XFormTest, BlendPerspective) {
  Transform from;
  from.ApplyPerspectiveDepth(200);
  for (int i = -1; i < 3; ++i) {
    Transform to;
    to.ApplyPerspectiveDepth(800);
    double t = i;
    double depth = 1.0 / ((1.0 / 200) * (1.0 - t) + (1.0 / 800) * t);
    Transform expected;
    expected.ApplyPerspectiveDepth(depth);
    EXPECT_TRUE(to.Blend(from, t));
    EXPECT_TRUE(MatricesAreNearlyEqual(expected, to));
  }
}

TEST(XFormTest, BlendIdentity) {
  Transform from;
  Transform to;
  EXPECT_TRUE(to.Blend(from, 0.5));
  EXPECT_EQ(to, from);
}

TEST(XFormTest, CannotBlendSingularMatrix) {
  Transform from;
  Transform to;
  to.set_rc(1, 1, 0);
  EXPECT_FALSE(to.Blend(from, 0.5));
}

TEST(XFormTest, VerifyBlendForTranslation) {
  Transform from;
  from.Translate3d(100.0, 200.0, 100.0);

  Transform to;

  to.Translate3d(200.0, 100.0, 300.0);
  to.Blend(from, 0.0);
  EXPECT_EQ(from, to);

  to = Transform();
  to.Translate3d(200.0, 100.0, 300.0);
  to.Blend(from, 0.25);
  EXPECT_ROW1_EQ(1.0f, 0.0f, 0.0f, 125.0f, to);
  EXPECT_ROW2_EQ(0.0f, 1.0f, 0.0f, 175.0f, to);
  EXPECT_ROW3_EQ(0.0f, 0.0f, 1.0f, 150.0f, to);
  EXPECT_ROW4_EQ(0.0f, 0.0f, 0.0f, 1.0f, to);

  to = Transform();
  to.Translate3d(200.0, 100.0, 300.0);
  to.Blend(from, 0.5);
  EXPECT_ROW1_EQ(1.0f, 0.0f, 0.0f, 150.0f, to);
  EXPECT_ROW2_EQ(0.0f, 1.0f, 0.0f, 150.0f, to);
  EXPECT_ROW3_EQ(0.0f, 0.0f, 1.0f, 200.0f, to);
  EXPECT_ROW4_EQ(0.0f, 0.0f, 0.0f, 1.0f, to);

  to = Transform();
  to.Translate3d(200.0, 100.0, 300.0);
  to.Blend(from, 1.0);
  EXPECT_ROW1_EQ(1.0f, 0.0f, 0.0f, 200.0f, to);
  EXPECT_ROW2_EQ(0.0f, 1.0f, 0.0f, 100.0f, to);
  EXPECT_ROW3_EQ(0.0f, 0.0f, 1.0f, 300.0f, to);
  EXPECT_ROW4_EQ(0.0f, 0.0f, 0.0f, 1.0f, to);
}

TEST(XFormTest, VerifyBlendForScale) {
  Transform from;
  from.Scale3d(100.0, 200.0, 100.0);

  Transform to;

  to.Scale3d(200.0, 100.0, 300.0);
  to.Blend(from, 0.0);
  EXPECT_EQ(from, to);

  to = Transform();
  to.Scale3d(200.0, 100.0, 300.0);
  to.Blend(from, 0.25);
  EXPECT_ROW1_EQ(125.0f, 0.0f, 0.0f, 0.0f, to);
  EXPECT_ROW2_EQ(0.0f, 175.0f, 0.0f, 0.0f, to);
  EXPECT_ROW3_EQ(0.0f, 0.0f, 150.0f, 0.0f, to);
  EXPECT_ROW4_EQ(0.0f, 0.0f, 0.0f, 1.0f, to);

  to = Transform();
  to.Scale3d(200.0, 100.0, 300.0);
  to.Blend(from, 0.5);
  EXPECT_ROW1_EQ(150.0f, 0.0f, 0.0f, 0.0f, to);
  EXPECT_ROW2_EQ(0.0f, 150.0f, 0.0f, 0.0f, to);
  EXPECT_ROW3_EQ(0.0f, 0.0f, 200.0f, 0.0f, to);
  EXPECT_ROW4_EQ(0.0f, 0.0f, 0.0f, 1.0f, to);

  to = Transform();
  to.Scale3d(200.0, 100.0, 300.0);
  to.Blend(from, 1.0);
  EXPECT_ROW1_EQ(200.0f, 0.0f, 0.0f, 0.0f, to);
  EXPECT_ROW2_EQ(0.0f, 100.0f, 0.0f, 0.0f, to);
  EXPECT_ROW3_EQ(0.0f, 0.0f, 300.0f, 0.0f, to);
  EXPECT_ROW4_EQ(0.0f, 0.0f, 0.0f, 1.0f, to);
}

TEST(XFormTest, VerifyBlendForSkew) {
  // Along X axis only
  Transform from;
  from.Skew(0.0, 0.0);

  Transform to;

  to.Skew(45.0, 0.0);
  to.Blend(from, 0.0);
  EXPECT_EQ(from, to);

  to = Transform();
  to.Skew(45.0, 0.0);
  to.Blend(from, 0.5);
  EXPECT_ROW1_EQ(1.0f, 0.5f, 0.0f, 0.0f, to);
  EXPECT_ROW2_EQ(0.0f, 1.0f, 0.0f, 0.0f, to);
  EXPECT_ROW3_EQ(0.0f, 0.0f, 1.0f, 0.0f, to);
  EXPECT_ROW4_EQ(0.0f, 0.0f, 0.0f, 1.0f, to);

  to = Transform();
  to.Skew(45.0, 0.0);
  to.Blend(from, 0.25);
  EXPECT_ROW1_EQ(1.0f, 0.25f, 0.0f, 0.0f, to);
  EXPECT_ROW2_EQ(0.0f, 1.0f, 0.0f, 0.0f, to);
  EXPECT_ROW3_EQ(0.0f, 0.0f, 1.0f, 0.0f, to);
  EXPECT_ROW4_EQ(0.0f, 0.0f, 0.0f, 1.0f, to);

  to = Transform();
  to.Skew(45.0, 0.0);
  to.Blend(from, 1.0);
  EXPECT_ROW1_EQ(1.0f, 1.0f, 0.0f, 0.0f, to);
  EXPECT_ROW2_EQ(0.0f, 1.0f, 0.0f, 0.0f, to);
  EXPECT_ROW3_EQ(0.0f, 0.0f, 1.0f, 0.0f, to);
  EXPECT_ROW4_EQ(0.0f, 0.0f, 0.0f, 1.0f, to);

  // NOTE CAREFULLY: Decomposition of skew and rotation terms of the matrix
  // is inherently underconstrained, and so it does not always compute the
  // originally intended skew parameters. The current implementation uses QR
  // decomposition, which decomposes the shear into a rotation + non-uniform
  // scale.
  //
  // It is unlikely that the decomposition implementation will need to change
  // very often, so to get any test coverage, the compromise is to verify the
  // exact matrix that the.Blend() operation produces.
  //
  // This problem also potentially exists for skew along the X axis, but the
  // current QR decomposition implementation just happens to decompose those
  // test matrices intuitively.
  //
  // Unfortunately, this case suffers from uncomfortably large precision
  // error.

  from = Transform();
  from.Skew(0.0, 0.0);

  to = Transform();

  to.Skew(0.0, 45.0);
  to.Blend(from, 0.0);
  EXPECT_EQ(from, to);

  to = Transform();
  to.Skew(0.0, 45.0);
  to.Blend(from, 0.25);
  EXPECT_LT(1.0, to.rc(0, 0));
  EXPECT_GT(1.5, to.rc(0, 0));
  EXPECT_LT(0.0, to.rc(0, 1));
  EXPECT_GT(0.5, to.rc(0, 1));
  EXPECT_FLOAT_EQ(0.0, to.rc(0, 2));
  EXPECT_FLOAT_EQ(0.0, to.rc(0, 3));

  EXPECT_LT(0.0, to.rc(1, 0));
  EXPECT_GT(0.5, to.rc(1, 0));
  EXPECT_LT(0.0, to.rc(1, 1));
  EXPECT_GT(1.0, to.rc(1, 1));
  EXPECT_FLOAT_EQ(0.0, to.rc(1, 2));
  EXPECT_FLOAT_EQ(0.0, to.rc(1, 3));

  EXPECT_ROW3_EQ(0.0f, 0.0f, 1.0f, 0.0f, to);
  EXPECT_ROW4_EQ(0.0f, 0.0f, 0.0f, 1.0f, to);

  to = Transform();
  to.Skew(0.0, 45.0);
  to.Blend(from, 0.5);

  EXPECT_LT(1.0, to.rc(0, 0));
  EXPECT_GT(1.5, to.rc(0, 0));
  EXPECT_LT(0.0, to.rc(0, 1));
  EXPECT_GT(0.5, to.rc(0, 1));
  EXPECT_FLOAT_EQ(0.0, to.rc(0, 2));
  EXPECT_FLOAT_EQ(0.0, to.rc(0, 3));

  EXPECT_LT(0.0, to.rc(1, 0));
  EXPECT_GT(1.0, to.rc(1, 0));
  EXPECT_LT(0.0, to.rc(1, 1));
  EXPECT_GT(1.0, to.rc(1, 1));
  EXPECT_FLOAT_EQ(0.0, to.rc(1, 2));
  EXPECT_FLOAT_EQ(0.0, to.rc(1, 3));

  EXPECT_ROW3_EQ(0.0f, 0.0f, 1.0f, 0.0f, to);
  EXPECT_ROW4_EQ(0.0f, 0.0f, 0.0f, 1.0f, to);

  to = Transform();
  to.Skew(0.0, 45.0);
  to.Blend(from, 1.0);
  EXPECT_ROW1_NEAR(1.0, 0.0, 0.0, 0.0, to, LOOSE_ERROR_THRESHOLD);
  EXPECT_ROW2_NEAR(1.0, 1.0, 0.0, 0.0, to, LOOSE_ERROR_THRESHOLD);
  EXPECT_ROW3_EQ(0.0f, 0.0f, 1.0f, 0.0f, to);
  EXPECT_ROW4_EQ(0.0f, 0.0f, 0.0f, 1.0f, to);
}

TEST(XFormTest, VerifyBlendForRotationAboutX) {
  // Even though.Blending uses quaternions, axis-aligned rotations should.
  // Blend the same with quaternions or Euler angles. So we can test
  // rotation.Blending by comparing against manually specified matrices from
  // Euler angles.

  Transform from;
  from.RotateAbout(Vector3dF(1.0, 0.0, 0.0), 0.0);

  Transform to;

  to.RotateAbout(Vector3dF(1.0, 0.0, 0.0), 90.0);
  to.Blend(from, 0.0);
  EXPECT_EQ(from, to);

  double expectedRotationAngle = gfx::DegToRad(22.5);
  to = Transform();
  to.RotateAbout(Vector3dF(1.0, 0.0, 0.0), 90.0);
  to.Blend(from, 0.25);
  EXPECT_ROW1_NEAR(1.0, 0.0, 0.0, 0.0, to, ERROR_THRESHOLD);
  EXPECT_ROW2_NEAR(0.0, std::cos(expectedRotationAngle),
                   -std::sin(expectedRotationAngle), 0.0, to, ERROR_THRESHOLD);
  EXPECT_ROW3_NEAR(0.0, std::sin(expectedRotationAngle),
                   std::cos(expectedRotationAngle), 0.0, to, ERROR_THRESHOLD);
  EXPECT_ROW4_EQ(0.0f, 0.0f, 0.0f, 1.0f, to);

  expectedRotationAngle = gfx::DegToRad(45.0);
  to = Transform();
  to.RotateAbout(Vector3dF(1.0, 0.0, 0.0), 90.0);
  to.Blend(from, 0.5);
  EXPECT_ROW1_NEAR(1.0, 0.0, 0.0, 0.0, to, ERROR_THRESHOLD);
  EXPECT_ROW2_NEAR(0.0, std::cos(expectedRotationAngle),
                   -std::sin(expectedRotationAngle), 0.0, to, ERROR_THRESHOLD);
  EXPECT_ROW3_NEAR(0.0, std::sin(expectedRotationAngle),
                   std::cos(expectedRotationAngle), 0.0, to, ERROR_THRESHOLD);
  EXPECT_ROW4_EQ(0.0f, 0.0f, 0.0f, 1.0f, to);

  to = Transform();
  to.RotateAbout(Vector3dF(1.0, 0.0, 0.0), 90.0);
  to.Blend(from, 1.0);
  EXPECT_ROW1_NEAR(1.0, 0.0, 0.0, 0.0, to, ERROR_THRESHOLD);
  EXPECT_ROW2_NEAR(0.0, 0.0, -1.0, 0.0, to, ERROR_THRESHOLD);
  EXPECT_ROW3_NEAR(0.0, 1.0, 0.0, 0.0, to, ERROR_THRESHOLD);
  EXPECT_ROW4_EQ(0.0f, 0.0f, 0.0f, 1.0f, to);
}

TEST(XFormTest, VerifyBlendForRotationAboutY) {
  Transform from;
  from.RotateAbout(Vector3dF(0.0, 1.0, 0.0), 0.0);

  Transform to;

  to.RotateAbout(Vector3dF(0.0, 1.0, 0.0), 90.0);
  to.Blend(from, 0.0);
  EXPECT_EQ(from, to);

  double expectedRotationAngle = gfx::DegToRad(22.5);
  to = Transform();
  to.RotateAbout(Vector3dF(0.0, 1.0, 0.0), 90.0);
  to.Blend(from, 0.25);
  EXPECT_ROW1_NEAR(std::cos(expectedRotationAngle), 0.0,
                   std::sin(expectedRotationAngle), 0.0, to, ERROR_THRESHOLD);
  EXPECT_ROW2_NEAR(0.0, 1.0, 0.0, 0.0, to, ERROR_THRESHOLD);
  EXPECT_ROW3_NEAR(-std::sin(expectedRotationAngle), 0.0,
                   std::cos(expectedRotationAngle), 0.0, to, ERROR_THRESHOLD);
  EXPECT_ROW4_EQ(0.0f, 0.0f, 0.0f, 1.0f, to);

  expectedRotationAngle = gfx::DegToRad(45.0);
  to = Transform();
  to.RotateAbout(Vector3dF(0.0, 1.0, 0.0), 90.0);
  to.Blend(from, 0.5);
  EXPECT_ROW1_NEAR(std::cos(expectedRotationAngle), 0.0,
                   std::sin(expectedRotationAngle), 0.0, to, ERROR_THRESHOLD);
  EXPECT_ROW2_NEAR(0.0, 1.0, 0.0, 0.0, to, ERROR_THRESHOLD);
  EXPECT_ROW3_NEAR(-std::sin(expectedRotationAngle), 0.0,
                   std::cos(expectedRotationAngle), 0.0, to, ERROR_THRESHOLD);
  EXPECT_ROW4_EQ(0.0f, 0.0f, 0.0f, 1.0f, to);

  to = Transform();
  to.RotateAbout(Vector3dF(0.0, 1.0, 0.0), 90.0);
  to.Blend(from, 1.0);
  EXPECT_ROW1_NEAR(0.0, 0.0, 1.0, 0.0, to, ERROR_THRESHOLD);
  EXPECT_ROW2_NEAR(0.0, 1.0, 0.0, 0.0, to, ERROR_THRESHOLD);
  EXPECT_ROW3_NEAR(-1.0, 0.0, 0.0, 0.0, to, ERROR_THRESHOLD);
  EXPECT_ROW4_EQ(0.0f, 0.0f, 0.0f, 1.0f, to);
}

TEST(XFormTest, VerifyBlendForRotationAboutZ) {
  Transform from;
  from.RotateAbout(Vector3dF(0.0, 0.0, 1.0), 0.0);

  Transform to;

  to.RotateAbout(Vector3dF(0.0, 0.0, 1.0), 90.0);
  to.Blend(from, 0.0);
  EXPECT_EQ(from, to);

  double expectedRotationAngle = gfx::DegToRad(22.5);
  to = Transform();
  to.RotateAbout(Vector3dF(0.0, 0.0, 1.0), 90.0);
  to.Blend(from, 0.25);
  EXPECT_ROW1_NEAR(std::cos(expectedRotationAngle),
                   -std::sin(expectedRotationAngle), 0.0, 0.0, to,
                   ERROR_THRESHOLD);
  EXPECT_ROW2_NEAR(std::sin(expectedRotationAngle),
                   std::cos(expectedRotationAngle), 0.0, 0.0, to,
                   ERROR_THRESHOLD);
  EXPECT_ROW3_NEAR(0.0, 0.0, 1.0, 0.0, to, ERROR_THRESHOLD);
  EXPECT_ROW4_EQ(0.0f, 0.0f, 0.0f, 1.0f, to);

  expectedRotationAngle = gfx::DegToRad(45.0);
  to = Transform();
  to.RotateAbout(Vector3dF(0.0, 0.0, 1.0), 90.0);
  to.Blend(from, 0.5);
  EXPECT_ROW1_NEAR(std::cos(expectedRotationAngle),
                   -std::sin(expectedRotationAngle), 0.0, 0.0, to,
                   ERROR_THRESHOLD);
  EXPECT_ROW2_NEAR(std::sin(expectedRotationAngle),
                   std::cos(expectedRotationAngle), 0.0, 0.0, to,
                   ERROR_THRESHOLD);
  EXPECT_ROW3_NEAR(0.0, 0.0, 1.0, 0.0, to, ERROR_THRESHOLD);
  EXPECT_ROW4_EQ(0.0f, 0.0f, 0.0f, 1.0f, to);

  to = Transform();
  to.RotateAbout(Vector3dF(0.0, 0.0, 1.0), 90.0);
  to.Blend(from, 1.0);
  EXPECT_ROW1_NEAR(0.0, -1.0, 0.0, 0.0, to, ERROR_THRESHOLD);
  EXPECT_ROW2_NEAR(1.0, 0.0, 0.0, 0.0, to, ERROR_THRESHOLD);
  EXPECT_ROW3_NEAR(0.0, 0.0, 1.0, 0.0, to, ERROR_THRESHOLD);
  EXPECT_ROW4_EQ(0.0f, 0.0f, 0.0f, 1.0f, to);
}

TEST(XFormTest, VerifyBlendForCompositeTransform) {
  // Verify that the.Blending was done with a decomposition in correct order
  // by blending a composite transform. Using matrix x vector notation
  // (Ax = b, where x is column vector), the ordering should be:
  // perspective * translation * rotation * skew * scale
  //
  // It is not as important (or meaningful) to check intermediate
  // interpolations; order of operations will be tested well enough by the
  // end cases that are easier to specify.

  Transform from;
  Transform to;

  Transform expectedEndOfAnimation;
  expectedEndOfAnimation.ApplyPerspectiveDepth(1.0);
  expectedEndOfAnimation.Translate3d(10.0, 20.0, 30.0);
  expectedEndOfAnimation.RotateAbout(Vector3dF(0.0, 0.0, 1.0), 25.0);
  expectedEndOfAnimation.Skew(0.0, 45.0);
  expectedEndOfAnimation.Scale3d(6.0, 7.0, 8.0);

  to = expectedEndOfAnimation;
  to.Blend(from, 0.0);
  EXPECT_EQ(from, to);

  to = expectedEndOfAnimation;
  // We short circuit if blend is >= 1, so to check the numerics, we will
  // check that we get close to what we expect when we're nearly done
  // interpolating.
  to.Blend(from, .99999f);

  // Recomposing the matrix results in a normalized matrix, so to verify we
  // need to normalize the expectedEndOfAnimation before comparing elements.
  // Normalizing means dividing everything by expectedEndOfAnimation.m44().
  Transform normalizedExpectedEndOfAnimation = expectedEndOfAnimation;
  Transform normalizationMatrix;
  normalizationMatrix.set_rc(
      0.0, 0.0, SkDoubleToScalar(1 / expectedEndOfAnimation.rc(3.0, 3.0)));
  normalizationMatrix.set_rc(
      1.0, 1.0, SkDoubleToScalar(1 / expectedEndOfAnimation.rc(3.0, 3.0)));
  normalizationMatrix.set_rc(
      2.0, 2.0, SkDoubleToScalar(1 / expectedEndOfAnimation.rc(3.0, 3.0)));
  normalizationMatrix.set_rc(
      3.0, 3.0, SkDoubleToScalar(1 / expectedEndOfAnimation.rc(3.0, 3.0)));
  normalizedExpectedEndOfAnimation.PreConcat(normalizationMatrix);

  EXPECT_TRUE(MatricesAreNearlyEqual(normalizedExpectedEndOfAnimation, to));
}

TEST(XFormTest, DecomposedTransformCtor) {
  DecomposedTransform decomp;
  for (int i = 0; i < 3; ++i) {
    EXPECT_EQ(0.0, decomp.translate[i]);
    EXPECT_EQ(1.0, decomp.scale[i]);
    EXPECT_EQ(0.0, decomp.skew[i]);
    EXPECT_EQ(0.0, decomp.perspective[i]);
  }
  EXPECT_EQ(1.0, decomp.perspective[3]);

  EXPECT_EQ(0.0, decomp.quaternion.x());
  EXPECT_EQ(0.0, decomp.quaternion.y());
  EXPECT_EQ(0.0, decomp.quaternion.z());
  EXPECT_EQ(1.0, decomp.quaternion.w());

  Transform identity;
  Transform composed = ComposeTransform(decomp);
  EXPECT_TRUE(MatricesAreNearlyEqual(identity, composed));
}

TEST(XFormTest, FactorTRS) {
  for (int degrees = 0; degrees < 180; ++degrees) {
    // build a transformation matrix.
    gfx::Transform transform;
    transform.Translate(degrees * 2, -degrees * 3);
    transform.Rotate(degrees);
    transform.Scale(degrees + 1, 2 * degrees + 1);

    // factor the matrix
    DecomposedTransform decomp;
    bool success = DecomposeTransform(&decomp, transform);
    EXPECT_TRUE(success);
    EXPECT_FLOAT_EQ(decomp.translate[0], degrees * 2);
    EXPECT_FLOAT_EQ(decomp.translate[1], -degrees * 3);
    double rotation =
        gfx::RadToDeg(std::acos(double{decomp.quaternion.w()}) * 2);
    while (rotation < 0.0)
      rotation += 360.0;
    while (rotation > 360.0)
      rotation -= 360.0;

    const float epsilon = 0.00015f;
    EXPECT_NEAR(rotation, degrees, epsilon);
    EXPECT_NEAR(decomp.scale[0], degrees + 1, epsilon);
    EXPECT_NEAR(decomp.scale[1], 2 * degrees + 1, epsilon);
  }
}

TEST(XFormTest, DecomposeTransform) {
  for (float scale = 0.001f; scale < 2.0f; scale += 0.001f) {
    gfx::Transform transform;
    transform.Scale(scale, scale);
    EXPECT_TRUE(transform.Preserves2dAxisAlignment());

    DecomposedTransform decomp;
    bool success = DecomposeTransform(&decomp, transform);
    EXPECT_TRUE(success);

    gfx::Transform compose_transform = ComposeTransform(decomp);
    EXPECT_TRUE(compose_transform.Preserves2dAxisAlignment());
  }
}

TEST(XFormTest, IntegerTranslation) {
  gfx::Transform transform;
  EXPECT_TRUE(transform.IsIdentityOrIntegerTranslation());

  transform.Translate3d(1, 2, 3);
  EXPECT_TRUE(transform.IsIdentityOrIntegerTranslation());

  transform.MakeIdentity();
  transform.Translate3d(-1, -2, -3);
  EXPECT_TRUE(transform.IsIdentityOrIntegerTranslation());

  transform.MakeIdentity();
  transform.Translate3d(4.5f, 0, 0);
  EXPECT_FALSE(transform.IsIdentityOrIntegerTranslation());

  transform.MakeIdentity();
  transform.Translate3d(0, -6.7f, 0);
  EXPECT_FALSE(transform.IsIdentityOrIntegerTranslation());

  transform.MakeIdentity();
  transform.Translate3d(0, 0, 8.9f);
  EXPECT_FALSE(transform.IsIdentityOrIntegerTranslation());

  float max_int = static_cast<float>(std::numeric_limits<int>::max());
  transform.MakeIdentity();
  transform.Translate3d(0, 0, max_int + 1000.5f);
  EXPECT_FALSE(transform.IsIdentityOrIntegerTranslation());

  float max_float = std::numeric_limits<float>::max();
  transform.MakeIdentity();
  transform.Translate3d(0, 0, max_float - 0.5f);
  EXPECT_FALSE(transform.IsIdentityOrIntegerTranslation());
}

TEST(XFormTest, verifyMatrixInversion) {
  {
    // Invert a translation
    gfx::Transform translation;
    translation.Translate3d(2.0, 3.0, 4.0);
    EXPECT_TRUE(translation.IsInvertible());

    gfx::Transform inverse_translation;
    bool is_invertible = translation.GetInverse(&inverse_translation);
    EXPECT_TRUE(is_invertible);
    EXPECT_ROW1_EQ(1.0f, 0.0f, 0.0f, -2.0f, inverse_translation);
    EXPECT_ROW2_EQ(0.0f, 1.0f, 0.0f, -3.0f, inverse_translation);
    EXPECT_ROW3_EQ(0.0f, 0.0f, 1.0f, -4.0f, inverse_translation);
    EXPECT_ROW4_EQ(0.0f, 0.0f, 0.0f, 1.0f, inverse_translation);
  }

  {
    // Invert a non-uniform scale
    gfx::Transform scale;
    scale.Scale3d(4.0, 10.0, 100.0);
    EXPECT_TRUE(scale.IsInvertible());

    gfx::Transform inverse_scale;
    bool is_invertible = scale.GetInverse(&inverse_scale);
    EXPECT_TRUE(is_invertible);
    EXPECT_ROW1_EQ(0.25f, 0.0f, 0.0f, 0.0f, inverse_scale);
    EXPECT_ROW2_EQ(0.0f, 0.1f, 0.0f, 0.0f, inverse_scale);
    EXPECT_ROW3_EQ(0.0f, 0.0f, 0.01f, 0.0f, inverse_scale);
    EXPECT_ROW4_EQ(0.0f, 0.0f, 0.0f, 1.0f, inverse_scale);
  }

  {
    // Try to invert a matrix that is not invertible.
    // The inverse() function should reset the output matrix to identity.
    gfx::Transform uninvertible;
    uninvertible.set_rc(0, 0, 0.f);
    uninvertible.set_rc(1, 1, 0.f);
    uninvertible.set_rc(2, 2, 0.f);
    uninvertible.set_rc(3, 3, 0.f);
    EXPECT_FALSE(uninvertible.IsInvertible());

    gfx::Transform inverse_of_uninvertible;

    // Add a scale just to more easily ensure that inverse_of_uninvertible is
    // reset to identity.
    inverse_of_uninvertible.Scale3d(4.0, 10.0, 100.0);

    bool is_invertible = uninvertible.GetInverse(&inverse_of_uninvertible);
    EXPECT_FALSE(is_invertible);
    EXPECT_TRUE(inverse_of_uninvertible.IsIdentity());
    EXPECT_ROW1_EQ(1.0f, 0.0f, 0.0f, 0.0f, inverse_of_uninvertible);
    EXPECT_ROW2_EQ(0.0f, 1.0f, 0.0f, 0.0f, inverse_of_uninvertible);
    EXPECT_ROW3_EQ(0.0f, 0.0f, 1.0f, 0.0f, inverse_of_uninvertible);
    EXPECT_ROW4_EQ(0.0f, 0.0f, 0.0f, 1.0f, inverse_of_uninvertible);
  }
}

TEST(XFormTest, verifyBackfaceVisibilityBasicCases) {
  Transform transform;

  transform.MakeIdentity();
  EXPECT_FALSE(transform.IsBackFaceVisible());

  transform.MakeIdentity();
  transform.RotateAboutYAxis(80.0);
  EXPECT_FALSE(transform.IsBackFaceVisible());

  transform.MakeIdentity();
  transform.RotateAboutYAxis(100.0);
  EXPECT_TRUE(transform.IsBackFaceVisible());

  // Edge case, 90 degree rotation should return false.
  transform.MakeIdentity();
  transform.RotateAboutYAxis(90.0);
  EXPECT_FALSE(transform.IsBackFaceVisible());
}

TEST(XFormTest, verifyBackfaceVisibilityForPerspective) {
  Transform layer_space_to_projection_plane;

  // This tests if IsBackFaceVisible works properly under perspective
  // transforms.  Specifically, layers that may have their back face visible in
  // orthographic projection, may not actually have back face visible under
  // perspective projection.

  // Case 1: Layer is rotated by slightly more than 90 degrees, at the center
  //         of the perspective projection. In this case, the layer's back-side
  //         is visible to the camera.
  layer_space_to_projection_plane.MakeIdentity();
  layer_space_to_projection_plane.ApplyPerspectiveDepth(1.0);
  layer_space_to_projection_plane.Translate3d(0.0, 0.0, 0.0);
  layer_space_to_projection_plane.RotateAboutYAxis(100.0);
  EXPECT_TRUE(layer_space_to_projection_plane.IsBackFaceVisible());

  // Case 2: Layer is rotated by slightly more than 90 degrees, but shifted off
  //         to the side of the camera. Because of the wide field-of-view, the
  //         layer's front side is still visible.
  //
  //                       |<-- front side of layer is visible to camera
  //                    \  |            /
  //                     \ |           /
  //                      \|          /
  //                       |         /
  //                       |\       /<-- camera field of view
  //                       | \     /
  // back side of layer -->|  \   /
  //                           \./ <-- camera origin
  //
  layer_space_to_projection_plane.MakeIdentity();
  layer_space_to_projection_plane.ApplyPerspectiveDepth(1.0);
  layer_space_to_projection_plane.Translate3d(-10.0, 0.0, 0.0);
  layer_space_to_projection_plane.RotateAboutYAxis(100.0);
  EXPECT_FALSE(layer_space_to_projection_plane.IsBackFaceVisible());

  // Case 3: Additionally rotating the layer by 180 degrees should of course
  //         show the opposite result of case 2.
  layer_space_to_projection_plane.RotateAboutYAxis(180.0);
  EXPECT_TRUE(layer_space_to_projection_plane.IsBackFaceVisible());
}

TEST(XFormTest, verifyDefaultConstructorCreatesIdentityMatrix) {
  Transform A;
  EXPECT_ROW1_EQ(1.0f, 0.0f, 0.0f, 0.0f, A);
  EXPECT_ROW2_EQ(0.0f, 1.0f, 0.0f, 0.0f, A);
  EXPECT_ROW3_EQ(0.0f, 0.0f, 1.0f, 0.0f, A);
  EXPECT_ROW4_EQ(0.0f, 0.0f, 0.0f, 1.0f, A);
  EXPECT_TRUE(A.IsIdentity());
}

TEST(XFormTest, verifyCopyConstructor) {
  Transform A;
  InitializeTestMatrix(&A);

  // Copy constructor should produce exact same elements as matrix A.
  Transform B(A);
  EXPECT_ROW1_EQ(10.0f, 14.0f, 18.0f, 22.0f, B);
  EXPECT_ROW2_EQ(11.0f, 15.0f, 19.0f, 23.0f, B);
  EXPECT_ROW3_EQ(12.0f, 16.0f, 20.0f, 24.0f, B);
  EXPECT_ROW4_EQ(13.0f, 17.0f, 21.0f, 25.0f, B);
}

TEST(XFormTest, verifyConstructorFor16Elements) {
  auto transform =
      Transform::RowMajor(1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0, 9.0, 10.0,
                          11.0, 12.0, 13.0, 14.0, 15.0, 16.0);

  EXPECT_ROW1_EQ(1.0f, 2.0f, 3.0f, 4.0f, transform);
  EXPECT_ROW2_EQ(5.0f, 6.0f, 7.0f, 8.0f, transform);
  EXPECT_ROW3_EQ(9.0f, 10.0f, 11.0f, 12.0f, transform);
  EXPECT_ROW4_EQ(13.0f, 14.0f, 15.0f, 16.0f, transform);
}

TEST(XFormTest, verifyConstructorFor2dElements) {
  Transform transform =
      Transform::AffineForTesting(1.0, 2.0, 3.0, 4.0, 5.0, 6.0);

  EXPECT_ROW1_EQ(1.0f, 2.0f, 0.0f, 5.0f, transform);
  EXPECT_ROW2_EQ(3.0f, 4.0f, 0.0f, 6.0f, transform);
  EXPECT_ROW3_EQ(0.0f, 0.0f, 1.0f, 0.0f, transform);
  EXPECT_ROW4_EQ(0.0f, 0.0f, 0.0f, 1.0f, transform);
}

TEST(XFormTest, ColMajorF) {
  float data[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};
  auto transform = Transform::ColMajorF(data);

  EXPECT_ROW1_EQ(1.0f, 5.0f, 9.0f, 13.0f, transform);
  EXPECT_ROW2_EQ(2.0f, 6.0f, 10.0f, 14.0f, transform);
  EXPECT_ROW3_EQ(3.0f, 7.0f, 11.0f, 15.0f, transform);
  EXPECT_ROW4_EQ(4.0f, 8.0f, 12.0f, 16.0f, transform);

  float data1[16];
  transform.GetColMajorF(data1);
  EXPECT_EQ(transform, Transform::ColMajorF(data1));
}

TEST(XFormTest, FromQuaternion) {
  Transform t(Quaternion(1, 2, 3, 4));
  EXPECT_ROW1_EQ(-25.f, -20.f, 22.f, 0.f, t);
  EXPECT_ROW2_EQ(28.f, -19.f, 4.f, 0.f, t);
  EXPECT_ROW3_EQ(-10.f, 20.f, -9.f, 0.f, t);
  EXPECT_ROW4_EQ(0.f, 0.f, 0.f, 1.f, t);
}

TEST(XFormTest, verifyAssignmentOperator) {
  Transform A;
  InitializeTestMatrix(&A);
  Transform B;
  InitializeTestMatrix2(&B);
  Transform C;
  InitializeTestMatrix2(&C);
  C = B = A;

  // Both B and C should now have been re-assigned to the value of A.
  EXPECT_ROW1_EQ(10.0f, 14.0f, 18.0f, 22.0f, B);
  EXPECT_ROW2_EQ(11.0f, 15.0f, 19.0f, 23.0f, B);
  EXPECT_ROW3_EQ(12.0f, 16.0f, 20.0f, 24.0f, B);
  EXPECT_ROW4_EQ(13.0f, 17.0f, 21.0f, 25.0f, B);

  EXPECT_ROW1_EQ(10.0f, 14.0f, 18.0f, 22.0f, C);
  EXPECT_ROW2_EQ(11.0f, 15.0f, 19.0f, 23.0f, C);
  EXPECT_ROW3_EQ(12.0f, 16.0f, 20.0f, 24.0f, C);
  EXPECT_ROW4_EQ(13.0f, 17.0f, 21.0f, 25.0f, C);
}

TEST(XFormTest, verifyEqualsBooleanOperator) {
  Transform A;
  InitializeTestMatrix(&A);

  Transform B;
  InitializeTestMatrix(&B);
  EXPECT_TRUE(A == B);

  // Modifying multiple elements should cause equals operator to return false.
  Transform C;
  InitializeTestMatrix2(&C);
  EXPECT_FALSE(A == C);

  // Modifying any one individual element should cause equals operator to
  // return false.
  Transform D;
  D = A;
  D.set_rc(0, 0, 0.f);
  EXPECT_FALSE(A == D);

  D = A;
  D.set_rc(1, 0, 0.f);
  EXPECT_FALSE(A == D);

  D = A;
  D.set_rc(2, 0, 0.f);
  EXPECT_FALSE(A == D);

  D = A;
  D.set_rc(3, 0, 0.f);
  EXPECT_FALSE(A == D);

  D = A;
  D.set_rc(0, 1, 0.f);
  EXPECT_FALSE(A == D);

  D = A;
  D.set_rc(1, 1, 0.f);
  EXPECT_FALSE(A == D);

  D = A;
  D.set_rc(2, 1, 0.f);
  EXPECT_FALSE(A == D);

  D = A;
  D.set_rc(3, 1, 0.f);
  EXPECT_FALSE(A == D);

  D = A;
  D.set_rc(0, 2, 0.f);
  EXPECT_FALSE(A == D);

  D = A;
  D.set_rc(1, 2, 0.f);
  EXPECT_FALSE(A == D);

  D = A;
  D.set_rc(2, 2, 0.f);
  EXPECT_FALSE(A == D);

  D = A;
  D.set_rc(3, 2, 0.f);
  EXPECT_FALSE(A == D);

  D = A;
  D.set_rc(0, 3, 0.f);
  EXPECT_FALSE(A == D);

  D = A;
  D.set_rc(1, 3, 0.f);
  EXPECT_FALSE(A == D);

  D = A;
  D.set_rc(2, 3, 0.f);
  EXPECT_FALSE(A == D);

  D = A;
  D.set_rc(3, 3, 0.f);
  EXPECT_FALSE(A == D);
}

TEST(XFormTest, verifyMultiplyOperator) {
  Transform A;
  InitializeTestMatrix(&A);

  Transform B;
  InitializeTestMatrix2(&B);

  Transform C = A * B;
  EXPECT_ROW1_EQ(2036.0f, 2292.0f, 2548.0f, 2804.0f, C);
  EXPECT_ROW2_EQ(2162.0f, 2434.0f, 2706.0f, 2978.0f, C);
  EXPECT_ROW3_EQ(2288.0f, 2576.0f, 2864.0f, 3152.0f, C);
  EXPECT_ROW4_EQ(2414.0f, 2718.0f, 3022.0f, 3326.0f, C);

  // Just an additional sanity check; matrix multiplication is not commutative.
  EXPECT_FALSE(A * B == B * A);
}

TEST(XFormTest, verifyMultiplyAndAssignOperator) {
  Transform A;
  InitializeTestMatrix(&A);

  Transform B;
  InitializeTestMatrix2(&B);

  A *= B;
  EXPECT_ROW1_EQ(2036.0f, 2292.0f, 2548.0f, 2804.0f, A);
  EXPECT_ROW2_EQ(2162.0f, 2434.0f, 2706.0f, 2978.0f, A);
  EXPECT_ROW3_EQ(2288.0f, 2576.0f, 2864.0f, 3152.0f, A);
  EXPECT_ROW4_EQ(2414.0f, 2718.0f, 3022.0f, 3326.0f, A);

  // Just an additional sanity check; matrix multiplication is not commutative.
  Transform C = A;
  C *= B;
  Transform D = B;
  D *= A;
  EXPECT_FALSE(C == D);
}

TEST(XFormTest, verifyMatrixMultiplication) {
  Transform A;
  InitializeTestMatrix(&A);

  Transform B;
  InitializeTestMatrix2(&B);

  A.PreConcat(B);
  EXPECT_ROW1_EQ(2036.0f, 2292.0f, 2548.0f, 2804.0f, A);
  EXPECT_ROW2_EQ(2162.0f, 2434.0f, 2706.0f, 2978.0f, A);
  EXPECT_ROW3_EQ(2288.0f, 2576.0f, 2864.0f, 3152.0f, A);
  EXPECT_ROW4_EQ(2414.0f, 2718.0f, 3022.0f, 3326.0f, A);
}

TEST(XFormTest, verifyMakeIdentiy) {
  Transform A;
  InitializeTestMatrix(&A);
  A.MakeIdentity();
  EXPECT_ROW1_EQ(1.0f, 0.0f, 0.0f, 0.0f, A);
  EXPECT_ROW2_EQ(0.0f, 1.0f, 0.0f, 0.0f, A);
  EXPECT_ROW3_EQ(0.0f, 0.0f, 1.0f, 0.0f, A);
  EXPECT_ROW4_EQ(0.0f, 0.0f, 0.0f, 1.0f, A);
  EXPECT_TRUE(A.IsIdentity());
}

TEST(XFormTest, verifyTranslate) {
  Transform A;
  A.Translate(2.0, 3.0);
  EXPECT_ROW1_EQ(1.0f, 0.0f, 0.0f, 2.0f, A);
  EXPECT_ROW2_EQ(0.0f, 1.0f, 0.0f, 3.0f, A);
  EXPECT_ROW3_EQ(0.0f, 0.0f, 1.0f, 0.0f, A);
  EXPECT_ROW4_EQ(0.0f, 0.0f, 0.0f, 1.0f, A);

  // Verify that Translate() post-multiplies the existing matrix.
  A.MakeIdentity();
  A.Scale(5.0, 5.0);
  A.Translate(2.0, 3.0);
  EXPECT_ROW1_EQ(5.0f, 0.0f, 0.0f, 10.0f, A);
  EXPECT_ROW2_EQ(0.0f, 5.0f, 0.0f, 15.0f, A);
  EXPECT_ROW3_EQ(0.0f, 0.0f, 1.0f, 0.0f, A);
  EXPECT_ROW4_EQ(0.0f, 0.0f, 0.0f, 1.0f, A);

  Transform B;
  B.Scale(5.0, 5.0);
  B.Translate(Vector2dF(2.0f, 3.0f));
  EXPECT_EQ(A, B);
}

TEST(XFormTest, verifyPostTranslate) {
  Transform A;
  A.PostTranslate(2.0, 3.0);
  EXPECT_ROW1_EQ(1.0f, 0.0f, 0.0f, 2.0f, A);
  EXPECT_ROW2_EQ(0.0f, 1.0f, 0.0f, 3.0f, A);
  EXPECT_ROW3_EQ(0.0f, 0.0f, 1.0f, 0.0f, A);
  EXPECT_ROW4_EQ(0.0f, 0.0f, 0.0f, 1.0f, A);

  // Verify that PostTranslate() pre-multiplies the existing matrix.
  A.MakeIdentity();
  A.Scale(5.0, 5.0);
  A.PostTranslate(2.0, 3.0);
  EXPECT_ROW1_EQ(5.0f, 0.0f, 0.0f, 2.0f, A);
  EXPECT_ROW2_EQ(0.0f, 5.0f, 0.0f, 3.0f, A);
  EXPECT_ROW3_EQ(0.0f, 0.0f, 1.0f, 0.0f, A);
  EXPECT_ROW4_EQ(0.0f, 0.0f, 0.0f, 1.0f, A);

  Transform B;
  B.Scale(5.0, 5.0);
  B.PostTranslate(Vector2dF(2.0f, 3.0f));
  EXPECT_EQ(A, B);
}

TEST(XFormTest, verifyTranslate3d) {
  Transform A;
  A.Translate3d(2.0, 3.0, 4.0);
  EXPECT_ROW1_EQ(1.0f, 0.0f, 0.0f, 2.0f, A);
  EXPECT_ROW2_EQ(0.0f, 1.0f, 0.0f, 3.0f, A);
  EXPECT_ROW3_EQ(0.0f, 0.0f, 1.0f, 4.0f, A);
  EXPECT_ROW4_EQ(0.0f, 0.0f, 0.0f, 1.0f, A);

  // Verify that Translate3d() post-multiplies the existing matrix.
  A.MakeIdentity();
  A.Scale3d(6.0, 7.0, 8.0);
  A.Translate3d(2.0, 3.0, 4.0);
  EXPECT_ROW1_EQ(6.0f, 0.0f, 0.0f, 12.0f, A);
  EXPECT_ROW2_EQ(0.0f, 7.0f, 0.0f, 21.0f, A);
  EXPECT_ROW3_EQ(0.0f, 0.0f, 8.0f, 32.0f, A);
  EXPECT_ROW4_EQ(0.0f, 0.0f, 0.0f, 1.0f, A);

  Transform B;
  B.Scale3d(6.0, 7.0, 8.0);
  B.Translate3d(Vector3dF(2.0f, 3.0f, 4.0f));
  EXPECT_EQ(A, B);
}

TEST(XFormTest, verifyPostTranslate3d) {
  Transform A;
  A.PostTranslate3d(2.0, 3.0, 4.0);
  EXPECT_ROW1_EQ(1.0f, 0.0f, 0.0f, 2.0f, A);
  EXPECT_ROW2_EQ(0.0f, 1.0f, 0.0f, 3.0f, A);
  EXPECT_ROW3_EQ(0.0f, 0.0f, 1.0f, 4.0f, A);
  EXPECT_ROW4_EQ(0.0f, 0.0f, 0.0f, 1.0f, A);

  // Verify that PostTranslate3d() pre-multiplies the existing matrix.
  A.MakeIdentity();
  A.Scale3d(6.0, 7.0, 8.0);
  A.PostTranslate3d(2.0, 3.0, 4.0);
  EXPECT_ROW1_EQ(6.0f, 0.0f, 0.0f, 2.0f, A);
  EXPECT_ROW2_EQ(0.0f, 7.0f, 0.0f, 3.0f, A);
  EXPECT_ROW3_EQ(0.0f, 0.0f, 8.0f, 4.0f, A);
  EXPECT_ROW4_EQ(0.0f, 0.0f, 0.0f, 1.0f, A);

  Transform B;
  B.Scale3d(6.0, 7.0, 8.0);
  B.PostTranslate3d(Vector3dF(2.0f, 3.0f, 4.0f));
  EXPECT_EQ(A, B);
}

TEST(XFormTest, verifyScale) {
  Transform A;
  A.Scale(6.0, 7.0);
  EXPECT_ROW1_EQ(6.0f, 0.0f, 0.0f, 0.0f, A);
  EXPECT_ROW2_EQ(0.0f, 7.0f, 0.0f, 0.0f, A);
  EXPECT_ROW3_EQ(0.0f, 0.0f, 1.0f, 0.0f, A);
  EXPECT_ROW4_EQ(0.0f, 0.0f, 0.0f, 1.0f, A);

  // Verify that Scale() post-multiplies the existing matrix.
  A.MakeIdentity();
  A.Translate3d(2.0, 3.0, 4.0);
  A.Scale(6.0, 7.0);
  EXPECT_ROW1_EQ(6.0f, 0.0f, 0.0f, 2.0f, A);
  EXPECT_ROW2_EQ(0.0f, 7.0f, 0.0f, 3.0f, A);
  EXPECT_ROW3_EQ(0.0f, 0.0f, 1.0f, 4.0f, A);
  EXPECT_ROW4_EQ(0.0f, 0.0f, 0.0f, 1.0f, A);
}

TEST(XFormTest, verifyScale3d) {
  Transform A;
  A.Scale3d(6.0, 7.0, 8.0);
  EXPECT_ROW1_EQ(6.0f, 0.0f, 0.0f, 0.0f, A);
  EXPECT_ROW2_EQ(0.0f, 7.0f, 0.0f, 0.0f, A);
  EXPECT_ROW3_EQ(0.0f, 0.0f, 8.0f, 0.0f, A);
  EXPECT_ROW4_EQ(0.0f, 0.0f, 0.0f, 1.0f, A);

  // Verify that scale3d() post-multiplies the existing matrix.
  A.MakeIdentity();
  A.Translate3d(2.0, 3.0, 4.0);
  A.Scale3d(6.0, 7.0, 8.0);
  EXPECT_ROW1_EQ(6.0f, 0.0f, 0.0f, 2.0f, A);
  EXPECT_ROW2_EQ(0.0f, 7.0f, 0.0f, 3.0f, A);
  EXPECT_ROW3_EQ(0.0f, 0.0f, 8.0f, 4.0f, A);
  EXPECT_ROW4_EQ(0.0f, 0.0f, 0.0f, 1.0f, A);
}

TEST(XFormTest, verifyPostScale3d) {
  Transform A;
  A.PostScale3d(6.0, 7.0, 8.0);
  EXPECT_ROW1_EQ(6.0f, 0.0f, 0.0f, 0.0f, A);
  EXPECT_ROW2_EQ(0.0f, 7.0f, 0.0f, 0.0f, A);
  EXPECT_ROW3_EQ(0.0f, 0.0f, 8.0f, 0.0f, A);
  EXPECT_ROW4_EQ(0.0f, 0.0f, 0.0f, 1.0f, A);

  // Verify that PostScale3d() pre-multiplies the existing matrix.
  A.MakeIdentity();
  A.Translate3d(2.0, 3.0, 4.0);
  A.PostScale3d(6.0, 7.0, 8.0);
  EXPECT_ROW1_EQ(6.0f, 0.0f, 0.0f, 12.0f, A);
  EXPECT_ROW2_EQ(0.0f, 7.0f, 0.0f, 21.0f, A);
  EXPECT_ROW3_EQ(0.0f, 0.0f, 8.0f, 32.0f, A);
  EXPECT_ROW4_EQ(0.0f, 0.0f, 0.0f, 1.0f, A);
}

TEST(XFormTest, verifyRotate) {
  Transform A;
  A.Rotate(90.0);
  EXPECT_ROW1_NEAR(0.0, -1.0, 0.0, 0.0, A, ERROR_THRESHOLD);
  EXPECT_ROW2_NEAR(1.0, 0.0, 0.0, 0.0, A, ERROR_THRESHOLD);
  EXPECT_ROW3_EQ(0.0f, 0.0f, 1.0f, 0.0f, A);
  EXPECT_ROW4_EQ(0.0f, 0.0f, 0.0f, 1.0f, A);

  // Verify that Rotate() post-multiplies the existing matrix.
  A.MakeIdentity();
  A.Scale3d(6.0, 7.0, 8.0);
  A.Rotate(90.0);
  EXPECT_ROW1_NEAR(0.0, -6.0, 0.0, 0.0, A, ERROR_THRESHOLD);
  EXPECT_ROW2_NEAR(7.0, 0.0, 0.0, 0.0, A, ERROR_THRESHOLD);
  EXPECT_ROW3_EQ(0.0f, 0.0f, 8.0f, 0.0f, A);
  EXPECT_ROW4_EQ(0.0f, 0.0f, 0.0f, 1.0f, A);
}

TEST(XFormTest, verifyRotateAboutXAxis) {
  Transform A;
  double sin45 = 0.5 * sqrt(2.0);
  double cos45 = sin45;

  A.MakeIdentity();
  A.RotateAboutXAxis(90.0);
  EXPECT_ROW1_EQ(1.0f, 0.0f, 0.0f, 0.0f, A);
  EXPECT_ROW2_NEAR(0.0, 0.0, -1.0, 0.0, A, ERROR_THRESHOLD);
  EXPECT_ROW3_NEAR(0.0, 1.0, 0.0, 0.0, A, ERROR_THRESHOLD);
  EXPECT_ROW4_EQ(0.0f, 0.0f, 0.0f, 1.0f, A);

  A.MakeIdentity();
  A.RotateAboutXAxis(45.0);
  EXPECT_ROW1_EQ(1.0f, 0.0f, 0.0f, 0.0f, A);
  EXPECT_ROW2_NEAR(0.0, cos45, -sin45, 0.0, A, ERROR_THRESHOLD);
  EXPECT_ROW3_NEAR(0.0, sin45, cos45, 0.0, A, ERROR_THRESHOLD);
  EXPECT_ROW4_EQ(0.0f, 0.0f, 0.0f, 1.0f, A);

  // Verify that RotateAboutXAxis(angle) post-multiplies the existing matrix.
  A.MakeIdentity();
  A.Scale3d(6.0, 7.0, 8.0);
  A.RotateAboutXAxis(90.0);
  EXPECT_ROW1_NEAR(6.0, 0.0, 0.0, 0.0, A, ERROR_THRESHOLD);
  EXPECT_ROW2_NEAR(0.0, 0.0, -7.0, 0.0, A, ERROR_THRESHOLD);
  EXPECT_ROW3_NEAR(0.0, 8.0, 0.0, 0.0, A, ERROR_THRESHOLD);
  EXPECT_ROW4_EQ(0.0f, 0.0f, 0.0f, 1.0f, A);
}

TEST(XFormTest, verifyRotateAboutYAxis) {
  Transform A;
  double sin45 = 0.5 * sqrt(2.0);
  double cos45 = sin45;

  // Note carefully, the expected pattern is inverted compared to rotating
  // about x axis or z axis.
  A.MakeIdentity();
  A.RotateAboutYAxis(90.0);
  EXPECT_ROW1_NEAR(0.0, 0.0, 1.0, 0.0, A, ERROR_THRESHOLD);
  EXPECT_ROW2_EQ(0.0f, 1.0f, 0.0f, 0.0f, A);
  EXPECT_ROW3_NEAR(-1.0, 0.0, 0.0, 0.0, A, ERROR_THRESHOLD);
  EXPECT_ROW4_EQ(0.0f, 0.0f, 0.0f, 1.0f, A);

  A.MakeIdentity();
  A.RotateAboutYAxis(45.0);
  EXPECT_ROW1_NEAR(cos45, 0.0, sin45, 0.0, A, ERROR_THRESHOLD);
  EXPECT_ROW2_EQ(0.0f, 1.0f, 0.0f, 0.0f, A);
  EXPECT_ROW3_NEAR(-sin45, 0.0, cos45, 0.0, A, ERROR_THRESHOLD);
  EXPECT_ROW4_EQ(0.0f, 0.0f, 0.0f, 1.0f, A);

  // Verify that RotateAboutYAxis(angle) post-multiplies the existing matrix.
  A.MakeIdentity();
  A.Scale3d(6.0, 7.0, 8.0);
  A.RotateAboutYAxis(90.0);
  EXPECT_ROW1_NEAR(0.0, 0.0, 6.0, 0.0, A, ERROR_THRESHOLD);
  EXPECT_ROW2_NEAR(0.0, 7.0, 0.0, 0.0, A, ERROR_THRESHOLD);
  EXPECT_ROW3_NEAR(-8.0, 0.0, 0.0, 0.0, A, ERROR_THRESHOLD);
  EXPECT_ROW4_EQ(0.0f, 0.0f, 0.0f, 1.0f, A);
}

TEST(XFormTest, verifyRotateAboutZAxis) {
  Transform A;
  double sin45 = 0.5 * sqrt(2.0);
  double cos45 = sin45;

  A.MakeIdentity();
  A.RotateAboutZAxis(90.0);
  EXPECT_ROW1_NEAR(0.0, -1.0, 0.0, 0.0, A, ERROR_THRESHOLD);
  EXPECT_ROW2_NEAR(1.0, 0.0, 0.0, 0.0, A, ERROR_THRESHOLD);
  EXPECT_ROW3_EQ(0.0f, 0.0f, 1.0f, 0.0f, A);
  EXPECT_ROW4_EQ(0.0f, 0.0f, 0.0f, 1.0f, A);

  A.MakeIdentity();
  A.RotateAboutZAxis(45.0);
  EXPECT_ROW1_NEAR(cos45, -sin45, 0.0, 0.0, A, ERROR_THRESHOLD);
  EXPECT_ROW2_NEAR(sin45, cos45, 0.0, 0.0, A, ERROR_THRESHOLD);
  EXPECT_ROW3_EQ(0.0f, 0.0f, 1.0f, 0.0f, A);
  EXPECT_ROW4_EQ(0.0f, 0.0f, 0.0f, 1.0f, A);

  // Verify that RotateAboutZAxis(angle) post-multiplies the existing matrix.
  A.MakeIdentity();
  A.Scale3d(6.0, 7.0, 8.0);
  A.RotateAboutZAxis(90.0);
  EXPECT_ROW1_NEAR(0.0, -6.0, 0.0, 0.0, A, ERROR_THRESHOLD);
  EXPECT_ROW2_NEAR(7.0, 0.0, 0.0, 0.0, A, ERROR_THRESHOLD);
  EXPECT_ROW3_EQ(0.0f, 0.0f, 8.0f, 0.0f, A);
  EXPECT_ROW4_EQ(0.0f, 0.0f, 0.0f, 1.0f, A);
}

TEST(XFormTest, verifyRotateAboutForAlignedAxes) {
  Transform A;

  // Check rotation about z-axis
  A.MakeIdentity();
  A.RotateAbout(Vector3dF(0.0, 0.0, 1.0), 90.0);
  EXPECT_ROW1_NEAR(0.0, -1.0, 0.0, 0.0, A, ERROR_THRESHOLD);
  EXPECT_ROW2_NEAR(1.0, 0.0, 0.0, 0.0, A, ERROR_THRESHOLD);
  EXPECT_ROW3_EQ(0.0f, 0.0f, 1.0f, 0.0f, A);
  EXPECT_ROW4_EQ(0.0f, 0.0f, 0.0f, 1.0f, A);

  // Check rotation about x-axis
  A.MakeIdentity();
  A.RotateAbout(Vector3dF(1.0, 0.0, 0.0), 90.0);
  EXPECT_ROW1_EQ(1.0f, 0.0f, 0.0f, 0.0f, A);
  EXPECT_ROW2_NEAR(0.0, 0.0, -1.0, 0.0, A, ERROR_THRESHOLD);
  EXPECT_ROW3_NEAR(0.0, 1.0, 0.0, 0.0, A, ERROR_THRESHOLD);
  EXPECT_ROW4_EQ(0.0f, 0.0f, 0.0f, 1.0f, A);

  // Check rotation about y-axis. Note carefully, the expected pattern is
  // inverted compared to rotating about x axis or z axis.
  A.MakeIdentity();
  A.RotateAbout(Vector3dF(0.0, 1.0, 0.0), 90.0);
  EXPECT_ROW1_NEAR(0.0, 0.0, 1.0, 0.0, A, ERROR_THRESHOLD);
  EXPECT_ROW2_EQ(0.0f, 1.0f, 0.0f, 0.0f, A);
  EXPECT_ROW3_NEAR(-1.0, 0.0, 0.0, 0.0, A, ERROR_THRESHOLD);
  EXPECT_ROW4_EQ(0.0f, 0.0f, 0.0f, 1.0f, A);

  // Verify that rotate3d(axis, angle) post-multiplies the existing matrix.
  A.MakeIdentity();
  A.Scale3d(6.0, 7.0, 8.0);
  A.RotateAboutZAxis(90.0);
  EXPECT_ROW1_NEAR(0.0, -6.0, 0.0, 0.0, A, ERROR_THRESHOLD);
  EXPECT_ROW2_NEAR(7.0, 0.0, 0.0, 0.0, A, ERROR_THRESHOLD);
  EXPECT_ROW3_EQ(0.0f, 0.0f, 8.0f, 0.0f, A);
  EXPECT_ROW4_EQ(0.0f, 0.0f, 0.0f, 1.0f, A);
}

TEST(XFormTest, verifyRotateAboutForArbitraryAxis) {
  // Check rotation about an arbitrary non-axis-aligned vector.
  Transform A;
  A.RotateAbout(Vector3dF(1.0, 1.0, 1.0), 90.0);
  EXPECT_ROW1_NEAR(0.3333333333333334258519187, -0.2440169358562924717404030,
                   0.9106836025229592124219380, 0.0, A, ERROR_THRESHOLD);
  EXPECT_ROW2_NEAR(0.9106836025229592124219380, 0.3333333333333334258519187,
                   -0.2440169358562924717404030, 0.0, A, ERROR_THRESHOLD);
  EXPECT_ROW3_NEAR(-0.2440169358562924717404030, 0.9106836025229592124219380,
                   0.3333333333333334258519187, 0.0, A, ERROR_THRESHOLD);
  EXPECT_ROW4_EQ(0.0f, 0.0f, 0.0f, 1.0f, A);
}

TEST(XFormTest, verifyRotateAboutForDegenerateAxis) {
  // Check rotation about a degenerate zero vector.
  // It is expected to skip applying the rotation.
  Transform A;

  A.RotateAbout(Vector3dF(0.0, 0.0, 0.0), 45.0);
  // Verify that A remains unchanged.
  EXPECT_TRUE(A.IsIdentity());

  InitializeTestMatrix(&A);
  A.RotateAbout(Vector3dF(0.0, 0.0, 0.0), 35.0);

  // Verify that A remains unchanged.
  EXPECT_ROW1_EQ(10.0f, 14.0f, 18.0f, 22.0f, A);
  EXPECT_ROW2_EQ(11.0f, 15.0f, 19.0f, 23.0f, A);
  EXPECT_ROW3_EQ(12.0f, 16.0f, 20.0f, 24.0f, A);
  EXPECT_ROW4_EQ(13.0f, 17.0f, 21.0f, 25.0f, A);
}

TEST(XFormTest, verifySkew) {
  // Test a skew along X axis only
  Transform A;
  A.Skew(45.0, 0.0);
  EXPECT_ROW1_EQ(1.0f, 1.0f, 0.0f, 0.0f, A);
  EXPECT_ROW2_EQ(0.0f, 1.0f, 0.0f, 0.0f, A);
  EXPECT_ROW3_EQ(0.0f, 0.0f, 1.0f, 0.0f, A);
  EXPECT_ROW4_EQ(0.0f, 0.0f, 0.0f, 1.0f, A);

  // Test a skew along Y axis only
  A.MakeIdentity();
  A.Skew(0.0, 45.0);
  EXPECT_ROW1_EQ(1.0f, 0.0f, 0.0f, 0.0f, A);
  EXPECT_ROW2_EQ(1.0f, 1.0f, 0.0f, 0.0f, A);
  EXPECT_ROW3_EQ(0.0f, 0.0f, 1.0f, 0.0f, A);
  EXPECT_ROW4_EQ(0.0f, 0.0f, 0.0f, 1.0f, A);

  // Verify that skew() post-multiplies the existing matrix. Row 1, column 2,
  // would incorrectly have value "7" if the matrix is pre-multiplied instead
  // of post-multiplied.
  A.MakeIdentity();
  A.Scale3d(6.0, 7.0, 8.0);
  A.Skew(45.0, 0.0);
  EXPECT_ROW1_EQ(6.0f, 6.0f, 0.0f, 0.0f, A);
  EXPECT_ROW2_EQ(0.0f, 7.0f, 0.0f, 0.0f, A);
  EXPECT_ROW3_EQ(0.0f, 0.0f, 8.0f, 0.0f, A);
  EXPECT_ROW4_EQ(0.0f, 0.0f, 0.0f, 1.0f, A);

  // Test a skew along X and Y axes both
  A.MakeIdentity();
  A.Skew(45.0, 45.0);
  EXPECT_ROW1_EQ(1.0f, 1.0f, 0.0f, 0.0f, A);
  EXPECT_ROW2_EQ(1.0f, 1.0f, 0.0f, 0.0f, A);
  EXPECT_ROW3_EQ(0.0f, 0.0f, 1.0f, 0.0f, A);
  EXPECT_ROW4_EQ(0.0f, 0.0f, 0.0f, 1.0f, A);
}

TEST(XFormTest, verifyPerspectiveDepth) {
  Transform A;
  A.ApplyPerspectiveDepth(1.0);
  EXPECT_ROW1_EQ(1.0f, 0.0f, 0.0f, 0.0f, A);
  EXPECT_ROW2_EQ(0.0f, 1.0f, 0.0f, 0.0f, A);
  EXPECT_ROW3_EQ(0.0f, 0.0f, 1.0f, 0.0f, A);
  EXPECT_ROW4_EQ(0.0f, 0.0f, -1.0f, 1.0f, A);

  // Verify that PerspectiveDepth() post-multiplies the existing matrix.
  A.MakeIdentity();
  A.Translate3d(2.0, 3.0, 4.0);
  A.ApplyPerspectiveDepth(1.0);
  EXPECT_ROW1_EQ(1.0f, 0.0f, -2.0f, 2.0f, A);
  EXPECT_ROW2_EQ(0.0f, 1.0f, -3.0f, 3.0f, A);
  EXPECT_ROW3_EQ(0.0f, 0.0f, -3.0f, 4.0f, A);
  EXPECT_ROW4_EQ(0.0f, 0.0f, -1.0f, 1.0f, A);
}

TEST(XFormTest, verifyHasPerspective) {
  Transform A;
  A.ApplyPerspectiveDepth(1.0);
  EXPECT_TRUE(A.HasPerspective());

  A.MakeIdentity();
  A.ApplyPerspectiveDepth(0.0);
  EXPECT_FALSE(A.HasPerspective());

  A.MakeIdentity();
  A.set_rc(3, 0, -1.f);
  EXPECT_TRUE(A.HasPerspective());

  A.MakeIdentity();
  A.set_rc(3, 1, -1.f);
  EXPECT_TRUE(A.HasPerspective());

  A.MakeIdentity();
  A.set_rc(3, 2, -0.3f);
  EXPECT_TRUE(A.HasPerspective());

  A.MakeIdentity();
  A.set_rc(3, 3, 0.5f);
  EXPECT_TRUE(A.HasPerspective());

  A.MakeIdentity();
  A.set_rc(3, 3, 0.f);
  EXPECT_TRUE(A.HasPerspective());
}

TEST(XFormTest, verifyIsInvertible) {
  Transform A;

  // Translations, rotations, scales, skews and arbitrary combinations of them
  // are invertible.
  A.MakeIdentity();
  EXPECT_TRUE(A.IsInvertible());

  A.MakeIdentity();
  A.Translate3d(2.0, 3.0, 4.0);
  EXPECT_TRUE(A.IsInvertible());

  A.MakeIdentity();
  A.Scale3d(6.0, 7.0, 8.0);
  EXPECT_TRUE(A.IsInvertible());

  A.MakeIdentity();
  A.RotateAboutXAxis(10.0);
  A.RotateAboutYAxis(20.0);
  A.RotateAboutZAxis(30.0);
  EXPECT_TRUE(A.IsInvertible());

  A.MakeIdentity();
  A.Skew(45.0, 0.0);
  EXPECT_TRUE(A.IsInvertible());

  // A perspective matrix (projection plane at z=0) is invertible. The
  // intuitive explanation is that perspective is equivalent to a skew of the
  // w-axis; skews are invertible.
  A.MakeIdentity();
  A.ApplyPerspectiveDepth(1.0);
  EXPECT_TRUE(A.IsInvertible());

  // A "pure" perspective matrix derived by similar triangles, with m44() set
  // to zero (i.e. camera positioned at the origin), is not invertible.
  A.MakeIdentity();
  A.ApplyPerspectiveDepth(1.0);
  A.set_rc(3, 3, 0.f);
  EXPECT_FALSE(A.IsInvertible());

  // Adding more to a non-invertible matrix will not make it invertible in the
  // general case.
  A.MakeIdentity();
  A.ApplyPerspectiveDepth(1.0);
  A.set_rc(3, 3, 0.f);
  A.Scale3d(6.0, 7.0, 8.0);
  A.RotateAboutXAxis(10.0);
  A.RotateAboutYAxis(20.0);
  A.RotateAboutZAxis(30.0);
  A.Translate3d(6.0, 7.0, 8.0);
#if !defined(ARCH_CPU_ARM_FAMILY)
  // TODO(enne): Make this pass on ARM, https://crbug.com/662558
  EXPECT_FALSE(A.IsInvertible());
#endif

  // A degenerate matrix of all zeros is not invertible.
  A.MakeIdentity();
  A.set_rc(0, 0, 0.f);
  A.set_rc(1, 1, 0.f);
  A.set_rc(2, 2, 0.f);
  A.set_rc(3, 3, 0.f);
  EXPECT_FALSE(A.IsInvertible());
}

TEST(XFormTest, verifyIsIdentity) {
  Transform A;

  InitializeTestMatrix(&A);
  EXPECT_FALSE(A.IsIdentity());

  A.MakeIdentity();
  EXPECT_TRUE(A.IsIdentity());

  // Modifying any one individual element should cause the matrix to no longer
  // be identity.
  A.MakeIdentity();
  A.set_rc(0, 0, 2.f);
  EXPECT_FALSE(A.IsIdentity());

  A.MakeIdentity();
  A.set_rc(1, 0, 2.f);
  EXPECT_FALSE(A.IsIdentity());

  A.MakeIdentity();
  A.set_rc(2, 0, 2.f);
  EXPECT_FALSE(A.IsIdentity());

  A.MakeIdentity();
  A.set_rc(3, 0, 2.f);
  EXPECT_FALSE(A.IsIdentity());

  A.MakeIdentity();
  A.set_rc(0, 1, 2.f);
  EXPECT_FALSE(A.IsIdentity());

  A.MakeIdentity();
  A.set_rc(1, 1, 2.f);
  EXPECT_FALSE(A.IsIdentity());

  A.MakeIdentity();
  A.set_rc(2, 1, 2.f);
  EXPECT_FALSE(A.IsIdentity());

  A.MakeIdentity();
  A.set_rc(3, 1, 2.f);
  EXPECT_FALSE(A.IsIdentity());

  A.MakeIdentity();
  A.set_rc(0, 2, 2.f);
  EXPECT_FALSE(A.IsIdentity());

  A.MakeIdentity();
  A.set_rc(1, 2, 2.f);
  EXPECT_FALSE(A.IsIdentity());

  A.MakeIdentity();
  A.set_rc(2, 2, 2.f);
  EXPECT_FALSE(A.IsIdentity());

  A.MakeIdentity();
  A.set_rc(3, 2, 2.f);
  EXPECT_FALSE(A.IsIdentity());

  A.MakeIdentity();
  A.set_rc(0, 3, 2.f);
  EXPECT_FALSE(A.IsIdentity());

  A.MakeIdentity();
  A.set_rc(1, 3, 2.f);
  EXPECT_FALSE(A.IsIdentity());

  A.MakeIdentity();
  A.set_rc(2, 3, 2.f);
  EXPECT_FALSE(A.IsIdentity());

  A.MakeIdentity();
  A.set_rc(3, 3, 2.f);
  EXPECT_FALSE(A.IsIdentity());
}

TEST(XFormTest, verifyIsIdentityOrTranslation) {
  Transform A;

  InitializeTestMatrix(&A);
  EXPECT_FALSE(A.IsIdentityOrTranslation());

  A.MakeIdentity();
  EXPECT_TRUE(A.IsIdentityOrTranslation());

  // Modifying any non-translation components should cause
  // IsIdentityOrTranslation() to return false. NOTE: (0, 3), (1, 3), and
  // (2, 3) are the translation components, so modifying them should still
  // return true.
  A.MakeIdentity();
  A.set_rc(0, 0, 2.f);
  EXPECT_FALSE(A.IsIdentityOrTranslation());

  A.MakeIdentity();
  A.set_rc(1, 0, 2.f);
  EXPECT_FALSE(A.IsIdentityOrTranslation());

  A.MakeIdentity();
  A.set_rc(2, 0, 2.f);
  EXPECT_FALSE(A.IsIdentityOrTranslation());

  A.MakeIdentity();
  A.set_rc(3, 0, 2.f);
  EXPECT_FALSE(A.IsIdentityOrTranslation());

  A.MakeIdentity();
  A.set_rc(0, 1, 2.f);
  EXPECT_FALSE(A.IsIdentityOrTranslation());

  A.MakeIdentity();
  A.set_rc(1, 1, 2.f);
  EXPECT_FALSE(A.IsIdentityOrTranslation());

  A.MakeIdentity();
  A.set_rc(2, 1, 2.f);
  EXPECT_FALSE(A.IsIdentityOrTranslation());

  A.MakeIdentity();
  A.set_rc(3, 1, 2.f);
  EXPECT_FALSE(A.IsIdentityOrTranslation());

  A.MakeIdentity();
  A.set_rc(0, 2, 2.f);
  EXPECT_FALSE(A.IsIdentityOrTranslation());

  A.MakeIdentity();
  A.set_rc(1, 2, 2.f);
  EXPECT_FALSE(A.IsIdentityOrTranslation());

  A.MakeIdentity();
  A.set_rc(2, 2, 2.f);
  EXPECT_FALSE(A.IsIdentityOrTranslation());

  A.MakeIdentity();
  A.set_rc(3, 2, 2.f);
  EXPECT_FALSE(A.IsIdentityOrTranslation());

  // Note carefully - expecting true here.
  A.MakeIdentity();
  A.set_rc(0, 3, 2.f);
  EXPECT_TRUE(A.IsIdentityOrTranslation());

  // Note carefully - expecting true here.
  A.MakeIdentity();
  A.set_rc(1, 3, 2.f);
  EXPECT_TRUE(A.IsIdentityOrTranslation());

  // Note carefully - expecting true here.
  A.MakeIdentity();
  A.set_rc(2, 3, 2.f);
  EXPECT_TRUE(A.IsIdentityOrTranslation());

  A.MakeIdentity();
  A.set_rc(3, 3, 2.f);
  EXPECT_FALSE(A.IsIdentityOrTranslation());
}

TEST(XFormTest, verifyIsApproximatelyIdentityOrTranslation) {
  Transform A;

  // Exact pure translation.
  A.MakeIdentity();

  EXPECT_TRUE(A.IsApproximatelyIdentityOrTranslation(0));
  EXPECT_TRUE(A.IsApproximatelyIdentityOrTranslation(kApproxZero));
  EXPECT_TRUE(A.IsApproximatelyIdentityOrIntegerTranslation(0));
  EXPECT_TRUE(A.IsApproximatelyIdentityOrIntegerTranslation(kApproxZero));

  // Set translate values to integer values other than 0 or 1.
  A.set_rc(0, 3, 3);
  A.set_rc(1, 3, 4);
  A.set_rc(2, 3, 5);

  EXPECT_TRUE(A.IsApproximatelyIdentityOrTranslation(0));
  EXPECT_TRUE(A.IsApproximatelyIdentityOrTranslation(kApproxZero));
  EXPECT_TRUE(A.IsApproximatelyIdentityOrIntegerTranslation(0));
  EXPECT_TRUE(A.IsApproximatelyIdentityOrIntegerTranslation(kApproxZero));

  // Set translate values to values other than 0 or 1.
  A.set_rc(0, 3, 3.4f);
  A.set_rc(1, 3, 4.4f);
  A.set_rc(2, 3, 5.6f);

  EXPECT_TRUE(A.IsApproximatelyIdentityOrTranslation(0));
  EXPECT_TRUE(A.IsApproximatelyIdentityOrTranslation(kApproxZero));
  EXPECT_FALSE(A.IsApproximatelyIdentityOrIntegerTranslation(0));
  EXPECT_FALSE(A.IsApproximatelyIdentityOrIntegerTranslation(kApproxZero));

  // Approximately pure translation.
  InitializeApproxIdentityMatrix(&A);

  EXPECT_FALSE(A.IsApproximatelyIdentityOrTranslation(0));
  EXPECT_FALSE(A.IsApproximatelyIdentityOrTranslation(kApproxZero));
  EXPECT_FALSE(A.IsApproximatelyIdentityOrIntegerTranslation(0));
  EXPECT_FALSE(A.IsApproximatelyIdentityOrIntegerTranslation(kApproxZero));

  // Some values must be exact.
  A.set_rc(3, 0, 0);
  A.set_rc(3, 1, 0);
  A.set_rc(3, 2, 0);
  A.set_rc(3, 3, 1);

  EXPECT_FALSE(A.IsApproximatelyIdentityOrTranslation(0));
  EXPECT_TRUE(A.IsApproximatelyIdentityOrTranslation(kApproxZero));
  EXPECT_FALSE(A.IsApproximatelyIdentityOrIntegerTranslation(0));
  EXPECT_TRUE(A.IsApproximatelyIdentityOrIntegerTranslation(kApproxZero));

  // Set translate values to values other than 0 or 1.
  A.set_rc(0, 3, A.rc(0, 3) + 3);
  A.set_rc(1, 3, A.rc(1, 3) + 4);
  A.set_rc(2, 3, A.rc(2, 3) + 5);

  EXPECT_FALSE(A.IsApproximatelyIdentityOrTranslation(0));
  EXPECT_TRUE(A.IsApproximatelyIdentityOrTranslation(kApproxZero));
  EXPECT_FALSE(A.IsApproximatelyIdentityOrIntegerTranslation(0));
  EXPECT_TRUE(A.IsApproximatelyIdentityOrIntegerTranslation(kApproxZero));

  // Set translate values to values other than 0 or 1.
  A.set_rc(0, 3, 3.4f);
  A.set_rc(1, 3, 4.4f);
  A.set_rc(2, 3, 5.6f);

  EXPECT_FALSE(A.IsApproximatelyIdentityOrTranslation(0));
  EXPECT_TRUE(A.IsApproximatelyIdentityOrTranslation(kApproxZero));
  EXPECT_FALSE(A.IsApproximatelyIdentityOrIntegerTranslation(0));
  EXPECT_FALSE(A.IsApproximatelyIdentityOrIntegerTranslation(kApproxZero));

  // Not approximately pure translation.
  InitializeApproxIdentityMatrix(&A);

  // Some values must be exact.
  A.set_rc(3, 0, 0);
  A.set_rc(3, 1, 0);
  A.set_rc(3, 2, 0);
  A.set_rc(3, 3, 1);

  // Set some values (not translate values) to values other than 0 or 1.
  A.set_rc(0, 1, 3.4f);
  A.set_rc(3, 2, 4.4f);
  A.set_rc(2, 0, 5.6f);

  EXPECT_FALSE(A.IsApproximatelyIdentityOrTranslation(0));
  EXPECT_FALSE(A.IsApproximatelyIdentityOrTranslation(kApproxZero));
  EXPECT_FALSE(A.IsApproximatelyIdentityOrIntegerTranslation(0));
  EXPECT_FALSE(A.IsApproximatelyIdentityOrIntegerTranslation(kApproxZero));
}

TEST(XFormTest, verifyIsScaleOrTranslation) {
  Transform A;

  InitializeTestMatrix(&A);
  EXPECT_FALSE(A.IsScaleOrTranslation());

  A.MakeIdentity();
  EXPECT_TRUE(A.IsScaleOrTranslation());

  // Modifying any non-scale or non-translation components should cause
  // IsScaleOrTranslation() to return false. (0, 0), (1, 1), (2, 2), (0, 3),
  // (1, 3), and (2, 3) are the scale and translation components, so
  // modifying them should still return true.

  // Note carefully - expecting true here.
  A.MakeIdentity();
  A.set_rc(0, 0, 2.f);
  EXPECT_TRUE(A.IsScaleOrTranslation());

  A.MakeIdentity();
  A.set_rc(1, 0, 2.f);
  EXPECT_FALSE(A.IsScaleOrTranslation());

  A.MakeIdentity();
  A.set_rc(2, 0, 2.f);
  EXPECT_FALSE(A.IsScaleOrTranslation());

  A.MakeIdentity();
  A.set_rc(3, 0, 2.f);
  EXPECT_FALSE(A.IsScaleOrTranslation());

  A.MakeIdentity();
  A.set_rc(0, 1, 2.f);
  EXPECT_FALSE(A.IsScaleOrTranslation());

  // Note carefully - expecting true here.
  A.MakeIdentity();
  A.set_rc(1, 1, 2.f);
  EXPECT_TRUE(A.IsScaleOrTranslation());

  A.MakeIdentity();
  A.set_rc(2, 1, 2.f);
  EXPECT_FALSE(A.IsScaleOrTranslation());

  A.MakeIdentity();
  A.set_rc(3, 1, 2.f);
  EXPECT_FALSE(A.IsScaleOrTranslation());

  A.MakeIdentity();
  A.set_rc(0, 2, 2.f);
  EXPECT_FALSE(A.IsScaleOrTranslation());

  A.MakeIdentity();
  A.set_rc(1, 2, 2.f);
  EXPECT_FALSE(A.IsScaleOrTranslation());

  // Note carefully - expecting true here.
  A.MakeIdentity();
  A.set_rc(2, 2, 2.f);
  EXPECT_TRUE(A.IsScaleOrTranslation());

  A.MakeIdentity();
  A.set_rc(3, 2, 2.f);
  EXPECT_FALSE(A.IsScaleOrTranslation());

  // Note carefully - expecting true here.
  A.MakeIdentity();
  A.set_rc(0, 3, 2.f);
  EXPECT_TRUE(A.IsScaleOrTranslation());

  // Note carefully - expecting true here.
  A.MakeIdentity();
  A.set_rc(1, 3, 2.f);
  EXPECT_TRUE(A.IsScaleOrTranslation());

  // Note carefully - expecting true here.
  A.MakeIdentity();
  A.set_rc(2, 3, 2.f);
  EXPECT_TRUE(A.IsScaleOrTranslation());

  A.MakeIdentity();
  A.set_rc(3, 3, 2.f);
  EXPECT_FALSE(A.IsScaleOrTranslation());
}

TEST(XFormTest, Scale) {
  Transform t;
  EXPECT_TRUE(t.IsScale());
  EXPECT_TRUE(t.IsScale2d());
  EXPECT_EQ(Vector2dF(1, 1), t.To2dScale());

  t.Scale(2.5f, 3.75f);
  EXPECT_TRUE(t.IsScale());
  EXPECT_TRUE(t.IsScale2d());
  EXPECT_EQ(Vector2dF(2.5f, 3.75f), t.To2dScale());

  t.Scale3d(3, 4, 5);
  EXPECT_TRUE(t.IsScale());
  EXPECT_FALSE(t.IsScale2d());
  EXPECT_EQ(Vector2dF(7.5f, 15.f), t.To2dScale());

  for (int row = 0; row < 4; row++) {
    for (int col = 0; col < 4; col++) {
      t.MakeIdentity();
      t.set_rc(row, col, 100);
      bool is_scale = row == col && (row == 0 || row == 1 || row == 2);
      bool is_scale_2d = row == col && (row == 0 || row == 1);
      EXPECT_EQ(is_scale, t.IsScale()) << " row=" << row << " col=" << col;
      EXPECT_EQ(is_scale_2d, t.IsScale2d()) << " row=" << row << " col=" << col;
    }
  }
}

TEST(XFormTest, verifyFlattenTo2d) {
  Transform A;
  InitializeTestMatrix(&A);

  A.FlattenTo2d();
  EXPECT_ROW1_EQ(10.0f, 14.0f, 0.0f, 22.0f, A);
  EXPECT_ROW2_EQ(11.0f, 15.0f, 0.0f, 23.0f, A);
  EXPECT_ROW3_EQ(0.0f, 0.0f, 1.0f, 0.0f, A);
  EXPECT_ROW4_EQ(13.0f, 17.0f, 0.0f, 25.0f, A);
}

TEST(XFormTest, IsFlat) {
  Transform transform;
  InitializeTestMatrix(&transform);

  // A transform with all entries non-zero isn't flat.
  EXPECT_FALSE(transform.IsFlat());

  transform.set_rc(0, 2, 0.f);
  transform.set_rc(1, 2, 0.f);
  transform.set_rc(2, 2, 1.f);
  transform.set_rc(3, 2, 0.f);

  EXPECT_FALSE(transform.IsFlat());

  transform.set_rc(2, 0, 0.f);
  transform.set_rc(2, 1, 0.f);
  transform.set_rc(2, 3, 0.f);

  // Since the third column and row are both (0, 0, 1, 0), the transform is
  // flat.
  EXPECT_TRUE(transform.IsFlat());
}

// Another implementation of Preserves2dAxisAlignment that isn't as fast,
// good for testing the faster implementation.
static bool EmpiricallyPreserves2dAxisAlignment(const Transform& transform) {
  Point3F p1(5.0f, 5.0f, 0.0f);
  Point3F p2(10.0f, 5.0f, 0.0f);
  Point3F p3(10.0f, 20.0f, 0.0f);
  Point3F p4(5.0f, 20.0f, 0.0f);

  QuadF test_quad(PointF(p1.x(), p1.y()), PointF(p2.x(), p2.y()),
                  PointF(p3.x(), p3.y()), PointF(p4.x(), p4.y()));
  EXPECT_TRUE(test_quad.IsRectilinear());

  p1 = transform.MapPoint(p1);
  p2 = transform.MapPoint(p2);
  p3 = transform.MapPoint(p3);
  p4 = transform.MapPoint(p4);

  QuadF transformedQuad(PointF(p1.x(), p1.y()), PointF(p2.x(), p2.y()),
                        PointF(p3.x(), p3.y()), PointF(p4.x(), p4.y()));
  return transformedQuad.IsRectilinear();
}

TEST(XFormTest, Preserves2dAxisAlignment) {
  static const struct TestCase {
    SkScalar a;  // row 1, column 1
    SkScalar b;  // row 1, column 2
    SkScalar c;  // row 2, column 1
    SkScalar d;  // row 2, column 2
    bool expected;
    bool degenerate;
  } test_cases[] = {
      // clang-format off
      { 3.f, 0.f,
        0.f, 4.f, true, false },  // basic case
      { 0.f, 4.f,
        3.f, 0.f, true, false },  // rotate by 90
      { 0.f, 0.f,
        0.f, 4.f, true, true },   // degenerate x
      { 3.f, 0.f,
        0.f, 0.f, true, true },   // degenerate y
      { 0.f, 0.f,
        3.f, 0.f, true, true },   // degenerate x + rotate by 90
      { 0.f, 4.f,
        0.f, 0.f, true, true },   // degenerate y + rotate by 90
      { 3.f, 4.f,
        0.f, 0.f, false, true },
      { 0.f, 0.f,
        3.f, 4.f, false, true },
      { 0.f, 3.f,
        0.f, 4.f, false, true },
      { 3.f, 0.f,
        4.f, 0.f, false, true },
      { 3.f, 4.f,
        5.f, 0.f, false, false },
      { 3.f, 4.f,
        0.f, 5.f, false, false },
      { 3.f, 0.f,
        4.f, 5.f, false, false },
      { 0.f, 3.f,
        4.f, 5.f, false, false },
      { 2.f, 3.f,
        4.f, 5.f, false, false },
      // clang-format on
  };

  Transform transform;
  for (const auto& value : test_cases) {
    transform.MakeIdentity();
    transform.set_rc(0, 0, value.a);
    transform.set_rc(0, 1, value.b);
    transform.set_rc(1, 0, value.c);
    transform.set_rc(1, 1, value.d);

    if (value.expected) {
      EXPECT_TRUE(EmpiricallyPreserves2dAxisAlignment(transform));
      EXPECT_TRUE(transform.Preserves2dAxisAlignment());
      if (value.degenerate) {
        EXPECT_FALSE(transform.NonDegeneratePreserves2dAxisAlignment());
      } else {
        EXPECT_TRUE(transform.NonDegeneratePreserves2dAxisAlignment());
      }
    } else {
      EXPECT_FALSE(EmpiricallyPreserves2dAxisAlignment(transform));
      EXPECT_FALSE(transform.Preserves2dAxisAlignment());
      EXPECT_FALSE(transform.NonDegeneratePreserves2dAxisAlignment());
    }
  }

  // Try the same test cases again, but this time make sure that other matrix
  // elements (except perspective) have entries, to test that they are ignored.
  for (const auto& value : test_cases) {
    transform.MakeIdentity();
    transform.set_rc(0, 0, value.a);
    transform.set_rc(0, 1, value.b);
    transform.set_rc(1, 0, value.c);
    transform.set_rc(1, 1, value.d);

    transform.set_rc(0, 2, 1.f);
    transform.set_rc(0, 3, 2.f);
    transform.set_rc(1, 2, 3.f);
    transform.set_rc(1, 3, 4.f);
    transform.set_rc(2, 0, 5.f);
    transform.set_rc(2, 1, 6.f);
    transform.set_rc(2, 2, 7.f);
    transform.set_rc(2, 3, 8.f);

    if (value.expected) {
      EXPECT_TRUE(EmpiricallyPreserves2dAxisAlignment(transform));
      EXPECT_TRUE(transform.Preserves2dAxisAlignment());
      if (value.degenerate) {
        EXPECT_FALSE(transform.NonDegeneratePreserves2dAxisAlignment());
      } else {
        EXPECT_TRUE(transform.NonDegeneratePreserves2dAxisAlignment());
      }
    } else {
      EXPECT_FALSE(EmpiricallyPreserves2dAxisAlignment(transform));
      EXPECT_FALSE(transform.Preserves2dAxisAlignment());
      EXPECT_FALSE(transform.NonDegeneratePreserves2dAxisAlignment());
    }
  }

  // Try the same test cases again, but this time add perspective which is
  // always assumed to not-preserve axis alignment.
  for (const auto& value : test_cases) {
    transform.MakeIdentity();
    transform.set_rc(0, 0, value.a);
    transform.set_rc(0, 1, value.b);
    transform.set_rc(1, 0, value.c);
    transform.set_rc(1, 1, value.d);

    transform.set_rc(0, 2, 1.f);
    transform.set_rc(0, 3, 2.f);
    transform.set_rc(1, 2, 3.f);
    transform.set_rc(1, 3, 4.f);
    transform.set_rc(2, 0, 5.f);
    transform.set_rc(2, 1, 6.f);
    transform.set_rc(2, 2, 7.f);
    transform.set_rc(2, 3, 8.f);
    transform.set_rc(3, 0, 9.f);
    transform.set_rc(3, 1, 10.f);
    transform.set_rc(3, 2, 11.f);
    transform.set_rc(3, 3, 12.f);

    EXPECT_FALSE(EmpiricallyPreserves2dAxisAlignment(transform));
    EXPECT_FALSE(transform.Preserves2dAxisAlignment());
    EXPECT_FALSE(transform.NonDegeneratePreserves2dAxisAlignment());
  }

  // Try a few more practical situations to check precision
  transform.MakeIdentity();
  transform.RotateAboutZAxis(90.0);
  EXPECT_TRUE(EmpiricallyPreserves2dAxisAlignment(transform));
  EXPECT_TRUE(transform.Preserves2dAxisAlignment());
  EXPECT_TRUE(transform.NonDegeneratePreserves2dAxisAlignment());

  transform.MakeIdentity();
  transform.RotateAboutZAxis(180.0);
  EXPECT_TRUE(EmpiricallyPreserves2dAxisAlignment(transform));
  EXPECT_TRUE(transform.Preserves2dAxisAlignment());
  EXPECT_TRUE(transform.NonDegeneratePreserves2dAxisAlignment());

  transform.MakeIdentity();
  transform.RotateAboutZAxis(270.0);
  EXPECT_TRUE(EmpiricallyPreserves2dAxisAlignment(transform));
  EXPECT_TRUE(transform.Preserves2dAxisAlignment());
  EXPECT_TRUE(transform.NonDegeneratePreserves2dAxisAlignment());

  transform.MakeIdentity();
  transform.RotateAboutYAxis(90.0);
  EXPECT_TRUE(EmpiricallyPreserves2dAxisAlignment(transform));
  EXPECT_TRUE(transform.Preserves2dAxisAlignment());
  EXPECT_FALSE(transform.NonDegeneratePreserves2dAxisAlignment());

  transform.MakeIdentity();
  transform.RotateAboutXAxis(90.0);
  EXPECT_TRUE(EmpiricallyPreserves2dAxisAlignment(transform));
  EXPECT_TRUE(transform.Preserves2dAxisAlignment());
  EXPECT_FALSE(transform.NonDegeneratePreserves2dAxisAlignment());

  transform.MakeIdentity();
  transform.RotateAboutZAxis(90.0);
  transform.RotateAboutYAxis(90.0);
  EXPECT_TRUE(EmpiricallyPreserves2dAxisAlignment(transform));
  EXPECT_TRUE(transform.Preserves2dAxisAlignment());
  EXPECT_FALSE(transform.NonDegeneratePreserves2dAxisAlignment());

  transform.MakeIdentity();
  transform.RotateAboutZAxis(90.0);
  transform.RotateAboutXAxis(90.0);
  EXPECT_TRUE(EmpiricallyPreserves2dAxisAlignment(transform));
  EXPECT_TRUE(transform.Preserves2dAxisAlignment());
  EXPECT_FALSE(transform.NonDegeneratePreserves2dAxisAlignment());

  transform.MakeIdentity();
  transform.RotateAboutYAxis(90.0);
  transform.RotateAboutZAxis(90.0);
  EXPECT_TRUE(EmpiricallyPreserves2dAxisAlignment(transform));
  EXPECT_TRUE(transform.Preserves2dAxisAlignment());
  EXPECT_FALSE(transform.NonDegeneratePreserves2dAxisAlignment());

  transform.MakeIdentity();
  transform.RotateAboutZAxis(45.0);
  EXPECT_FALSE(EmpiricallyPreserves2dAxisAlignment(transform));
  EXPECT_FALSE(transform.Preserves2dAxisAlignment());
  EXPECT_FALSE(transform.NonDegeneratePreserves2dAxisAlignment());

  // 3-d case; In 2d after an orthographic projection, this case does
  // preserve 2d axis alignment. But in 3d, it does not preserve axis
  // alignment.
  transform.MakeIdentity();
  transform.RotateAboutYAxis(45.0);
  EXPECT_TRUE(EmpiricallyPreserves2dAxisAlignment(transform));
  EXPECT_TRUE(transform.Preserves2dAxisAlignment());
  EXPECT_TRUE(transform.NonDegeneratePreserves2dAxisAlignment());

  transform.MakeIdentity();
  transform.RotateAboutXAxis(45.0);
  EXPECT_TRUE(EmpiricallyPreserves2dAxisAlignment(transform));
  EXPECT_TRUE(transform.Preserves2dAxisAlignment());
  EXPECT_TRUE(transform.NonDegeneratePreserves2dAxisAlignment());

  // Perspective cases.
  transform.MakeIdentity();
  transform.ApplyPerspectiveDepth(10.0);
  transform.RotateAboutYAxis(45.0);
  EXPECT_FALSE(EmpiricallyPreserves2dAxisAlignment(transform));
  EXPECT_FALSE(transform.Preserves2dAxisAlignment());
  EXPECT_FALSE(transform.NonDegeneratePreserves2dAxisAlignment());

  transform.MakeIdentity();
  transform.ApplyPerspectiveDepth(10.0);
  transform.RotateAboutZAxis(90.0);
  EXPECT_TRUE(EmpiricallyPreserves2dAxisAlignment(transform));
  EXPECT_TRUE(transform.Preserves2dAxisAlignment());
  EXPECT_TRUE(transform.NonDegeneratePreserves2dAxisAlignment());

  transform.MakeIdentity();
  transform.ApplyPerspectiveDepth(-10.0);
  transform.RotateAboutZAxis(90.0);
  EXPECT_TRUE(EmpiricallyPreserves2dAxisAlignment(transform));
  EXPECT_TRUE(transform.Preserves2dAxisAlignment());
  EXPECT_TRUE(transform.NonDegeneratePreserves2dAxisAlignment());

  // To be non-degenerate, the constant contribution to perspective must
  // be positive.

  // clang-format off
  transform = Transform::RowMajor(1.0, 0.0, 0.0, 0.0,
                                  0.0, 1.0, 0.0, 0.0,
                                  0.0, 0.0, 1.0, 0.0,
                                  0.0, 0.0, 0.0, -1.0);
  // clang-format on
  EXPECT_TRUE(EmpiricallyPreserves2dAxisAlignment(transform));
  EXPECT_TRUE(transform.Preserves2dAxisAlignment());
  EXPECT_FALSE(transform.NonDegeneratePreserves2dAxisAlignment());

  // clang-format off
  transform = Transform::RowMajor(2.0, 0.0, 0.0, 0.0,
                                  0.0, 5.0, 0.0, 0.0,
                                  0.0, 0.0, 1.0, 0.0,
                                  0.0, 0.0, 0.0, 0.0);
  // clang-format on
  EXPECT_TRUE(EmpiricallyPreserves2dAxisAlignment(transform));
  EXPECT_TRUE(transform.Preserves2dAxisAlignment());
  EXPECT_FALSE(transform.NonDegeneratePreserves2dAxisAlignment());
}

TEST(XFormTest, To2dTranslation) {
  Vector2dF translation(3.f, 7.f);
  Transform transform;
  transform.Translate(translation.x(), translation.y() + 1);
  EXPECT_NE(translation.ToString(), transform.To2dTranslation().ToString());
  transform.MakeIdentity();
  transform.Translate(translation.x(), translation.y());
  EXPECT_EQ(translation.ToString(), transform.To2dTranslation().ToString());
}

TEST(XFormTest, MapRect) {
  Transform translation = Transform::MakeTranslation(3.25f, 7.75f);
  RectF rect(1.25f, 2.5f, 3.75f, 4.f);
  RectF expected(4.5f, 10.25f, 3.75f, 4.f);
  EXPECT_EQ(expected, translation.MapRect(rect));

  EXPECT_EQ(rect, Transform().MapRect(rect));

  auto singular = Transform::MakeScale(0.f);
  EXPECT_EQ(RectF(0, 0, 0, 0), singular.MapRect(rect));
}

TEST(XFormTest, MapIntRect) {
  auto translation = Transform::MakeTranslation(3.25f, 7.75f);
  EXPECT_EQ(Rect(4, 9, 4, 5), translation.MapRect(Rect(1, 2, 3, 4)));

  EXPECT_EQ(Rect(1, 2, 3, 4), Transform().MapRect(Rect(1, 2, 3, 4)));

  auto singular = Transform::MakeScale(0.f);
  EXPECT_EQ(Rect(0, 0, 0, 0), singular.MapRect(Rect(1, 2, 3, 4)));
}

TEST(XFormTest, TransformRectReverse) {
  auto translation = Transform::MakeTranslation(3.25f, 7.75f);
  RectF rect(1.25f, 2.5f, 3.75f, 4.f);
  RectF expected(-2.f, -5.25f, 3.75f, 4.f);
  EXPECT_EQ(expected, translation.InverseMapRect(rect));

  EXPECT_EQ(rect, Transform().InverseMapRect(rect));

  auto singular = Transform::MakeScale(0.f);
  EXPECT_FALSE(singular.InverseMapRect(rect));
}

TEST(XFormTest, InverseMapIntRect) {
  auto translation = Transform::MakeTranslation(3.25f, 7.75f);
  EXPECT_EQ(Rect(-3, -6, 4, 5), translation.InverseMapRect(Rect(1, 2, 3, 4)));

  EXPECT_EQ(Rect(1, 2, 3, 4), Transform().InverseMapRect(Rect(1, 2, 3, 4)));

  auto singular = Transform::MakeScale(0.f);
  EXPECT_FALSE(singular.InverseMapRect(Rect(1, 2, 3, 4)));
}

TEST(XFormTest, MapBox) {
  Transform translation;
  translation.Translate3d(3.f, 7.f, 6.f);
  BoxF box(1.f, 2.f, 3.f, 4.f, 5.f, 6.f);
  BoxF expected(4.f, 9.f, 9.f, 4.f, 5.f, 6.f);
  BoxF transformed = translation.MapBox(box);
  EXPECT_EQ(expected, transformed);
}

TEST(XFormTest, RoundTranslationComponents) {
  Transform translation;
  Transform expected;

  translation.RoundTranslationComponents();
  EXPECT_EQ(expected.ToString(), translation.ToString());

  translation.Translate(1.0f, 1.0f);
  expected.Translate(1.0f, 1.0f);
  translation.RoundTranslationComponents();
  EXPECT_EQ(expected.ToString(), translation.ToString());

  translation.Translate(0.5f, 0.4f);
  expected.Translate(1.0f, 0.0f);
  translation.RoundTranslationComponents();
  EXPECT_EQ(expected.ToString(), translation.ToString());

  // Rounding should only affect 2d translation components.
  translation.Translate3d(0.f, 0.f, 0.5f);
  expected.Translate3d(0.f, 0.f, 0.5f);
  translation.RoundTranslationComponents();
  EXPECT_EQ(expected.ToString(), translation.ToString());
}

TEST(XFormTest, BackFaceVisiblilityTolerance) {
  Transform backface_invisible;
  backface_invisible.set_rc(0, 3, 1.f);
  backface_invisible.set_rc(3, 0, 1.f);
  backface_invisible.set_rc(2, 0, 1.f);
  backface_invisible.set_rc(3, 2, 1.f);

  // The transformation matrix has a determinant = 1 and cofactor33 = 0. So,
  // IsBackFaceVisible should return false.
  EXPECT_EQ(backface_invisible.Determinant(), 1.f);
  EXPECT_FALSE(backface_invisible.IsBackFaceVisible());

  // Adding a noise to the transformsation matrix that is within the tolerance
  // (machine epsilon) should not change the result.
  float noise = std::numeric_limits<float>::epsilon();
  backface_invisible.set_rc(0, 3, 1.f + noise);
  EXPECT_FALSE(backface_invisible.IsBackFaceVisible());

  // A noise that is more than the tolerance should change the result.
  backface_invisible.set_rc(0, 3, 1.f + (2 * noise));
  EXPECT_TRUE(backface_invisible.IsBackFaceVisible());
}

TEST(XFormTest, TransformVector4) {
  Transform transform;
  transform.set_rc(0, 0, 2.5f);
  transform.set_rc(1, 1, 3.5f);
  transform.set_rc(2, 2, 4.5f);
  transform.set_rc(3, 3, 5.5f);
  float v[] = {11.5f, 22.5f, 33.5f, 44.5f};
  transform.TransformVector4(v);
  EXPECT_EQ(28.75f, v[0]);
  EXPECT_EQ(78.75f, v[1]);
  EXPECT_EQ(150.75f, v[2]);
  EXPECT_EQ(244.75f, v[3]);
}

TEST(XFormTest, Make90NRotation) {
  auto t1 = Transform::Make90degRotation();
  EXPECT_EQ(gfx::PointF(-50, 100), t1.MapPoint(gfx::PointF(100, 50)));

  auto t2 = Transform::Make180degRotation();
  EXPECT_EQ(Transform::MakeScale(-1), t2);
  EXPECT_EQ(gfx::PointF(-100, -50), t2.MapPoint(gfx::PointF(100, 50)));

  auto t3 = Transform::Make270degRotation();
  EXPECT_EQ(gfx::PointF(50, -100), t3.MapPoint(gfx::PointF(100, 50)));

  auto t4 = t1 * t1;
  EXPECT_EQ(t2, t4);
  t4.PreConcat(t1);
  EXPECT_EQ(t3, t4);
  t4.PreConcat(t1);
  EXPECT_TRUE(t4.IsIdentity());
  t2.PreConcat(t2);
  EXPECT_TRUE(t2.IsIdentity());
}

TEST(XFormTest, MapPoint) {
  Transform transform;
  transform.Translate3d(1.25f, 2.75f, 3.875f);
  transform.Scale3d(3, 4, 5);
  EXPECT_EQ(PointF(38.75f, 140.75f), transform.MapPoint(PointF(12.5f, 34.5f)));
  EXPECT_EQ(Point3F(38.75f, 140.75f, 286.375),
            transform.MapPoint(Point3F(12.5f, 34.5f, 56.5f)));

  transform.MakeIdentity();
  transform.set_rc(3, 0, 0.5);
  transform.set_rc(3, 1, 2);
  transform.set_rc(3, 2, 0.75);
  EXPECT_POINTF_EQ(PointF(0.2, 0.4), transform.MapPoint(PointF(2, 4)));
  EXPECT_POINT3F_EQ(Point3F(0.18181818f, 0.27272727f, 0.36363636f),
                    transform.MapPoint(Point3F(2, 3, 4)));

  // 0 in all perspectives should be ignored.
  transform.MakeIdentity();
  transform.Translate3d(10, 20, 30);
  transform.set_rc(3, 3, 0);
  EXPECT_EQ(PointF(12, 24), transform.MapPoint(PointF(2, 4)));
  EXPECT_EQ(Point3F(12, 23, 34), transform.MapPoint(Point3F(2, 3, 4)));

  // NaN in perspective should be ignored.
  transform.set_rc(3, 3, std::numeric_limits<float>::quiet_NaN());
  EXPECT_EQ(PointF(12, 24), transform.MapPoint(PointF(2, 4)));
  EXPECT_EQ(Point3F(12, 23, 34), transform.MapPoint(Point3F(2, 3, 4)));
}

TEST(XFormTest, InverseMapPoint) {
  Transform transform;
  transform.Translate(1, 2);
  transform.Rotate(70);
  transform.Scale(3, 4);
  transform.Skew(30, 70);

  const PointF point_f(12.34f, 56.78f);
  PointF transformed_point_f = transform.MapPoint(point_f);
  const absl::optional<PointF> reverted_point_f =
      transform.InverseMapPoint(transformed_point_f);
  ASSERT_TRUE(reverted_point_f.has_value());
  EXPECT_TRUE(PointsAreNearlyEqual(reverted_point_f.value(), point_f));

  const Point point(12, 13);
  Point transformed_point = transform.MapPoint(point);
  EXPECT_EQ(point, transform.InverseMapPoint(transformed_point));

  Transform transform3d;
  transform3d.Translate3d(1, 2, 3);
  transform3d.RotateAbout(Vector3dF(4, 5, 6), 70);
  transform3d.Scale3d(7, 8, 9);
  transform3d.Skew(30, 70);

  const Point3F point_3f(14, 15, 16);
  Point3F transformed_point_3f = transform3d.MapPoint(point_3f);
  const absl::optional<Point3F> reverted_point_3f =
      transform3d.InverseMapPoint(transformed_point_3f);
  ASSERT_TRUE(reverted_point_3f.has_value());
  EXPECT_TRUE(PointsAreNearlyEqual(reverted_point_3f.value(), point_3f));
}

TEST(XFormTest, MapVector) {
  Transform transform;
  transform.Scale3d(3, 4, 5);
  Vector3dF vector(12.5f, 34.5f, 56.5f);
  Vector3dF expected(37.5f, 138.0f, 282.5f);
  EXPECT_EQ(expected, transform.MapVector(vector));

  // The translation components should be ignored.
  transform.Translate3d(1.25f, 2.75f, 3.875f);
  EXPECT_EQ(expected, transform.MapVector(vector));

  // The perspective components should be ignored.
  transform.set_rc(3, 0, 0.5f);
  transform.set_rc(3, 1, 2.5f);
  transform.set_rc(3, 2, 4.5f);
  transform.set_rc(3, 3, 8.5f);
  EXPECT_EQ(expected, transform.MapVector(vector));
}

TEST(XFormTest, PreConcatAxisTransform2d) {
  auto t = Transform::RowMajor(2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15,
                               16, 17);
  auto axis = AxisTransform2d::FromScaleAndTranslation(Vector2dF(10, 20),
                                                       Vector2dF(100, 200));
  auto axis_full =
      Transform::MakeTranslation(100, 200) * Transform::MakeScale(10, 20);
  auto t1 = t;
  t.PreConcat(axis);
  t1.PreConcat(axis_full);
  EXPECT_EQ(t, t1);
}

TEST(XFormTest, PostConcatAxisTransform2d) {
  auto t = Transform::RowMajor(2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15,
                               16, 17);
  auto axis = AxisTransform2d::FromScaleAndTranslation(Vector2dF(10, 20),
                                                       Vector2dF(100, 200));
  auto axis_full =
      Transform::MakeTranslation(100, 200) * Transform::MakeScale(10, 20);
  auto t1 = t;
  t.PostConcat(axis);
  t1.PostConcat(axis_full);
  EXPECT_EQ(t, t1);
}

TEST(TransformationMatrixTest, ClampOutput) {
  double entries[][2] = {
      // The first entry is used to initialize the transform.
      // The second entry is used to initialize the object to be mapped.
      {std::numeric_limits<float>::max(),
       std::numeric_limits<float>::infinity()},
      {1, std::numeric_limits<float>::infinity()},
      {-1, std::numeric_limits<float>::infinity()},
      {1, -std::numeric_limits<float>::infinity()},
      {
          std::numeric_limits<float>::max(),
          std::numeric_limits<float>::max(),
      },
      {
          std::numeric_limits<float>::lowest(),
          -std::numeric_limits<float>::infinity(),
      },
  };

  for (double* entry : entries) {
    const float mv = entry[0];
    const float factor = entry[1];

    auto is_valid_point = [&](const PointF& p) -> bool {
      return std::isfinite(p.x()) && std::isfinite(p.y());
    };
    auto is_valid_point3 = [&](const Point3F& p) -> bool {
      return std::isfinite(p.x()) && std::isfinite(p.y()) &&
             std::isfinite(p.z());
    };
    auto is_valid_vector2 = [&](const Vector2dF& v) -> bool {
      return std::isfinite(v.x()) && std::isfinite(v.y());
    };
    auto is_valid_vector3 = [&](const Vector3dF& v) -> bool {
      return std::isfinite(v.x()) && std::isfinite(v.y()) &&
             std::isfinite(v.z());
    };
    auto is_valid_rect = [&](const RectF& r) -> bool {
      return is_valid_point(r.origin()) && std::isfinite(r.width()) &&
             std::isfinite(r.height());
    };
    auto is_valid_array = [&](const float* a, size_t size) -> bool {
      for (size_t i = 0; i < size; i++) {
        if (!std::isfinite(a[i]))
          return false;
      }
      return true;
    };

    auto test = [&](const Transform& m) {
      SCOPED_TRACE(base::StringPrintf("m: %s factor: %lg", m.ToString().c_str(),
                                      factor));
      auto p = m.MapPoint(PointF(factor, factor));
      EXPECT_TRUE(is_valid_point(p)) << p.ToString();

      auto p3 = m.MapPoint(Point3F(factor, factor, factor));
      EXPECT_TRUE(is_valid_point3(p3)) << p3.ToString();

      auto r = m.MapRect(RectF(factor, factor, factor, factor));
      EXPECT_TRUE(is_valid_rect(r)) << r.ToString();

      auto v3 = m.MapVector(Vector3dF(factor, factor, factor));
      EXPECT_TRUE(is_valid_vector3(v3)) << v3.ToString();

      float v4[4] = {factor, factor, factor, factor};
      m.TransformVector4(v4);
      EXPECT_TRUE(is_valid_array(v4, 4));

      auto v2 = m.To2dTranslation();
      EXPECT_TRUE(is_valid_vector2(v2)) << v2.ToString();
      v2 = m.To2dScale();
      EXPECT_TRUE(is_valid_vector2(v2)) << v2.ToString();
    };

    test(Transform::ColMajor(mv, mv, mv, mv, mv, mv, mv, mv, mv, mv, mv, mv, mv,
                             mv, mv, mv));
    test(Transform::MakeTranslation(mv, mv));
  }
}

}  // namespace

}  // namespace gfx
