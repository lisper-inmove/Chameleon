#pragma once

#include <QDialog>

#include "processing/binary.h"

class QCheckBox;
class QLabel;
class QTableWidget;

namespace chameleon {

// Convex hull dialog: cv::convexHull over every detected contour, with
// cv::isContourConvex reporting the contour convexity. The hull
// orientation and the point/index result mode are settable.
class ConvexHullDialog : public QDialog
{
    Q_OBJECT
public:
    explicit ConvexHullDialog(QWidget *parent = nullptr);

    ConvexHullParams hullParams() const;
    void setResult(const ConvexHullResult &result);

signals:
    void applyRequested();

private:
    QCheckBox *m_clockwiseCheck = nullptr;
    QCheckBox *m_returnPointsCheck = nullptr;
    QLabel *m_countLabel = nullptr;
    QTableWidget *m_table = nullptr;
};

} // namespace chameleon
