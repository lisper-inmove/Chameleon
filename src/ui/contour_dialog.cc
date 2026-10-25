#include "ui/contour_dialog.h"

#include <algorithm>
#include <cmath>

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QTableWidget>
#include <QTabWidget>
#include <QVBoxLayout>

namespace chameleon {

namespace {

const int kRetrievalModes[4] = {cv::RETR_EXTERNAL, cv::RETR_LIST,
                                cv::RETR_CCOMP, cv::RETR_TREE};
const int kApproximationMethods[4] = {
    cv::CHAIN_APPROX_SIMPLE, cv::CHAIN_APPROX_NONE, cv::CHAIN_APPROX_TC89_L1,
    cv::CHAIN_APPROX_TC89_KCOS};
const int kLineTypes[3] = {cv::LINE_4, cv::LINE_8, cv::LINE_AA};
const int kDistanceTypes[6] = {cv::DIST_L2,    cv::DIST_L1,   cv::DIST_L12,
                               cv::DIST_FAIR,  cv::DIST_WELSCH,
                               cv::DIST_HUBER};

int currentValue(const QComboBox *combo, const int *values, int count)
{
    const int index = std::clamp(combo->currentIndex(), 0, count - 1);
    return values[index];
}

QDoubleSpinBox *makeDoubleSpin(QWidget *parent, const char *name, double min,
                               double max, double value,  // NOLINT(bugprone-easily-swappable-parameters) - fixed order
                               int decimals)
{
    auto *spin = new QDoubleSpinBox(parent);
    spin->setObjectName(QString::fromUtf8(name));
    spin->setRange(min, max);
    spin->setDecimals(decimals);
    spin->setValue(value);
    return spin;
}

} // namespace

ContourDialog::ContourDialog(QWidget *parent) : QDialog(parent)
{
    setWindowTitle(QStringLiteral("轮廓发现"));
    setObjectName(QStringLiteral("contourDialog"));
    setAttribute(Qt::WA_DeleteOnClose);

    m_modeCombo = new QComboBox(this);
    m_modeCombo->setObjectName(QStringLiteral("contourModeCombo"));
    m_modeCombo->addItem(QStringLiteral("仅最外层(RETR_EXTERNAL)"));
    m_modeCombo->addItem(QStringLiteral("全部(RETR_LIST)"));
    m_modeCombo->addItem(QStringLiteral("两级层级(RETR_CCOMP)"));
    m_modeCombo->addItem(QStringLiteral("完整层级(RETR_TREE)"));

    m_methodCombo = new QComboBox(this);
    m_methodCombo->setObjectName(QStringLiteral("contourMethodCombo"));
    m_methodCombo->addItem(QStringLiteral("简化(CHAIN_APPROX_SIMPLE)"));
    m_methodCombo->addItem(QStringLiteral("精确(CHAIN_APPROX_NONE)"));
    m_methodCombo->addItem(QStringLiteral("TC89-L1"));
    m_methodCombo->addItem(QStringLiteral("TC89-KCOS"));

    m_offsetXSpin = new QSpinBox(this);
    m_offsetXSpin->setObjectName(QStringLiteral("contourOffsetXSpin"));
    m_offsetXSpin->setRange(-4096, 4096);
    m_offsetXSpin->setValue(0);

    m_offsetYSpin = new QSpinBox(this);
    m_offsetYSpin->setObjectName(QStringLiteral("contourOffsetYSpin"));
    m_offsetYSpin->setRange(-4096, 4096);
    m_offsetYSpin->setValue(0);

    auto *offsetRow = new QWidget(this);
    auto *offsetLayout = new QHBoxLayout(offsetRow);
    offsetLayout->setContentsMargins(0, 0, 0, 0);
    offsetLayout->addWidget(m_offsetXSpin);
    offsetLayout->addWidget(new QLabel(QStringLiteral(","), offsetRow));
    offsetLayout->addWidget(m_offsetYSpin);

    m_thresholdSpin = new QSpinBox(this);
    m_thresholdSpin->setObjectName(QStringLiteral("contourThresholdSpin"));
    m_thresholdSpin->setRange(0, 255);
    m_thresholdSpin->setValue(128);

    m_otsuCheck = new QCheckBox(QStringLiteral("自动阈值(Otsu)"), this);
    m_otsuCheck->setObjectName(QStringLiteral("contourOtsuCheck"));
    m_otsuCheck->setChecked(true);

    m_invertCheck = new QCheckBox(QStringLiteral("反相"), this);
    m_invertCheck->setObjectName(QStringLiteral("contourInvertCheck"));

    auto *binaryRow = new QWidget(this);
    auto *binaryLayout = new QHBoxLayout(binaryRow);
    binaryLayout->setContentsMargins(0, 0, 0, 0);
    binaryLayout->addWidget(m_thresholdSpin);
    binaryLayout->addWidget(m_otsuCheck);
    binaryLayout->addWidget(m_invertCheck);

    m_thicknessSpin = new QSpinBox(this);
    m_thicknessSpin->setObjectName(QStringLiteral("contourThicknessSpin"));
    m_thicknessSpin->setRange(1, 20);
    m_thicknessSpin->setValue(2);

    m_lineTypeCombo = new QComboBox(this);
    m_lineTypeCombo->setObjectName(QStringLiteral("contourLineTypeCombo"));
    m_lineTypeCombo->addItem(QStringLiteral("4 连通"));
    m_lineTypeCombo->addItem(QStringLiteral("8 连通"));
    m_lineTypeCombo->addItem(QStringLiteral("抗锯齿"));
    m_lineTypeCombo->setCurrentIndex(2);

    m_largestCheck = new QCheckBox(QStringLiteral("只绘制最大轮廓"), this);
    m_largestCheck->setObjectName(QStringLiteral("contourLargestCheck"));

    // cv::pointPolygonTest query controls
    m_queryXSpin = new QSpinBox(this);
    m_queryXSpin->setObjectName(QStringLiteral("contourQueryXSpin"));
    m_queryXSpin->setRange(0, 16384);
    m_queryYSpin = new QSpinBox(this);
    m_queryYSpin->setObjectName(QStringLiteral("contourQueryYSpin"));
    m_queryYSpin->setRange(0, 16384);
    m_measureDistCheck = new QCheckBox(QStringLiteral("测距"), this);
    m_measureDistCheck->setObjectName(
        QStringLiteral("contourMeasureDistCheck"));
    m_measureDistCheck->setChecked(true);

    auto *queryRow = new QWidget(this);
    auto *queryLayout = new QHBoxLayout(queryRow);
    queryLayout->setContentsMargins(0, 0, 0, 0);
    queryLayout->addWidget(m_queryXSpin);
    queryLayout->addWidget(new QLabel(QStringLiteral(","), queryRow));
    queryLayout->addWidget(m_queryYSpin);
    queryLayout->addWidget(m_measureDistCheck);

    m_countLabel = new QLabel(QStringLiteral("检测到 -- 个轮廓"), this);
    m_countLabel->setObjectName(QStringLiteral("contourCountLabel"));

    m_statsTable = new QTableWidget(0, 10, this);
    m_statsTable->setObjectName(QStringLiteral("contourStatsTable"));
    m_statsTable->setHorizontalHeaderLabels(
        {QStringLiteral("编号"), QStringLiteral("面积"),
         QStringLiteral("周长"), QStringLiteral("包围盒 (x, y, 宽, 高)"),
         QStringLiteral("横纵比"), QStringLiteral("延展度"),
         QStringLiteral("实密度"), QStringLiteral("均值"),
         QStringLiteral("外接圆 (圆心, 半径)"), QStringLiteral("点包含")});
    m_statsTable->horizontalHeader()->setStretchLastSection(true);
    m_statsTable->verticalHeader()->setVisible(false);
    m_statsTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_statsTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_statsTable->setMinimumHeight(170);

    auto *hint = new QLabel(this);
    hint->setWordWrap(true);
    hint->setText(QStringLiteral(
        "对当前图片(或选区)的亮度二值化后,用 cv::findContours 查找轮廓,"
        "再用 cv::drawContours 以绿色绘制;支持撤销。"));

    auto *applyButton = new QPushButton(QStringLiteral("应用"), this);
    applyButton->setObjectName(QStringLiteral("contourApplyButton"));
    connect(applyButton, &QPushButton::clicked,
            this, &ContourDialog::applyRequested);

    // Fit controls: each method has its own button, with its full
    // parameter set exposed
    m_lineDistCombo = new QComboBox(this);
    m_lineDistCombo->setObjectName(QStringLiteral("contourLineDistCombo"));
    m_lineDistCombo->addItems(
        {QStringLiteral("L2(欧氏)"), QStringLiteral("L1"),
         QStringLiteral("L12"), QStringLiteral("FAIR"),
         QStringLiteral("WELSCH"), QStringLiteral("HUBER")});

    m_lineParamSpin =
        makeDoubleSpin(this, "contourLineParamSpin", 0.0, 100.0, 0.0, 2);
    m_lineRepsSpin =
        makeDoubleSpin(this, "contourLineRepsSpin", 0.001, 10.0, 0.01, 3);
    m_lineAepsSpin =
        makeDoubleSpin(this, "contourLineAepsSpin", 0.001, 10.0, 0.01, 3);
    m_polyEpsilonSpin =
        makeDoubleSpin(this, "contourPolyEpsilonSpin", 0.0, 200.0, 2.0, 1);
    m_polyClosedCheck = new QCheckBox(QStringLiteral("闭合"), this);
    m_polyClosedCheck->setObjectName(QStringLiteral("contourPolyClosedCheck"));
    m_polyClosedCheck->setChecked(true);
    m_fitThicknessSpin = new QSpinBox(this);
    m_fitThicknessSpin->setObjectName(
        QStringLiteral("contourFitThicknessSpin"));
    m_fitThicknessSpin->setRange(1, 20);
    m_fitThicknessSpin->setValue(2);

    auto *lineAccuracyRow = new QWidget(this);
    auto *lineAccuracyLayout = new QHBoxLayout(lineAccuracyRow);
    lineAccuracyLayout->setContentsMargins(0, 0, 0, 0);
    lineAccuracyLayout->addWidget(
        new QLabel(QStringLiteral("C"), lineAccuracyRow));
    lineAccuracyLayout->addWidget(m_lineParamSpin);
    lineAccuracyLayout->addWidget(
        new QLabel(QStringLiteral("reps"), lineAccuracyRow));
    lineAccuracyLayout->addWidget(m_lineRepsSpin);
    lineAccuracyLayout->addWidget(
        new QLabel(QStringLiteral("aeps"), lineAccuracyRow));
    lineAccuracyLayout->addWidget(m_lineAepsSpin);

    auto *polyRow = new QWidget(this);
    auto *polyLayout = new QHBoxLayout(polyRow);
    polyLayout->setContentsMargins(0, 0, 0, 0);
    polyLayout->addWidget(m_polyEpsilonSpin);
    polyLayout->addWidget(m_polyClosedCheck);

    auto *ellipseButton = new QPushButton(QStringLiteral("椭圆拟合"), this);
    ellipseButton->setObjectName(QStringLiteral("contourFitEllipseButton"));
    connect(ellipseButton, &QPushButton::clicked, this,
            [this] { emit fitRequested(ContourFitType::Ellipse); });
    auto *lineButton = new QPushButton(QStringLiteral("直线拟合"), this);
    lineButton->setObjectName(QStringLiteral("contourFitLineButton"));
    connect(lineButton, &QPushButton::clicked, this,
            [this] { emit fitRequested(ContourFitType::Line); });
    auto *polyButton = new QPushButton(QStringLiteral("多边形逼近"), this);
    polyButton->setObjectName(QStringLiteral("contourFitPolyButton"));
    connect(polyButton, &QPushButton::clicked, this,
            [this] { emit fitRequested(ContourFitType::Polygon); });

    auto *fitButtons = new QHBoxLayout;
    fitButtons->addWidget(ellipseButton);
    fitButtons->addWidget(lineButton);
    fitButtons->addWidget(polyButton);

    auto *fitGroup = new QGroupBox(QStringLiteral("轮廓拟合"), this);
    auto *fitForm = new QFormLayout(fitGroup);
    fitForm->addRow(QStringLiteral("直线距离:"), m_lineDistCombo);
    fitForm->addRow(QStringLiteral("直线精度:"), lineAccuracyRow);
    fitForm->addRow(QStringLiteral("逼近精度:"), polyRow);
    fitForm->addRow(QStringLiteral("拟合线宽:"), m_fitThicknessSpin);
    fitForm->addRow(fitButtons);

    m_fitTable = new QTableWidget(0, 2, this);
    m_fitTable->setObjectName(QStringLiteral("contourFitTable"));
    m_fitTable->setHorizontalHeaderLabels(
        {QStringLiteral("编号"), QStringLiteral("拟合结果")});
    m_fitTable->horizontalHeader()->setStretchLastSection(true);
    m_fitTable->verticalHeader()->setVisible(false);
    m_fitTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_fitTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_fitTable->setMinimumHeight(150);

    auto *tabs = new QTabWidget(this);
    tabs->setObjectName(QStringLiteral("contourResultTabs"));
    tabs->addTab(m_statsTable, QStringLiteral("轮廓"));
    tabs->addTab(m_fitTable, QStringLiteral("拟合"));

    auto *form = new QFormLayout;
    form->addRow(QStringLiteral("检索模式:"), m_modeCombo);
    form->addRow(QStringLiteral("近似方法:"), m_methodCombo);
    form->addRow(QStringLiteral("偏移:"), offsetRow);
    form->addRow(QStringLiteral("二值化:"), binaryRow);
    form->addRow(QStringLiteral("线宽:"), m_thicknessSpin);
    form->addRow(QStringLiteral("线型:"), m_lineTypeCombo);
    form->addRow(QStringLiteral("点测试:"), queryRow);
    form->addRow(QString(), m_largestCheck);

    auto *layout = new QVBoxLayout(this);
    layout->addLayout(form);
    layout->addWidget(fitGroup);
    layout->addWidget(m_countLabel);
    layout->addWidget(tabs, 1);
    layout->addWidget(hint);
    layout->addWidget(applyButton);
    resize(940, 740);

    connect(m_otsuCheck, &QCheckBox::toggled, this, [this] {
        updateThresholdEnabled();
    });
    updateThresholdEnabled();
}

void ContourDialog::updateThresholdEnabled()
{
    m_thresholdSpin->setEnabled(!m_otsuCheck->isChecked());
}

ContourParams ContourDialog::contourParams() const
{
    ContourParams params;
    params.otsu = m_otsuCheck->isChecked();
    params.threshold = m_thresholdSpin->value();
    params.invert = m_invertCheck->isChecked();
    params.retrieval = currentValue(m_modeCombo, kRetrievalModes, 4);
    params.approximation =
        currentValue(m_methodCombo, kApproximationMethods, 4);
    params.offset = cv::Point(m_offsetXSpin->value(), m_offsetYSpin->value());
    params.thickness = m_thicknessSpin->value();
    params.lineType = currentValue(m_lineTypeCombo, kLineTypes, 3);
    params.largestOnly = m_largestCheck->isChecked();
    params.queryPoint = cv::Point2f(float(m_queryXSpin->value()),
                                    float(m_queryYSpin->value()));
    params.measureDistance = m_measureDistCheck->isChecked();
    return params;
}

void ContourDialog::setQueryPoint(const cv::Point2f &point)
{
    m_queryXSpin->setValue(int(std::lround(point.x)));
    m_queryYSpin->setValue(int(std::lround(point.y)));
}

void ContourDialog::setContourInfos(const std::vector<ContourInfo> &infos)
{
    m_countLabel->setText(
        QStringLiteral("检测到 %1 个轮廓").arg(infos.size()));
    m_statsTable->setRowCount(int(infos.size()));
    for (int row = 0; row < int(infos.size()); ++row) {
        const ContourInfo &info = infos[size_t(row)];
        m_statsTable->setItem(row, 0,
                              new QTableWidgetItem(QString::number(info.index)));
        m_statsTable->setItem(row, 1, new QTableWidgetItem(
                                          QString::number(info.area, 'f', 1)));
        m_statsTable->setItem(row, 2, new QTableWidgetItem(
                                          QString::number(info.perimeter, 'f', 1)));
        m_statsTable->setItem(
            row, 3,
            new QTableWidgetItem(
                QStringLiteral("%1, %2, %3, %4")
                    .arg(info.boundingBox.x)
                    .arg(info.boundingBox.y)
                    .arg(info.boundingBox.width)
                    .arg(info.boundingBox.height)));
        m_statsTable->setItem(
            row, 4,
            new QTableWidgetItem(QString::number(info.aspectRatio, 'f', 2)));
        m_statsTable->setItem(
            row, 5,
            new QTableWidgetItem(QString::number(info.extent, 'f', 3)));
        m_statsTable->setItem(
            row, 6,
            new QTableWidgetItem(QString::number(info.solidity, 'f', 3)));
        m_statsTable->setItem(
            row, 7,
            new QTableWidgetItem(QString::number(info.meanValue, 'f', 1)));
        m_statsTable->setItem(
            row, 8,
            new QTableWidgetItem(
                QStringLiteral("(%1, %2) r=%3")
                    .arg(info.enclosingCircleCenter.x, 0, 'f', 1)
                    .arg(info.enclosingCircleCenter.y, 0, 'f', 1)
                    .arg(info.enclosingCircleRadius, 0, 'f', 1)));

        QString pointText;
        if (info.pointTest > 0.0)
            pointText = QStringLiteral("内部");
        else if (info.pointTest < 0.0)
            pointText = QStringLiteral("外部");
        else
            pointText = QStringLiteral("边界");
        if (m_measureDistCheck->isChecked())
            pointText += QStringLiteral(" %1").arg(info.pointTest, 0, 'f', 1);
        m_statsTable->setItem(row, 9, new QTableWidgetItem(pointText));
    }
}

ContourFitParams ContourDialog::contourFitParams(ContourFitType type) const
{
    ContourFitParams params;
    params.type = type;
    params.distType = currentValue(m_lineDistCombo, kDistanceTypes, 6);
    params.param = m_lineParamSpin->value();
    params.reps = m_lineRepsSpin->value();
    params.aeps = m_lineAepsSpin->value();
    params.epsilon = m_polyEpsilonSpin->value();
    params.closed = m_polyClosedCheck->isChecked();
    params.thickness = m_fitThicknessSpin->value();
    return params;
}

void ContourDialog::setContourFits(const std::vector<ContourFitInfo> &fits,
                                   ContourFitType type)
{
    m_fitTable->setRowCount(int(fits.size()));
    for (int row = 0; row < int(fits.size()); ++row) {
        const ContourFitInfo &fit = fits[size_t(row)];
        m_fitTable->setItem(
            row, 0, new QTableWidgetItem(QString::number(fit.index)));

        QString text;
        if (!fit.valid) {
            text = QStringLiteral("拟合失败(点数不足)");
        } else {
            switch (type) {
            case ContourFitType::Ellipse:
                text = QStringLiteral("中心=(%1, %2) 轴=(%3, %4) 角度=%5°")
                           .arg(fit.ellipse.center.x, 0, 'f', 1)
                           .arg(fit.ellipse.center.y, 0, 'f', 1)
                           .arg(fit.ellipse.size.width, 0, 'f', 1)
                           .arg(fit.ellipse.size.height, 0, 'f', 1)
                           .arg(fit.ellipse.angle, 0, 'f', 1);
                break;
            case ContourFitType::Line: {
                const double angle =
                    std::atan2(fit.lineDirection.y, fit.lineDirection.x)
                    * 180.0 / CV_PI;
                text = QStringLiteral("点=(%1, %2) 方向=(%3, %4) 角度=%5°")
                           .arg(fit.linePoint.x, 0, 'f', 1)
                           .arg(fit.linePoint.y, 0, 'f', 1)
                           .arg(fit.lineDirection.x, 0, 'f', 3)
                           .arg(fit.lineDirection.y, 0, 'f', 3)
                           .arg(angle, 0, 'f', 1);
                break;
            }
            case ContourFitType::Polygon:
                text = QStringLiteral("顶点数=%1 面积=%2")
                           .arg(fit.vertexCount)
                           .arg(fit.polygonArea, 0, 'f', 1);
                break;
            }
        }
        m_fitTable->setItem(row, 1, new QTableWidgetItem(text));
    }
}

} // namespace chameleon
