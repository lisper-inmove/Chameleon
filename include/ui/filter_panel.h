#pragma once

#include <QWidget>

namespace chameleon {

// Placeholder panel; filter parameters get wired up with the filter feature.
class FilterPanel : public QWidget
{
    Q_OBJECT
public:
    explicit FilterPanel(QWidget *parent = nullptr);
};

} // namespace chameleon
