#include "ui/histogram_match_dialog.h"

#include <QComboBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

#include "processing/image_io.h"

namespace chameleon {

HistogramMatchDialog::HistogramMatchDialog(const QStringList &templatePaths,
                                           QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(QStringLiteral("直方图匹配"));
    setObjectName(QStringLiteral("histogramMatchDialog"));
    setAttribute(Qt::WA_DeleteOnClose);

    m_targetCombo = new QComboBox(this);
    m_targetCombo->setObjectName(QStringLiteral("matchTargetCombo"));
    for (const QString &path : templatePaths)
        addTargetPath(path);
    m_targetCombo->setCurrentIndex(m_targetCombo->count() > 0 ? 0 : -1);

    auto *browseButton = new QPushButton(QStringLiteral("浏览..."), this);
    browseButton->setObjectName(QStringLiteral("matchBrowseButton"));
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

    m_infoLabel = new QLabel(this);
    m_infoLabel->setObjectName(QStringLiteral("matchInfoLabel"));

    auto *applyButton = new QPushButton(QStringLiteral("应用"), this);
    applyButton->setObjectName(QStringLiteral("matchApplyButton"));
    connect(applyButton, &QPushButton::clicked,
            this, &HistogramMatchDialog::applyRequested);

    auto *form = new QFormLayout;
    form->addRow(QStringLiteral("模板:"), targetRow);

    auto *layout = new QVBoxLayout(this);
    layout->addLayout(form);
    layout->addWidget(m_infoLabel);
    layout->addWidget(new QLabel(
        QStringLiteral("将当前图片(或选中区域)的直方图匹配到模板;可撤销。"), this));
    layout->addWidget(applyButton);
    layout->addStretch();
    resize(420, 190);

    connect(m_targetCombo, &QComboBox::currentIndexChanged,
            this, [this] { updateTemplateInfo(); });
    updateTemplateInfo();
}

void HistogramMatchDialog::addTargetPath(const QString &path)
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

cv::Mat HistogramMatchDialog::currentTemplateImage()
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

QString HistogramMatchDialog::currentTemplateName() const
{
    return m_targetCombo->currentText();
}

void HistogramMatchDialog::updateTemplateInfo()
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
