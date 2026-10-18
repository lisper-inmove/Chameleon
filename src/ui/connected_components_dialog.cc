#include "ui/connected_components_dialog.h"

#include <algorithm>
#include <numeric>

#include <QComboBox>
#include <QFormLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

namespace chameleon {

ConnectedComponentsDialog::ConnectedComponentsDialog(QWidget *parent) : QDialog(parent)
{
    setWindowTitle(QStringLiteral("连通域标记"));
    setObjectName(QStringLiteral("connectedComponentsDialog"));
    setAttribute(Qt::WA_DeleteOnClose);

    m_connectivityCombo = new QComboBox(this);
    m_connectivityCombo->setObjectName(QStringLiteral("ccConnectivityCombo"));
    m_connectivityCombo->addItem(QStringLiteral("8 邻域"));
    m_connectivityCombo->addItem(QStringLiteral("4 邻域"));

    m_methodCombo = new QComboBox(this);
    m_methodCombo->setObjectName(QStringLiteral("ccMethodCombo"));
    m_methodCombo->addItem(QStringLiteral("仅标记(connectedComponents)"));
    m_methodCombo->addItem(QStringLiteral("标记 + 统计(WithStats)"));

    auto *computeButton = new QPushButton(QStringLiteral("计算并显示"), this);
    computeButton->setObjectName(QStringLiteral("ccComputeButton"));
    connect(computeButton, &QPushButton::clicked,
            this, &ConnectedComponentsDialog::computeRequested);

    m_statsLabel = new QLabel(this);
    m_statsLabel->setObjectName(QStringLiteral("ccStatsLabel"));
    m_statsLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_statsLabel->setText(QStringLiteral("组件数:--"));
    m_statsLabel->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    m_statsLabel->setMinimumHeight(150);

    auto *hint = new QLabel(this);
    hint->setWordWrap(true);
    hint->setText(QStringLiteral(
        "对当前图片(或选区)的亮度做连通域标记:结果以彩色标记图在新窗口展示。"
        "通常先做阈值化分割;统计模式给出组件的面积、包围盒与质心。"));

    auto *form = new QFormLayout;
    form->addRow(QStringLiteral("连通性:"), m_connectivityCombo);
    form->addRow(QStringLiteral("算法:"), m_methodCombo);

    auto *layout = new QVBoxLayout(this);
    layout->addLayout(form);
    layout->addWidget(m_statsLabel);
    layout->addWidget(hint);
    layout->addWidget(computeButton);
    resize(430, 360);
}

ConnectedComponentsParams ConnectedComponentsDialog::connectedComponentsParams() const
{
    ConnectedComponentsParams params;
    params.connectivity = m_connectivityCombo->currentIndex() == 1 ? 4 : 8;
    params.withStats = m_methodCombo->currentIndex() == 1;
    return params;
}

void ConnectedComponentsDialog::setStats(const ConnectedComponentsResult &result)
{
    QString text = QStringLiteral("组件数:%1(不含背景)")
                       .arg(std::max(0, result.count - 1));
    if (!result.areas.empty() && result.count > 0) {
        std::vector<int> order(size_t(result.count));
        std::iota(order.begin(), order.end(), 0);
        std::sort(order.begin(), order.end(),
                  [&result](int a, int b) { return result.areas[size_t(a)] > result.areas[size_t(b)]; });
        const size_t shown = std::min<size_t>(10, order.size());
        text += QStringLiteral("\n按面积排序(前 %1 个):").arg(shown);
        for (size_t i = 0; i < shown; ++i) {
            const int label = order[i];
            const cv::Rect &box = result.boundingBoxes[size_t(label)];
            text += QStringLiteral("\n%1:面积=%2, 包围盒=(%3,%4,%5,%6)")
                        .arg(label == 0 ? QStringLiteral("背景")
                                        : QStringLiteral("组件 %1").arg(label))
                        .arg(result.areas[size_t(label)])
                        .arg(box.x)
                        .arg(box.y)
                        .arg(box.width)
                        .arg(box.height);
        }
        if (order.size() > shown)
            text += QStringLiteral("\n……共 %1 个").arg(order.size());
    }
    m_statsLabel->setText(text);
}

} // namespace chameleon
