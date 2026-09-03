#include "workstation/cad/AttachmentDialogs.h"
#include "workstation/cad/AttachmentManager.h"
#include <QFileInfo>
#include <QSplitter>

namespace workstation {
namespace cad {

AttachmentDialog::AttachmentDialog(AttachmentManager* manager, QWidget* parent)
    : QDialog(parent), m_manager(manager) {
    setWindowTitle("File Attachments");
    setMinimumSize(600, 450);
    setStyleSheet(
        "QDialog { background-color: #2b2b2b; color: #d4d4d4; }"
        "QListWidget { background-color: #1e1e1e; color: #d4d4d4; border: 1px solid #555; }"
        "QPushButton { background-color: #3c3c3c; color: #d4d4d4; border: 1px solid #555; padding: 5px 12px; }"
        "QPushButton:hover { background-color: #4c4c4c; }"
        "QPushButton:disabled { color: #666; }"
        "QLabel { color: #d4d4d4; }"
        "QProgressBar { border: 1px solid #555; background-color: #1e1e1e; text-align: center; color: #d4d4d4; }"
        "QProgressBar::chunk { background-color: #0078d4; }"
    );

    auto* mainLayout = new QVBoxLayout(this);
    auto* headerLabel = new QLabel("Manage DXF / DWG / SNT file attachments");
    headerLabel->setStyleSheet("font-weight: bold; font-size: 13px; padding: 5px;");
    mainLayout->addWidget(headerLabel);

    auto* splitter = new QSplitter(Qt::Horizontal);

    auto* leftWidget = new QWidget();
    auto* leftLayout = new QVBoxLayout(leftWidget);
    leftLayout->setContentsMargins(0, 0, 0, 0);

    m_fileList = new QListWidget();
    m_fileList->setSelectionMode(QAbstractItemView::SingleSelection);
    connect(m_fileList, &QListWidget::currentRowChanged, this, &AttachmentDialog::onItemSelectionChanged);
    leftLayout->addWidget(m_fileList);

    auto* btnRow = new QHBoxLayout();
    m_addDxfBtn = new QPushButton("Add DXF");
    m_addDwgBtn = new QPushButton("Add DWG");
    m_addSntBtn = new QPushButton("Add SNT");
    connect(m_addDxfBtn, &QPushButton::clicked, this, &AttachmentDialog::onAddDxf);
    connect(m_addDwgBtn, &QPushButton::clicked, this, &AttachmentDialog::onAddDwg);
    connect(m_addSntBtn, &QPushButton::clicked, this, &AttachmentDialog::onAddSnt);
    btnRow->addWidget(m_addDxfBtn);
    btnRow->addWidget(m_addDwgBtn);
    btnRow->addWidget(m_addSntBtn);
    leftLayout->addLayout(btnRow);

    auto* btnRow2 = new QHBoxLayout();
    m_removeBtn = new QPushButton("Remove");
    m_removeAllBtn = new QPushButton("Remove All");
    connect(m_removeBtn, &QPushButton::clicked, this, &AttachmentDialog::onRemoveSelected);
    connect(m_removeAllBtn, &QPushButton::clicked, this, &AttachmentDialog::onRemoveAll);
    btnRow2->addWidget(m_removeBtn);
    btnRow2->addWidget(m_removeAllBtn);
    leftLayout->addLayout(btnRow2);

    splitter->addWidget(leftWidget);

    auto* rightWidget = new QWidget();
    auto* rightLayout = new QVBoxLayout(rightWidget);
    rightLayout->setContentsMargins(0, 0, 0, 0);
    rightLayout->addWidget(new QLabel("Layers:"));
    m_layerList = new QListWidget();
    m_layerList->setSelectionMode(QAbstractItemView::NoSelection);
    connect(m_layerList, &QListWidget::itemChanged, this, &AttachmentDialog::onLayerItemChanged);
    rightLayout->addWidget(m_layerList);

    splitter->addWidget(rightWidget);
    splitter->setSizes({300, 300});
    mainLayout->addWidget(splitter);

    m_progressBar = new QProgressBar();
    m_progressBar->setVisible(false);
    mainLayout->addWidget(m_progressBar);

    auto* closeBtn = new QPushButton("Close");
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::accept);
    mainLayout->addWidget(closeBtn, 0, Qt::AlignRight);

    rebuildFileList();
    updateButtonStates();
}

AttachmentDialog::~AttachmentDialog() = default;

void AttachmentDialog::onAddDxf() {
    QString filepath = QFileDialog::getOpenFileName(this, "Select DXF File", QString(), "DXF Files (*.dxf);;All Files (*)");
    if (filepath.isEmpty()) return;
    m_progressBar->setVisible(true);
    m_progressBar->setRange(0, 0);
    std::string error;
    std::string path = filepath.toStdString();
    if (m_manager->addDxf(path, &error)) {
        rebuildFileList();
    } else {
        QMessageBox::warning(this, "Load Failed", QString::fromStdString(error));
    }
    m_progressBar->setVisible(false);
}

void AttachmentDialog::onAddDwg() {
    QString filepath = QFileDialog::getOpenFileName(this, "Select DWG File", QString(), "DWG Files (*.dwg);;All Files (*)");
    if (filepath.isEmpty()) return;
    m_progressBar->setVisible(true);
    m_progressBar->setRange(0, 0);
    std::string error;
    std::string path = filepath.toStdString();
    if (m_manager->addDwg(path, &error)) {
        rebuildFileList();
    } else {
        QMessageBox::warning(this, "Load Failed", QString::fromStdString(error));
    }
    m_progressBar->setVisible(false);
}

void AttachmentDialog::onAddSnt() {
    QString filepath = QFileDialog::getOpenFileName(this, "Select SNT File", QString(), "SNT Files (*.snt);;All Files (*)");
    if (filepath.isEmpty()) return;
    m_progressBar->setVisible(true);
    m_progressBar->setRange(0, 0);
    std::string error;
    std::string path = filepath.toStdString();
    if (m_manager->addSnt(path, &error)) {
        rebuildFileList();
    } else {
        QMessageBox::warning(this, "Load Failed", QString::fromStdString(error));
    }
    m_progressBar->setVisible(false);
}

void AttachmentDialog::onRemoveSelected() {
    int row = m_fileList->currentRow();
    if (row < 0) return;
    m_manager->removeAttachment(row);
    rebuildFileList();
    rebuildLayerList();
}

void AttachmentDialog::onRemoveAll() {
    if (QMessageBox::question(this, "Confirm", "Remove all attachments?") == QMessageBox::Yes) {
        m_manager->removeAll();
        rebuildFileList();
        rebuildLayerList();
    }
}

void AttachmentDialog::onItemSelectionChanged() { rebuildLayerList(); }

void AttachmentDialog::onLayerItemChanged(QListWidgetItem* item) {
    if (!item) return;
    std::string layerName = item->text().toStdString();
    bool visible = item->checkState() == Qt::Checked;
    int row = m_fileList->currentRow();
    if (row < 0) return;
    auto atts = m_manager->attachments();
    if (row >= static_cast<int>(atts.size())) return;
    const auto& att = atts[row];
    switch (att.type) {
        case AttachmentInfo::Type::DXF: {
            auto* dxf = m_manager->dxfAttachment(att.index);
            if (dxf) dxf->layerManager()->setLayerVisible(layerName, visible);
            break;
        }
        case AttachmentInfo::Type::DWG: {
            auto* dwg = m_manager->dwgAttachment(att.index);
            if (dwg) dwg->layerManager()->setLayerVisible(layerName, visible);
            break;
        }
        case AttachmentInfo::Type::SNT: {
            auto* snt = m_manager->sntAttachment(att.index);
            if (snt) snt->layerManager()->setLayerVisible(layerName, visible);
            break;
        }
    }
}

void AttachmentDialog::rebuildFileList() {
    m_fileList->clear();
    auto atts = m_manager->attachments();
    for (const auto& att : atts) {
        QString typeName;
        switch (att.type) {
            case AttachmentInfo::Type::DXF: typeName = "[DXF]"; break;
            case AttachmentInfo::Type::DWG: typeName = "[DWG]"; break;
            case AttachmentInfo::Type::SNT: typeName = "[SNT]"; break;
        }
        m_fileList->addItem(QString("%1 %2").arg(typeName, QString::fromStdString(att.filepath)));
    }
    updateButtonStates();
}

void AttachmentDialog::rebuildLayerList() {
    m_layerList->clear();
    int row = m_fileList->currentRow();
    if (row < 0) return;
    auto atts = m_manager->attachments();
    if (row >= static_cast<int>(atts.size())) return;
    const auto& att = atts[row];

    auto addLayers = [this](const std::vector<LayerInfo>& layers) {
        for (const auto& layer : layers) {
            auto* item = new QListWidgetItem(QString("%1 (%2)").arg(QString::fromStdString(layer.name)).arg(layer.entityCount));
            item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
            item->setCheckState(layer.visible ? Qt::Checked : Qt::Unchecked);
            m_layerList->addItem(item);
        }
    };

    switch (att.type) {
        case AttachmentInfo::Type::DXF: {
            auto* dxf = m_manager->dxfAttachment(att.index);
            if (dxf) addLayers(dxf->layerManager()->layers());
            break;
        }
        case AttachmentInfo::Type::DWG: {
            auto* dwg = m_manager->dwgAttachment(att.index);
            if (dwg) addLayers(dwg->layerManager()->layers());
            break;
        }
        case AttachmentInfo::Type::SNT: {
            auto* snt = m_manager->sntAttachment(att.index);
            if (snt) addLayers(snt->layerManager()->layers());
            break;
        }
    }
}

void AttachmentDialog::updateButtonStates() {
    bool hasItems = m_fileList->count() > 0;
    bool hasSelection = m_fileList->currentRow() >= 0;
    m_removeBtn->setEnabled(hasSelection);
    m_removeAllBtn->setEnabled(hasItems);
}

// DisplayOptionsDialog

DisplayOptionsDialog::DisplayOptionsDialog(AttachmentType type, QWidget* parent) : QDialog(parent) {
    QString title;
    switch (type) {
        case AttachmentType::DXF: title = "DXF Display Options"; break;
        case AttachmentType::DWG: title = "DWG Display Options"; break;
        case AttachmentType::SNT: title = "SNT Display Options"; break;
    }
    setWindowTitle(title);
    setStyleSheet(
        "QDialog { background-color: #2b2b2b; color: #d4d4d4; }"
        "QRadioButton, QCheckBox { color: #d4d4d4; }"
        "QComboBox { background-color: #3c3c3c; color: #d4d4d4; border: 1px solid #555; }"
        "QDoubleSpinBox { background-color: #3c3c3c; color: #d4d4d4; border: 1px solid #555; }"
        "QPushButton { background-color: #3c3c3c; color: #d4d4d4; border: 1px solid #555; padding: 5px 12px; }"
    );

    auto* layout = new QVBoxLayout(this);

    auto* modeGroup = new QGroupBox("Display Mode");
    auto* modeLayout = new QHBoxLayout(modeGroup);
    m_overlayRadio = new QRadioButton("Overlay (on top)");
    m_underlayRadio = new QRadioButton("Underlay (below)");
    m_overlayRadio->setChecked(true);
    modeLayout->addWidget(m_overlayRadio);
    modeLayout->addWidget(m_underlayRadio);
    layout->addWidget(modeGroup);

    auto* colorGroup = new QGroupBox("Color Override");
    auto* colorLayout = new QHBoxLayout(colorGroup);
    m_overrideCheck = new QCheckBox("Override color:");
    m_colorCombo = new QComboBox();
    m_colorCombo->addItem("Red", QColor(255, 0, 0));
    m_colorCombo->addItem("Green", QColor(0, 255, 0));
    m_colorCombo->addItem("Blue", QColor(0, 0, 255));
    m_colorCombo->addItem("Yellow", QColor(255, 255, 0));
    m_colorCombo->addItem("Cyan", QColor(0, 255, 255));
    m_colorCombo->addItem("Magenta", QColor(255, 0, 255));
    m_colorCombo->addItem("White", QColor(255, 255, 255));
    connect(m_overrideCheck, &QCheckBox::toggled, m_colorCombo, &QWidget::setEnabled);
    m_colorCombo->setEnabled(false);
    colorLayout->addWidget(m_overrideCheck);
    colorLayout->addWidget(m_colorCombo);
    layout->addWidget(colorGroup);

    auto* zRow = new QHBoxLayout();
    zRow->addWidget(new QLabel("Z Offset:"));
    m_zOffsetSpin = new QDoubleSpinBox();
    m_zOffsetSpin->setRange(-100.0, 100.0);
    m_zOffsetSpin->setSingleStep(0.1);
    m_zOffsetSpin->setValue(0.1);
    zRow->addWidget(m_zOffsetSpin);
    layout->addLayout(zRow);

    auto* btnRow = new QHBoxLayout();
    btnRow->addStretch();
    auto* okBtn = new QPushButton("OK");
    auto* cancelBtn = new QPushButton("Cancel");
    connect(okBtn, &QPushButton::clicked, this, &QDialog::accept);
    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
    btnRow->addWidget(okBtn);
    btnRow->addWidget(cancelBtn);
    layout->addLayout(btnRow);
}

QString DisplayOptionsDialog::displayMode() const { return m_underlayRadio->isChecked() ? "underlay" : "overlay"; }
bool DisplayOptionsDialog::isOverrideEnabled() const { return m_overrideCheck->isChecked(); }
QColor DisplayOptionsDialog::overrideColor() const { return m_colorCombo->currentData().value<QColor>(); }
double DisplayOptionsDialog::zOffset() const { return m_zOffsetSpin->value(); }

// ShadingDisplayDialog

ShadingDisplayDialog::ShadingDisplayDialog(QWidget* parent) : QDialog(parent) {
    setWindowTitle("Shading Display Settings");
    setMinimumSize(350, 350);
    setStyleSheet(
        "QDialog { background-color: #2b2b2b; color: #d4d4d4; }"
        "QDoubleSpinBox { background-color: #3c3c3c; color: #d4d4d4; border: 1px solid #555; }"
        "QComboBox { background-color: #3c3c3c; color: #d4d4d4; border: 1px solid #555; }"
        "QPushButton { background-color: #3c3c3c; color: #d4d4d4; border: 1px solid #555; padding: 5px 12px; }"
        "QLabel { color: #d4d4d4; }"
    );

    auto* layout = new QVBoxLayout(this);

    layout->addWidget(new QLabel("Light Azimuth:"));
    m_azimuthSpin = new QDoubleSpinBox();
    m_azimuthSpin->setRange(0, 360); m_azimuthSpin->setValue(45); m_azimuthSpin->setSuffix(" deg");
    layout->addWidget(m_azimuthSpin);

    layout->addWidget(new QLabel("Light Elevation:"));
    m_elevationSpin = new QDoubleSpinBox();
    m_elevationSpin->setRange(1, 89); m_elevationSpin->setValue(45); m_elevationSpin->setSuffix(" deg");
    layout->addWidget(m_elevationSpin);

    layout->addWidget(new QLabel("Ambient Light:"));
    m_ambientSpin = new QDoubleSpinBox();
    m_ambientSpin->setRange(0, 1); m_ambientSpin->setSingleStep(0.05); m_ambientSpin->setValue(0.25);
    layout->addWidget(m_ambientSpin);

    layout->addWidget(new QLabel("Max Edge Length:"));
    m_maxEdgeSpin = new QDoubleSpinBox();
    m_maxEdgeSpin->setRange(0.1, 1000); m_maxEdgeSpin->setValue(10); m_maxEdgeSpin->setSuffix(" m");
    layout->addWidget(m_maxEdgeSpin);

    layout->addWidget(new QLabel("Quality:"));
    m_qualityCombo = new QComboBox();
    m_qualityCombo->addItem("Fast", 0);
    m_qualityCombo->addItem("Normal", 1);
    m_qualityCombo->addItem("Slow (Best)", 2);
    m_qualityCombo->setCurrentIndex(1);
    layout->addWidget(m_qualityCombo);

    auto* presetRow = new QHBoxLayout();
    auto* microBtn = new QPushButton("MicroStation Preset");
    auto* arcgisBtn = new QPushButton("ArcGIS Preset");
    connect(microBtn, &QPushButton::clicked, this, &ShadingDisplayDialog::onPresetMicroStation);
    connect(arcgisBtn, &QPushButton::clicked, this, &ShadingDisplayDialog::onPresetArcGIS);
    presetRow->addWidget(microBtn);
    presetRow->addWidget(arcgisBtn);
    layout->addLayout(presetRow);

    auto* btnRow = new QHBoxLayout();
    btnRow->addStretch();
    auto* applyBtn = new QPushButton("Apply");
    auto* closeBtn = new QPushButton("Close");
    connect(applyBtn, &QPushButton::clicked, this, &ShadingDisplayDialog::shadingRequested);
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::accept);
    btnRow->addWidget(applyBtn);
    btnRow->addWidget(closeBtn);
    layout->addLayout(btnRow);
}

float ShadingDisplayDialog::azimuth() const { return m_azimuthSpin->value(); }
float ShadingDisplayDialog::elevation() const { return m_elevationSpin->value(); }
float ShadingDisplayDialog::ambient() const { return m_ambientSpin->value(); }
float ShadingDisplayDialog::maxEdgeLength() const { return m_maxEdgeSpin->value(); }
int ShadingDisplayDialog::qualityLevel() const { return m_qualityCombo->currentIndex(); }

void ShadingDisplayDialog::onPresetMicroStation() {
    m_azimuthSpin->setValue(315); m_elevationSpin->setValue(45); m_ambientSpin->setValue(0.2);
}

void ShadingDisplayDialog::onPresetArcGIS() {
    m_azimuthSpin->setValue(315); m_elevationSpin->setValue(45); m_ambientSpin->setValue(0.3);
}

} // namespace cad
} // namespace workstation
