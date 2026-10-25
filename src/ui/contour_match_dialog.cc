#include "ui/contour_match_dialog.h"

#include <cmath>

#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>

#include "processing/image_io.h"

namespace chameleon {

namespace {

// Hu moments span many orders of magnitude: show the signed log10
QString formatHu(double value)
{
    if (!std::isfinite(value) || value == 0.0)
        return QStringLiteral("0.00");
    const double logValue = std::log10(std::abs(value));
    return QString::number(std::copysign(logValue, value), 'f', 2);
}

} // namespace

ContourMatchDialog::ContourMatchDialog(const QStringList &templatePaths,
                                       QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(QStringLiteral("轮廓匹配"));
    setObjectName(QStringLiteral("contourMatchDialog"));
    setAttribute(Qt::WA_DeleteOnClose);

    m_targetCombo = new QComboBox(this);
    m_targetCombo->setObjectName(QStringLiteral("contourMatchTargetCombo"));
    for (const QString &path : templatePaths)
        addTargetPath(path);
    m_targetCombo->setCurrentIndex(m_targetCombo->count() > 0 ? 0 : -1);

    auto *browseButton = new QPushButton(QStringLiteral("浏览..."), this);
    browseButton->setObjectName(QStringLiteral("contourMatchBrowseButton"));
    connect(browseButton, &QPushButton::clicked, this, [this] {
        const QString path = QFileDialog::getOpenFileName(
            this, QStringLiteral("选择模板图片"), QString(),
            QStringLiteral("图片文件 (*.png *.jpg *.jpeg *.bmp *.webp *.tif *.tiff);;"
                           "所有文件 (*.*)"));
        if (!path.isEmpty())
            addTargetPath(path);
    });

    auto *targetRow = new QWidget(this);
    auto *targetRowLayout = new QHBoxLayout(targetRow);
    targetRowLayout->setContentsMargins(0, 0, 0, 0);
    targetRowLayout->addWidget(m_targetCombo, 1);
    targetRowLayout->addWidget(browseButton);

    m_methodCombo = new QComboBox(this);
    m_methodCombo->setObjectName(QStringLiteral("contourMatchMethodCombo"));
    m_methodCombo->addItem(QStringLiteral("I1"));
    m_methodCombo->addItem(QStringLiteral("I2"));
    m_methodCombo->addItem(QStringLiteral("I3"));

    m_parameterSpin = new QDoubleSpinBox(this);
    m_parameterSpin->setObjectName(
        QStringLiteral("contourMatchParameterSpin"));
    m_parameterSpin->setRange(-100.0, 100.0);
    m_parameterSpin->setDecimals(2);
    m_parameterSpin->setValue(0.0);

    m_infoLabel = new QLabel(this);
    m_infoLabel->setObjectName(QStringLiteral("contourMatchInfoLabel"));

    m_bestLabel = new QLabel(QStringLiteral("最佳匹配:--"), this);
    m_bestLabel->setObjectName(QStringLiteral("contourMatchBestLabel"));

    m_table = new QTableWidget(0, 10, this);
    m_table->setObjectName(QStringLiteral("contourMatchTable"));
    m_table->setHorizontalHeaderLabels(
        {QStringLiteral("编号"), QStringLiteral("匹配值"),
         QStringLiteral("m00"), QStringLiteral("Hu1"), QStringLiteral("Hu2"),
         QStringLiteral("Hu3"), QStringLiteral("Hu4"), QStringLiteral("Hu5"),
         QStringLiteral("Hu6"), QStringLiteral("Hu7")});
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->verticalHeader()->setVisible(false);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setMinimumHeight(180);

    auto *hint = new QLabel(this);
    hint->setWordWrap(true);
    hint->setText(QStringLiteral(
        "对当前图片(或选区)与模板图片做 Otsu 二值化后提取轮廓;"
        "以模板最大轮廓为基准,用 cv::moments、cv::HuMoments 与 "
        "cv::matchShapes 计算相似度(数值越小越相似)。Hu 值以 log10 显示。"));

    auto *applyButton = new QPushButton(QStringLiteral("应用"), this);
    applyButton->setObjectName(QStringLiteral("contourMatchApplyButton"));
    connect(applyButton, &QPushButton::clicked,
            this, &ContourMatchDialog::applyRequested);

    auto *form = new QFormLayout;
    form->addRow(QStringLiteral("模板:"), targetRow);
    form->addRow(QStringLiteral("匹配方法:"), m_methodCombo);
    form->addRow(QStringLiteral("参数:"), m_parameterSpin);

    auto *layout = new QVBoxLayout(this);
    layout->addLayout(form);
    layout->addWidget(m_infoLabel);
    layout->addWidget(m_bestLabel);
    layout->addWidget(m_table, 1);
    layout->addWidget(hint);
    layout->addWidget(applyButton);
    resize(680, 540);

    connect(m_targetCombo, &QComboBox::currentIndexChanged, this,
            [this] { updateTemplateInfo(); });
    updateTemplateInfo();
}

void ContourMatchDialog::addTargetPath(const QString &path)
{
    for (int i = 0; i < m_targetCombo->count(); ++i) {
        if (m_targetCombo->itemData(i, Qt::UserRole).toString() == path) {
            m_targetCombo->setCurrentIndex(i);
            return;
        }
    }
    m_targetCombo->addItem(QFileInfo(path).fileName());
    const int index = m_targetCombo->count() - 1;
    m_targetCombo->setItemData(index, path, Qt::UserRole);
    m_targetCombo->setItemData(index, path, Qt::ToolTipRole);
    m_targetCombo->setCurrentIndex(index);
}

cv::Mat ContourMatchDialog::currentTemplateImage()
{
    const int index = m_targetCombo->currentIndex();
    if (index < 0)
        return {};
    const QString path = m_targetCombo->itemData(index, Qt::UserRole).toString();
    if (path != m_loadedPath) {
        m_loadedTemplate = ImageIO::imreadUtf8(path.toStdString());
        m_loadedPath = path;
    }
    return m_loadedTemplate;
}

QString ContourMatchDialog::currentTemplateName() const
{
    return m_targetCombo->currentText();
}

ContourMatchParams ContourMatchDialog::contourMatchParams() const
{
    ContourMatchParams params;
    switch (m_methodCombo->currentIndex()) {
    case 1:
        params.method = cv::CONTOURS_MATCH_I2;
        break;
    case 2:
        params.method = cv::CONTOURS_MATCH_I3;
        break;
    default:
        params.method = cv::CONTOURS_MATCH_I1;
        break;
    }
    params.parameter = m_parameterSpin->value();
    return params;
}

void ContourMatchDialog::setResult(const ContourMatchResult &result)
{
    m_table->setRowCount(int(result.matches.size()));
    for (int row = 0; row < int(result.matches.size()); ++row) {
        const ContourMatchInfo &match = result.matches[size_t(row)];
        m_table->setItem(
            row, 0, new QTableWidgetItem(QString::number(match.index)));
        m_table->setItem(row, 1,
                         new QTableWidgetItem(QString::number(match.score, 'f', 4)));
        m_table->setItem(
            row, 2,
            new QTableWidgetItem(QString::number(match.moments.m00, 'f', 1)));
        for (int k = 0; k < 7; ++k) {
            m_table->setItem(
                row, 3 + k,
                new QTableWidgetItem(formatHu(match.hu[size_t(k)])));
        }
    }

    if (result.bestIndex > 0) {
        double bestScore = 0.0;
        for (const ContourMatchInfo &match : result.matches) {
            if (match.index == result.bestIndex)
                bestScore = match.score;
        }
        m_bestLabel->setText(QStringLiteral("最佳匹配:#%1(匹配值 %2)")
                                 .arg(result.bestIndex)
                                 .arg(bestScore, 0, 'f', 4));
    } else {
        m_bestLabel->setText(QStringLiteral("最佳匹配:--"));
    }
}

void ContourMatchDialog::updateTemplateInfo()
{
    const cv::Mat templateImage = currentTemplateImage();
    if (templateImage.empty())
        m_infoLabel->setText(QStringLiteral("无法加载模板图片"));
    else
        m_infoLabel->setText(QStringLiteral("模板尺寸:%1 × %2")
                                 .arg(templateImage.cols)
                                 .arg(templateImage.rows));
}

} // namespace chameleon
