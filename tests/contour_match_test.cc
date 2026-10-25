#include <gtest/gtest.h>

#include <cmath>

#include <opencv2/imgproc.hpp>

#include "processing/contour_match.h"

using chameleon::ContourMatchParams;
using chameleon::matchContours;

namespace {

cv::Mat shapeImage(int size, const cv::Rect &square)
{
    cv::Mat image(size, size, CV_8UC1, cv::Scalar(0));
    cv::rectangle(image, square, cv::Scalar(255), cv::FILLED);
    return image;
}

cv::Mat filledCircle(int size, cv::Point center, int radius)
{
    cv::Mat image(size, size, CV_8UC1, cv::Scalar(0));
    cv::circle(image, center, radius, cv::Scalar(255), cv::FILLED);
    return image;
}

} // namespace

TEST(ContourMatchTest, IdenticalShapesScoreZero)
{
    const cv::Mat query = shapeImage(100, cv::Rect(20, 20, 30, 30));
    // The same square at another position and scale: Hu moments are
    // translation and scale invariant
    const cv::Mat templ = shapeImage(120, cv::Rect(10, 10, 50, 50));

    ContourMatchParams params;  // CONTOURS_MATCH_I1
    const auto result = matchContours(query, templ, params);

    ASSERT_EQ(result.matches.size(), 1u);
    EXPECT_LT(result.matches[0].score, 1e-6);
    EXPECT_EQ(result.bestIndex, 1);
    // cv::moments / cv::HuMoments data is reported
    EXPECT_GT(result.matches[0].moments.m00, 0.0);
    EXPECT_NE(result.matches[0].hu[0], 0.0);
}

TEST(ContourMatchTest, DifferentShapesScoreHigher)
{
    const cv::Mat query = shapeImage(100, cv::Rect(20, 20, 40, 40));
    const cv::Mat squareTemplate = shapeImage(100, cv::Rect(30, 30, 30, 30));
    const cv::Mat circle = filledCircle(100, cv::Point(50, 50), 20);

    ContourMatchParams params;
    const auto asSquare = matchContours(query, squareTemplate, params);
    const auto asCircle = matchContours(query, circle, params);

    ASSERT_EQ(asSquare.matches.size(), 1u);
    ASSERT_EQ(asCircle.matches.size(), 1u);
    EXPECT_LT(asSquare.matches[0].score, asCircle.matches[0].score);
    EXPECT_GT(asCircle.matches[0].score, 1e-3);
}

TEST(ContourMatchTest, BestMatchIndexSelectsTheClosestShape)
{
    cv::Mat query(120, 120, CV_8UC1, cv::Scalar(0));
    cv::rectangle(query, cv::Rect(10, 10, 25, 25), cv::Scalar(255),
                  cv::FILLED);                                     // 1: square
    cv::circle(query, cv::Point(80, 85), 15, cv::Scalar(255), cv::FILLED);  // 2: circle
    const cv::Mat templ = filledCircle(100, cv::Point(50, 50), 25);

    ContourMatchParams params;
    const auto result = matchContours(query, templ, params);

    ASSERT_EQ(result.matches.size(), 2u);
    // Rows are ordered by similarity: the closest shape comes first
    EXPECT_EQ(result.matches[0].index, 2);
    EXPECT_EQ(result.matches[1].index, 1);
    EXPECT_EQ(result.bestIndex, 2);  // the circle is the closer shape
    EXPECT_LT(result.matches[0].score, result.matches[1].score);
}

TEST(ContourMatchTest, SupportsAllMatchMethods)
{
    const cv::Mat query = shapeImage(100, cv::Rect(20, 20, 30, 30));
    const cv::Mat templ = filledCircle(100, cv::Point(50, 50), 20);

    for (const int method : {cv::CONTOURS_MATCH_I1, cv::CONTOURS_MATCH_I2,
                             cv::CONTOURS_MATCH_I3}) {
        ContourMatchParams params;
        params.method = method;
        const auto result = matchContours(query, templ, params);
        ASSERT_EQ(result.matches.size(), 1u);
        EXPECT_TRUE(std::isfinite(result.matches[0].score));
        EXPECT_GE(result.matches[0].score, 0.0);
    }
}

TEST(ContourMatchTest, MissingTemplateContoursProduceNoMatches)
{
    const cv::Mat query = shapeImage(100, cv::Rect(20, 20, 30, 30));
    const cv::Mat blank(80, 80, CV_8UC1, cv::Scalar(0));

    ContourMatchParams params;
    const auto result = matchContours(query, blank, params);
    EXPECT_TRUE(result.matches.empty());
    EXPECT_EQ(result.bestIndex, 0);
}

TEST(ContourMatchTest, DrawsTheContoursAndHighlightsTheBest)
{
    const cv::Mat query = shapeImage(100, cv::Rect(20, 20, 30, 30));
    const cv::Mat templ = shapeImage(100, cv::Rect(30, 30, 30, 30));

    ContourMatchParams params;
    const auto result = matchContours(query, templ, params);
    ASSERT_FALSE(result.image.empty());
    cv::Mat diff;
    cv::absdiff(result.image, query, diff);
    EXPECT_GT(cv::countNonZero(diff), 0);
}
