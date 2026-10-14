#include "ui/histogram_compare_dialog.h"

#include <algorithm>

#include <QComboBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

#include "processing/histogram.h"
#include "processing/image_io.h"

namespace chameleon {

namespace {

const char *channelName(int index, int count, bool bothGray)  // NOLINT(bugprone-easily-swappable-parameters) - fixed argument order, one call site
{
    static const char *names[] = {"蓝色通道", "绿色通道", "红色通道", "Alpha 通道"};
    if (count == 1)
        return bothGray ? "灰度" : "通道 0";
    return names[std::min(index, 3)];
}

} // namespace

HistogramCompareDialog::HistogramCompareDialog(const cv::Mat &reference,
                                               const QString &referenceLabel,
                                               const QStringList &targetPaths,
                                               QWidget *parent)
    : QDialog(parent)
    , m_reference(reference)
{
    setWindowTitle(QStringLiteral("直方图比较"));
    setObjectName(QStringLiteral("histogramCompareDialog"));
    setAttribute(Qt::WA_DeleteOnClose);

    m_methodCombo = new QComboBox(this);
    m_methodCombo->setObjectName(QStringLiteral("compareMethodCombo"));
    m_methodCombo->addItem(QStringLiteral("相关性"),
                           int(HistogramCompareMethod::Correlation));
    m_methodCombo->addItem(QStringLiteral("卡方"),
                           int(HistogramCompareMethod::ChiSquare));
    m_methodCombo->addItem(QStringLiteral("直方图交集"),
                           int(HistogramCompareMethod::Intersect));
    m_methodCombo->addItem(QStringLiteral("巴氏距离"),
                           int(HistogramCompareMethod::Bhattacharyya));
    m_methodCombo->addItem(QStringLiteral("KL 散度"),
                           int(HistogramCompareMethod::KlDivergence));

    m_targetCombo = new QComboBox(this);
    m_targetCombo->setObjectName(QStringLiteral("compareTargetCombo"));
    for (const QString &path : targetPaths)
        addTargetPath(path);
    m_targetCombo->setCurrentIndex(m_targetCombo->count() > 0 ? 0 : -1);

    m_browseButton = new QPushButton(QStringLiteral("浏览..."), this);
    m_browseButton->setObjectName(QStringLiteral("compareBrowseButton"));
    connect(m_browseButton, &QPushButton::clicked, this, [this] {
        const QString path = QFileDialog::getOpenFileName(
            this, QStringLiteral("选择要比较的图片"), QString(),
            QStringLiteral("图片文件 (*.png *.jpg *.jpeg *.bmp *.webp *.tif *.tiff);;"
                           "所有文件 (*.*)"));
        if (!path.isEmpty())
            addTargetPath(path);
    });

    auto *targetRow = new QWidget(this);
    auto *targetRowLayout = new QHBoxLayout(targetRow);
    targetRowLayout->setContentsMargins(0, 0, 0, 0);
    targetRowLayout->addWidget(m_targetCombo, 1);
    targetRowLayout->addWidget(m_browseButton);

    m_resultsLabel = new QLabel(this);
    m_resultsLabel->setObjectName(QStringLiteral("compareResultsLabel"));
    m_resultsLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_resultsLabel->setMinimumHeight(110);
    m_resultsLabel->setAlignment(Qt::AlignTop | Qt::AlignLeft);

    auto *form = new QFormLayout;
    form->addRow(QStringLiteral("方法:"), m_methodCombo);
    form->addRow(QStringLiteral("比较目标:"), targetRow);

    auto *layout = new QVBoxLayout(this);
    layout->addLayout(form);
    layout->addWidget(new QLabel(QStringLiteral("基准:%1").arg(referenceLabel), this));
    layout->addWidget(m_resultsLabel);
    resize(420, 250);

    connect(m_methodCombo, &QComboBox::currentIndexChanged,
            this, [this] { updateComparison(); });
    connect(m_targetCombo, &QComboBox::currentIndexChanged,
            this, [this] { updateComparison(); });
    updateComparison();
}

void HistogramCompareDialog::addTargetPath(const QString &path)
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

void HistogramCompareDialog::updateComparison()
{
    const int index = m_targetCombo->currentIndex();
    if (index < 0) {
        m_resultsLabel->setText(QStringLiteral("没有可比较的图片"));
        return;
    }
    const QString path = m_targetCombo->itemData(index, Qt::UserRole).toString();

    // Load on demand and cache; invalidate when the target changes
    if (path != m_loadedPath) {
        m_loadedTarget = ImageIO::imreadUtf8(path.toStdString());
        m_loadedPath = path;
    }
    if (m_reference.empty() || m_loadedTarget.empty()) {
        m_resultsLabel->setText(QStringLiteral("无法加载目标图片"));
        return;
    }

    const auto method = HistogramCompareMethod(m_methodCombo->currentData().toInt());
    const std::vector<double> scores =
        compareHistograms(m_reference, m_loadedTarget, method);

    const bool bothGray = m_reference.channels() == 1 && m_loadedTarget.channels() == 1;
    QString text = QStringLiteral("相似度(越接近 1 越相似)\
");
    double sum = 0.0;
    for (size_t i = 0; i < scores.size(); ++i) {
        const double similarity = normalizedSimilarity(method, scores[i]);
        text += QStringLiteral("%1: %2\
")
                    .arg(QString::fromUtf8(channelName(int(i), int(scores.size()), bothGray)),
                         QString::number(similarity, 'f', 4));
        sum += similarity;
    }
    if (!scores.empty()) {
        text += QStringLiteral("平均: %1\
")
                    .arg(QString::number(sum / double(scores.size()), 'f', 4));
    }
    m_resultsLabel->setText(text);
}

} // namespace chameleon
