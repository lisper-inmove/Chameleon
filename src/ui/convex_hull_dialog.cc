#include "ui/convex_hull_dialog.h"

#include <QCheckBox>
#include <QFormLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>

namespace chameleon {

ConvexHullDialog::ConvexHullDialog(QWidget *parent) : QDialog(parent)
{
    setWindowTitle(QStringLiteral("凸包检测"));
    setObjectName(QStringLiteral("convexHullDialog"));
    setAttribute(Qt::WA_DeleteOnClose);

    m_clockwiseCheck = new QCheckBox(QStringLiteral("顺时针方向"), this);
    m_clockwiseCheck->setObjectName(QStringLiteral("convexClockwiseCheck"));

    m_returnPointsCheck =
        new QCheckBox(QStringLiteral("返回点坐标(取消则返回索引)"), this);
    m_returnPointsCheck->setObjectName(
        QStringLiteral("convexReturnPointsCheck"));
    m_returnPointsCheck->setChecked(true);

    m_countLabel = new QLabel(QStringLiteral("检测到 -- 个轮廓"), this);
    m_countLabel->setObjectName(QStringLiteral("convexHullCountLabel"));

    m_table = new QTableWidget(0, 5, this);
    m_table->setObjectName(QStringLiteral("convexHullTable"));
    m_table->setHorizontalHeaderLabels(
        {QStringLiteral("编号"), QStringLiteral("凸性"),
         QStringLiteral("凸包顶点数"), QStringLiteral("凸包面积"),
         QStringLiteral("凸包周长")});
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->verticalHeader()->setVisible(false);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setMinimumHeight(180);

    auto *hint = new QLabel(this);
    hint->setWordWrap(true);
    hint->setText(QStringLiteral(
        "对当前图片(或选区)二值化后提取轮廓,用 cv::convexHull 计算凸包、"
        "cv::isContourConvex 判断凸性;图像上绿色为轮廓、红色为凸包。"
        "支持选区,可撤销。"));

    auto *applyButton = new QPushButton(QStringLiteral("应用"), this);
    applyButton->setObjectName(QStringLiteral("convexHullApplyButton"));
    connect(applyButton, &QPushButton::clicked, this,
            &ConvexHullDialog::applyRequested);

    auto *form = new QFormLayout;
    form->addRow(QString(), m_clockwiseCheck);
    form->addRow(QString(), m_returnPointsCheck);

    auto *layout = new QVBoxLayout(this);
    layout->addLayout(form);
    layout->addWidget(m_countLabel);
    layout->addWidget(m_table, 1);
    layout->addWidget(hint);
    layout->addWidget(applyButton);
    resize(560, 460);
}

ConvexHullParams ConvexHullDialog::hullParams() const
{
    ConvexHullParams params;
    params.clockwise = m_clockwiseCheck->isChecked();
    params.returnPoints = m_returnPointsCheck->isChecked();
    return params;
}

void ConvexHullDialog::setResult(const ConvexHullResult &result)
{
    int convexCount = 0;
    m_table->setRowCount(int(result.infos.size()));
    for (int row = 0; row < int(result.infos.size()); ++row) {
        const ConvexHullInfo &info = result.infos[size_t(row)];
        if (info.convex)
            ++convexCount;
        m_table->setItem(
            row, 0, new QTableWidgetItem(QString::number(info.index)));
        m_table->setItem(
            row, 1,
            new QTableWidgetItem(info.convex ? QStringLiteral("是")
                                             : QStringLiteral("否")));
        m_table->setItem(row, 2,
                         new QTableWidgetItem(QString::number(info.hullPointCount)));
        m_table->setItem(
            row, 3,
            new QTableWidgetItem(QString::number(info.hullArea, 'f', 1)));
        m_table->setItem(
            row, 4,
            new QTableWidgetItem(QString::number(info.hullPerimeter, 'f', 1)));
    }
    m_countLabel->setText(QStringLiteral("检测到 %1 个轮廓,其中 %2 个凸轮廓")
                              .arg(result.infos.size())
                              .arg(convexCount));
}

} // namespace chameleon
