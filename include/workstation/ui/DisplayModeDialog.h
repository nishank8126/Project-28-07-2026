#pragma once
#include "workstation/display/DisplayModeManager.h"
#include <QDialog>
#include <QTableWidget>
#include <QComboBox>
#include <QLabel>
#include <QPushButton>
#include <QLineEdit>
#include <QCheckBox>
#include <QMenu>
#include <QAction>
#include <QSettings>
#include <memory>

namespace workstation {
namespace ui {

class ViewportWindow;

class DisplayModeDialog : public QDialog {
    Q_OBJECT
public:
    explicit DisplayModeDialog(ViewportWindow* viewport, QWidget* parent = nullptr);
    ~DisplayModeDialog() override = default;

    void syncWithRenderer();
    void loadSettings();
    void saveSettings();

protected:
    void closeEvent(QCloseEvent* event) override;

signals:
    void modeChanged(int mode);
    void paletteChanged(int slot);
    void borderChanged(int slot, float percent, int mode);

public slots:
    void onApply();
    void onSlotChanged(int idx);
    void onModeChanged(int idx);
    void onClassToggled(int row, int state);
    void onWeightChanged(int row);
    void onBorderIncrease();
    void onBorderDecrease();
    void onBorderModeChanged(int mode);
    void onAddClass();
    void onEditClass();
    void onRemoveClass();
    void onSelectAll();
    void onClearAll();
    void onLoadPtc();
    void onSavePtc();
    void onSavePtcAs();

private:
    void setupUI();
    void rebuildTable();
    void saveSlotState(int slot);
    void loadSlotState(int slot);
    void updateBorderDisplay();
    void pushBorderToGpu(int slot, float border);

    ViewportWindow* m_viewport = nullptr;
    display::DisplayModeManager m_manager;

    QComboBox* m_slotCombo = nullptr;
    QComboBox* m_modeCombo = nullptr;
    QTableWidget* m_table = nullptr;
    QLabel* m_borderLabel = nullptr;
    QLabel* m_borderValue = nullptr;
    QPushButton* m_borderMinus = nullptr;
    QPushButton* m_borderPlus = nullptr;
    QPushButton* m_applyBtn = nullptr;
    QPushButton* m_closeBtn = nullptr;
    QPushButton* m_addBtn = nullptr;
    QPushButton* m_editBtn = nullptr;
    QPushButton* m_delBtn = nullptr;
    QPushButton* m_selectAllBtn = nullptr;
    QPushButton* m_clearAllBtn = nullptr;
    QPushButton* m_loadPtcBtn = nullptr;
    QPushButton* m_savePtcBtn = nullptr;

    int m_currentSlot = 0;
};

} // namespace ui
} // namespace workstation
