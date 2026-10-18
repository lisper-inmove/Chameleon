#pragma once

#include <QDialog>

#include "processing/binary.h"

class QComboBox;
class QLabel;

namespace chameleon {

// Connected components labeling dialog: connectivity (4/8) and the API
// (connectedComponents vs WithStats). The colorized label map opens in a
// result window; with statistics enabled the component count, areas and
// bounding boxes are listed here.
class ConnectedComponentsDialog : public QDialog
{
    Q_OBJECT
public:
    explicit ConnectedComponentsDialog(QWidget *parent = nullptr);

    ConnectedComponentsParams connectedComponentsParams() const;
    void setStats(const ConnectedComponentsResult &result);

signals:
    void computeRequested();

private:
    QComboBox *m_connectivityCombo = nullptr;
    QComboBox *m_methodCombo = nullptr;
    QLabel *m_statsLabel = nullptr;
};

} // namespace chameleon
