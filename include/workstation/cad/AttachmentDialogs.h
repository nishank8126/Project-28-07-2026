#pragma once
#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QListWidget>
#include <QListWidgetItem>
#include <QComboBox>
#include <QCheckBox>
#include <QRadioButton>
#include <QProgressBar>
#include <QGroupBox>
#include <QDoubleSpinBox>
#include <QScrollArea>
#include <QFileDialog>
#include <QMessageBox>

namespace workstation {
namespace cad {

class AttachmentManager;

class AttachmentDialog : public QDialog {
    Q_OBJECT
public:
    explicit AttachmentDialog(AttachmentManager* manager, QWidget* parent = nullptr);
    ~AttachmentDialog() override;

private slots:
    void onAddDxf();
    void onAddDwg();
    void onAddSnt();
    void onRemoveSelected();
    void onRemoveAll();
    void onItemSelectionChanged();
    void onLayerItemChanged(QListWidgetItem* item);

private:
    void rebuildFileList();
    void rebuildLayerList();
    void updateButtonStates();

    AttachmentManager* m_manager;
    QListWidget* m_fileList;
    QListWidget* m_layerList;
    QPushButton* m_addDxfBtn;
    QPushButton* m_addDwgBtn;
    QPushButton* m_addSntBtn;
    QPushButton* m_removeBtn;
    QPushButton* m_removeAllBtn;
    QProgressBar* m_progressBar;
};

class DisplayOptionsDialog : public QDialog {
    Q_OBJECT
public:
    enum class AttachmentType { DXF, DWG, SNT };
    explicit DisplayOptionsDialog(AttachmentType type, QWidget* parent = nullptr);

    QString displayMode() const;
    bool isOverrideEnabled() const;
    QColor overrideColor() const;
    double zOffset() const;

private:
    QRadioButton* m_overlayRadio;
    QRadioButton* m_underlayRadio;
    QCheckBox* m_overrideCheck;
    QComboBox* m_colorCombo;
    QDoubleSpinBox* m_zOffsetSpin;
};

class ShadingDisplayDialog : public QDialog {
    Q_OBJECT
public:
    explicit ShadingDisplayDialog(QWidget* parent = nullptr);

    float azimuth() const;
    float elevation() const;
    float ambient() const;
    float maxEdgeLength() const;
    int qualityLevel() const;

signals:
    void shadingRequested();

private slots:
    void onPresetMicroStation();
    void onPresetArcGIS();

private:
    QDoubleSpinBox* m_azimuthSpin;
    QDoubleSpinBox* m_elevationSpin;
    QDoubleSpinBox* m_ambientSpin;
    QDoubleSpinBox* m_maxEdgeSpin;
    QComboBox* m_qualityCombo;
};

} // namespace cad
} // namespace workstation
