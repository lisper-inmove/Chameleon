#include "ui/back_project_dialog.h"

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

BackProjectDialog::BackProjectDialog(const QStringList &templatePaths, QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(QStringLiteral("直方图反向投影"));
    setObjectName(QStringLiteral("backProjectDialog"));
    setAttribute(Qt::WA_DeleteOnClose);

    m_targetCombo = new QComboBox(this);
    m_targetCombo->setObjectName(QStringLiteral("backProjectTargetCombo"));
    for (const QString &path : templatePaths)
        addTargetPath(path);
    m_targetCombo->setCurrentIndex(m_targetCombo->count() > 0 ? 0 : -1);

    auto *browseButton = new QPushButton(QStringLiteral("浏览..."), this);
    browseButton->setObjectName(QStringLiteral("backProjectBrowseButton"));
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
    m_infoLabel->setObjectName(QStringLiteral("backProjectInfoLabel"));

    auto *computeButton = new QPushButton(QStringLiteral("计算并显示"), this);
    computeButton->setObjectName(QStringLiteral("backProjectButton"));
    connect(computeButton, &QPushButton::clicked,
            this, &BackProjectDialog::computeRequested);

    auto *form = new QFormLayout;
    form->addRow(QStringLiteral("模板:"), targetRow);

    auto *layout = new QVBoxLayout(this);
    layout->addLayout(form);
    layout->addWidget(m_infoLabel);
    layout->addWidget(new QLabel(
        QStringLiteral("用模板直方图对当前图片(或选中区域)做反向投影,结果在新窗口展示。"),
        this));
    layout->addWidget(computeButton);
    layout->addStretch();
    resize(440, 200);

    connect(m_targetCombo, &QComboBox::currentIndexChanged,
            this, [this] { updateTemplateInfo(); });
    updateTemplateInfo();
}

void BackProjectDialog::addTargetPath(const QString &path)
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

cv::Mat BackProjectDialog::currentTemplateImage()
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

QString BackProjectDialog::currentTemplateName() const
{
    return m_targetCombo->currentText();
}

void BackProjectDialog::updateTemplateInfo()
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
