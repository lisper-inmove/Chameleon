#pragma once

#include <QWidget>

namespace chameleon {

// Placeholder panel; the histogram gets implemented with the image pipeline.
class HistogramPanel : public QWidget
{
    Q_OBJECT
public:
    explicit HistogramPanel(QWidget *parent = nullptr);
};

} // namespace chameleon
