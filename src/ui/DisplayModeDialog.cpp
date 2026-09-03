#include "workstation/ui/DisplayModeDialog.h"
#include "workstation/ui/ViewportWidget.h"
#include "workstation/renderer/Renderer.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QHeaderView>
#include <QFileDialog>
#include <QMessageBox>
#include <QSettings>
#include <QFileInfo>
#include <QCloseEvent>

namespace workstation {
namespace ui {

DisplayModeDialog::DisplayModeDialog(ViewportWindow* viewport, QWidget* parent)
    : QDialog(parent), m_viewport(viewport) {
    setWindowTitle("Display Mode");
    resize(520, 680);
    setMinimumSize(420, 400);
    setupUI();
    loadSettings();
    syncWithRenderer();
}

void DisplayModeDialog::setupUI() {
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(10, 8, 10, 10);
    mainLayout->setSpacing(6);

    auto* controlsCard = new QFrame();
    auto* controlsLayout = new QHBoxLayout(controlsCard);
    controlsLayout->setContentsMargins(10, 8, 10, 8);
    controlsLayout->setSpacing(8);

    m_slotCombo = new QComboBox();
    m_slotCombo->addItems({"Main View", "View 1", "View 2", "View 3", "View 4"});
    m_slotCombo->setMinimumWidth(110);
    connect(m_slotCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &DisplayModeDialog::onSlotChanged);
    controlsLayout->addWidget(m_slotCombo, 2);

    m_modeCombo = new QComboBox();
    m_modeCombo->addItems({
        "By Classification", "Shaded Classification", "Depth", "Intensity",
        "RGB", "Elevation", "Surface", "Line"
    });
    m_modeCombo->setMinimumWidth(110);
    connect(m_modeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &DisplayModeDialog::onModeChanged);
    controlsLayout->addWidget(m_modeCombo, 2);

    mainLayout->addWidget(controlsCard);

    auto* tableCard = new QFrame();
    auto* tableLayout = new QVBoxLayout(tableCard);
    tableLayout->setContentsMargins(12, 12, 12, 12);
    tableLayout->setSpacing(12);

    m_table = new QTableWidget(0, 7);
    m_table->setHorizontalHeaderLabels({"Show", "Code", "Description", "Draw", "Lvl", "Color", "Weight"});
    m_table->setAlternatingRowColors(true);
    m_table->verticalHeader()->setVisible(false);
    m_table->verticalHeader()->setDefaultSectionSize(38);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setFocusPolicy(Qt::NoFocus);
    m_table->setWordWrap(false);
    m_table->setShowGrid(false);

    auto* hdr = m_table->horizontalHeader();
    for (int i = 0; i < 7; ++i) hdr->setSectionResizeMode(i, QHeaderView::Interactive);
    hdr->setStretchLastSection(true);
    m_table->setColumnWidth(0, 60);
    m_table->setColumnWidth(1, 60);
    m_table->setColumnWidth(2, 140);
    m_table->setColumnWidth(3, 80);
    m_table->setColumnWidth(4, 56);
    m_table->setColumnWidth(5, 75);
    m_table->setColumnWidth(6, 65);

    tableLayout->addWidget(m_table);

    auto* actionRail = new QHBoxLayout();
    m_addBtn = new QPushButton("Add");
    m_editBtn = new QPushButton("Edit");
    m_delBtn = new QPushButton("Delete");
    m_selectAllBtn = new QPushButton("Select All");
    m_clearAllBtn = new QPushButton("Clear All");

    for (auto* btn : {m_addBtn, m_editBtn, m_delBtn, m_selectAllBtn, m_clearAllBtn}) {
        btn->setAutoDefault(false);
        btn->setDefault(false);
        btn->setFocusPolicy(Qt::NoFocus);
        actionRail->addWidget(btn, 1);
    }
    tableLayout->addLayout(actionRail);
    mainLayout->addWidget(tableCard);

    auto* bottom = new QHBoxLayout();
    bottom->setSpacing(6);
    bottom->addStretch();

    m_borderLabel = new QLabel("Border");
    m_borderMinus = new QPushButton("-");
    m_borderMinus->setFixedWidth(26);
    m_borderMinus->setAutoDefault(false);
    m_borderMinus->setFocusPolicy(Qt::NoFocus);
    m_borderValue = new QLabel("0%");
    m_borderValue->setFixedWidth(42);
    m_borderValue->setAlignment(Qt::AlignCenter);
    m_borderPlus = new QPushButton("+");
    m_borderPlus->setFixedWidth(26);
    m_borderPlus->setAutoDefault(false);
    m_borderPlus->setFocusPolicy(Qt::NoFocus);

    m_applyBtn = new QPushButton("Apply");
    m_applyBtn->setAutoDefault(false);
    m_applyBtn->setDefault(false);
    m_applyBtn->setFocusPolicy(Qt::NoFocus);
    m_closeBtn = new QPushButton("Close");
    m_closeBtn->setAutoDefault(false);
    m_closeBtn->setDefault(false);
    m_closeBtn->setFocusPolicy(Qt::NoFocus);

    bottom->addWidget(m_borderLabel);
    bottom->addWidget(m_borderMinus);
    bottom->addWidget(m_borderValue);
    bottom->addWidget(m_borderPlus);
    bottom->addSpacing(8);
    bottom->addWidget(m_applyBtn);
    bottom->addWidget(m_closeBtn);
    mainLayout->addLayout(bottom);

    connect(m_applyBtn, &QPushButton::clicked, this, &DisplayModeDialog::onApply);
    connect(m_closeBtn, &QPushButton::clicked, this, &DisplayModeDialog::hide);
    connect(m_addBtn, &QPushButton::clicked, this, &DisplayModeDialog::onAddClass);
    connect(m_editBtn, &QPushButton::clicked, this, &DisplayModeDialog::onEditClass);
    connect(m_delBtn, &QPushButton::clicked, this, &DisplayModeDialog::onRemoveClass);
    connect(m_selectAllBtn, &QPushButton::clicked, this, &DisplayModeDialog::onSelectAll);
    connect(m_clearAllBtn, &QPushButton::clicked, this, &DisplayModeDialog::onClearAll);
    connect(m_borderMinus, &QPushButton::clicked, this, &DisplayModeDialog::onBorderDecrease);
    connect(m_borderPlus, &QPushButton::clicked, this, &DisplayModeDialog::onBorderIncrease);

    connect(m_table, &QTableWidget::cellChanged, this, &DisplayModeDialog::onWeightChanged);

    loadSlotState(0);
}

void DisplayModeDialog::rebuildTable() {
    m_table->blockSignals(true);
    m_table->setRowCount(0);

    const auto& palette = m_manager.mainPalette();
    for (const auto& [code, entry] : palette) {
        int row = m_table->rowCount();
        m_table->insertRow(row);

        auto* chk = new QCheckBox();
        chk->setChecked(entry.visible);
        chk->setFocusPolicy(Qt::NoFocus);
        connect(chk, &QCheckBox::checkStateChanged, this, [this, row](Qt::CheckState state) { onClassToggled(row, static_cast<int>(state)); });
        m_table->setCellWidget(row, 0, chk);

        m_table->setItem(row, 1, new QTableWidgetItem(QString::number(code)));
        m_table->setItem(row, 2, new QTableWidgetItem(QString::fromStdString(entry.description)));
        m_table->setItem(row, 3, new QTableWidgetItem(QString::fromStdString(entry.drawMode)));
        m_table->setItem(row, 4, new QTableWidgetItem(QString::fromStdString(entry.level)));

        auto* colorItem = new QTableWidgetItem();
        QColor color(entry.color[0], entry.color[1], entry.color[2]);
        colorItem->setBackground(color);
        m_table->setItem(row, 5, colorItem);

        m_table->setItem(row, 6, new QTableWidgetItem(QString::number(entry.weight, 'f', 2)));

        for (int col : {1, 4, 6}) {
            auto* item = m_table->item(row, col);
            if (item) item->setTextAlignment(Qt::AlignCenter);
        }
    }
    m_table->blockSignals(false);
}

void DisplayModeDialog::saveSlotState(int slot) {
    auto& vis = m_manager.slot(slot).visibility;
    auto& pal = m_manager.slot(slot).palette;

    for (int row = 0; row < m_table->rowCount(); ++row) {
        auto* codeItem = m_table->item(row, 1);
        if (!codeItem) continue;
        int code = codeItem->text().toInt();

        auto* chk = qobject_cast<QCheckBox*>(m_table->cellWidget(row, 0));
        bool visible = chk ? chk->isChecked() : true;
        vis[code] = visible;

        auto* wtItem = m_table->item(row, 6);
        float weight = wtItem ? wtItem->text().toFloat() : 1.0f;

        auto it = pal.find(code);
        if (it != pal.end()) {
            it->second.visible = visible;
            it->second.weight = weight;
        }
    }
}

void DisplayModeDialog::loadSlotState(int slot) {
    m_table->blockSignals(true);
    const auto& pal = m_manager.slot(slot).palette;
    const auto& vis = m_manager.slot(slot).visibility;

    for (int row = 0; row < m_table->rowCount(); ++row) {
        auto* codeItem = m_table->item(row, 1);
        if (!codeItem) continue;
        int code = codeItem->text().toInt();

        auto* chk = qobject_cast<QCheckBox*>(m_table->cellWidget(row, 0));
        if (chk) {
            auto visIt = vis.find(code);
            auto palIt = pal.find(code);
            bool show = (visIt != vis.end()) ? visIt->second
                      : (palIt != pal.end()) ? palIt->second.visible : true;
            chk->setChecked(show);
        }

        auto* wtItem = m_table->item(row, 6);
        if (wtItem) {
            auto palIt = pal.find(code);
            float weight = (palIt != pal.end()) ? palIt->second.weight : 1.0f;
            wtItem->setText(QString::number(weight, 'f', 2));
        }
    }
    m_table->blockSignals(false);
}

void DisplayModeDialog::syncWithRenderer() {
    if (!m_viewport) return;
    rebuildTable();
    loadSlotState(m_currentSlot);
    updateBorderDisplay();
}

void DisplayModeDialog::onSlotChanged(int idx) {
    saveSlotState(m_currentSlot);
    m_currentSlot = idx;
    m_manager.setCurrentSlot(idx);
    loadSlotState(idx);
    updateBorderDisplay();
}

void DisplayModeDialog::onModeChanged(int idx) {
    m_manager.setCurrentMode(static_cast<display::DisplayModeManager::Mode>(idx));
}

void DisplayModeDialog::onClassToggled(int row, int state) {
    auto* codeItem = m_table->item(row, 1);
    if (!codeItem) return;
    int code = codeItem->text().toInt();
    m_manager.setClassVisible(m_currentSlot, code, state != 0);
}

void DisplayModeDialog::onWeightChanged(int row) {
    if (row < 0 || row >= m_table->rowCount()) return;
    auto* codeItem = m_table->item(row, 1);
    auto* wtItem = m_table->item(row, 6);
    if (!codeItem || !wtItem) return;
    int code = codeItem->text().toInt();
    float weight = wtItem->text().toFloat();
    weight = std::max(0.1f, std::min(weight, 12.0f));
    m_table->blockSignals(true);
    wtItem->setText(QString::number(weight, 'f', 2));
    m_table->blockSignals(false);
    m_manager.setClassWeight(m_currentSlot, code, weight);
}

void DisplayModeDialog::updateBorderDisplay() {
    float border = m_manager.borderPercent(m_currentSlot);
    m_borderValue->setText(QString::number(static_cast<int>(border)) + "%");
}

void DisplayModeDialog::pushBorderToGpu(int slot, float border) {
    m_manager.setBorderPercent(slot, border);
    emit borderChanged(slot, border, static_cast<int>(m_manager.borderMode(slot)));
}

void DisplayModeDialog::onBorderIncrease() {
    float current = m_manager.borderPercent(m_currentSlot);
    float newVal = std::min(100.0f, current + 5.0f);
    pushBorderToGpu(m_currentSlot, newVal);
    updateBorderDisplay();
}

void DisplayModeDialog::onBorderDecrease() {
    float current = m_manager.borderPercent(m_currentSlot);
    float newVal = std::max(0.0f, current - 5.0f);
    pushBorderToGpu(m_currentSlot, newVal);
    updateBorderDisplay();
}

void DisplayModeDialog::onBorderModeChanged(int mode) {
    m_manager.setBorderMode(m_currentSlot, static_cast<display::DisplayModeManager::BorderMode>(mode));
    pushBorderToGpu(m_currentSlot, m_manager.borderPercent(m_currentSlot));
}

void DisplayModeDialog::onAddClass() {
    auto& pal = m_manager.mainPalette();
    int maxCode = 0;
    for (const auto& [code, entry] : pal) maxCode = std::max(maxCode, code);
    int newCode = maxCode + 1;

    display::ClassEntry entry;
    entry.code = newCode;
    entry.description = "New Class " + std::to_string(newCode);
    entry.visible = true;
    entry.weight = 1.0f;
    pal[newCode] = entry;
    rebuildTable();
}

void DisplayModeDialog::onEditClass() {
    int row = m_table->currentRow();
    if (row < 0) return;
    auto* codeItem = m_table->item(row, 1);
    if (!codeItem) return;
    int code = codeItem->text().toInt();
    auto& pal = m_manager.mainPalette();
    auto it = pal.find(code);
    if (it == pal.end()) return;

    auto& entry = it->second;
    entry.description = m_table->item(row, 2) ? m_table->item(row, 2)->text().toStdString() : "";
    entry.drawMode = m_table->item(row, 3) ? m_table->item(row, 3)->text().toStdString() : "";
    entry.level = m_table->item(row, 4) ? m_table->item(row, 4)->text().toStdString() : "";
    auto* colorItem = m_table->item(row, 5);
    if (colorItem) {
        QColor c = colorItem->background().color();
        entry.color = {static_cast<uint8_t>(c.red()), static_cast<uint8_t>(c.green()), static_cast<uint8_t>(c.blue())};
    }
}

void DisplayModeDialog::onRemoveClass() {
    int row = m_table->currentRow();
    if (row < 0) return;
    auto* codeItem = m_table->item(row, 1);
    if (!codeItem) return;
    int code = codeItem->text().toInt();
    m_manager.mainPalette().erase(code);
    rebuildTable();
}

void DisplayModeDialog::onSelectAll() {
    for (int row = 0; row < m_table->rowCount(); ++row) {
        auto* chk = qobject_cast<QCheckBox*>(m_table->cellWidget(row, 0));
        if (chk) chk->setChecked(true);
    }
}

void DisplayModeDialog::onClearAll() {
    for (int row = 0; row < m_table->rowCount(); ++row) {
        auto* chk = qobject_cast<QCheckBox*>(m_table->cellWidget(row, 0));
        if (chk) chk->setChecked(false);
    }
}

void DisplayModeDialog::onLoadPtc() {
    QString path = QFileDialog::getOpenFileName(this, "Open Class Table", "", "Point Class Table (*.ptc)");
    if (path.isEmpty()) return;
    std::string error;
    m_manager.loadPtc(path.toStdString(), &error);
    if (!error.empty()) {
        QMessageBox::warning(this, "Load Failed", QString::fromStdString(error));
        return;
    }
    rebuildTable();
    loadSlotState(m_currentSlot);
}

void DisplayModeDialog::onSavePtc() {
    if (m_manager.ptcPath().empty()) {
        onSavePtcAs();
        return;
    }
    std::string error;
    m_manager.savePtc(m_manager.ptcPath(), &error);
    if (!error.empty()) QMessageBox::warning(this, "Save Failed", QString::fromStdString(error));
}

void DisplayModeDialog::onSavePtcAs() {
    QString path = QFileDialog::getSaveFileName(this, "Save Class Table As", "", "Point Class Table (*.ptc)");
    if (path.isEmpty()) return;
    std::string error;
    m_manager.savePtc(path.toStdString(), &error);
    if (!error.empty()) QMessageBox::warning(this, "Save Failed", QString::fromStdString(error));
}

void DisplayModeDialog::onApply() {
    saveSlotState(m_currentSlot);
    m_manager.syncFromMainPalette();
    emit paletteChanged(m_currentSlot);
    emit modeChanged(static_cast<int>(m_manager.currentMode()));
}

void DisplayModeDialog::loadSettings() {
    QSettings settings("NakshaAI", "WorkstationCAD");

    auto savedGeo = settings.value("display_dialog_geometry");
    if (!savedGeo.isNull()) restoreGeometry(savedGeo.toByteArray());

    auto savedMode = settings.value("display_color_mode");
    if (!savedMode.isNull()) m_modeCombo->setCurrentIndex(savedMode.toInt());

    auto savedBorders = settings.value("display_view_borders");
    if (savedBorders.canConvert<QVariantMap>()) {
        QVariantMap map = savedBorders.toMap();
        for (auto it = map.begin(); it != map.end(); ++it) {
            int slot = it.key().toInt();
            float border = it.value().toFloat();
            m_manager.setBorderPercent(slot, border);
        }
    }

    auto savedPtcs = settings.value("display_last_ptc");
    if (!savedPtcs.isNull()) {
        std::string ptcPath = savedPtcs.toString().toStdString();
        if (!ptcPath.empty()) {
            std::string error;
            m_manager.loadPtc(ptcPath, &error);
            rebuildTable();
        }
    }
}

void DisplayModeDialog::saveSettings() {
    QSettings settings("NakshaAI", "WorkstationCAD");
    settings.setValue("display_dialog_geometry", saveGeometry());
    settings.setValue("display_color_mode", m_modeCombo->currentIndex());
    settings.setValue("display_last_ptc", QString::fromStdString(m_manager.ptcPath()));

    QVariantMap borders;
    for (int i = 0; i < display::DisplayModeManager::SLOT_COUNT; ++i) {
        borders[QString::number(i)] = m_manager.borderPercent(i);
    }
    settings.setValue("display_view_borders", borders);
    settings.sync();
}

void DisplayModeDialog::closeEvent(QCloseEvent* event) {
    saveSettings();
    hide();
    event->ignore();
}

} // namespace ui
} // namespace workstation
