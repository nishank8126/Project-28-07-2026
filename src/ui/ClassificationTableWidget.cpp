#include "workstation/ui/ClassificationTableWidget.h"
#include "workstation/ui/ViewportWidget.h"

#include <QTableWidget>
#include <QHeaderView>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QFileDialog>
#include <QMessageBox>
#include <QLabel>
#include <QColorDialog>

namespace workstation {
namespace ui {

ClassificationTableWidget::ClassificationTableWidget(
    const display::ClassPalette& palette,
    ViewportWindow* viewport,
    QWidget* parent)
    : QDialog(parent)
    , palette_(palette)
    , viewport_(viewport)
{
    setWindowTitle("Classification Table");
    setMinimumSize(520, 440);
    resize(600, 500);

    auto* mainLayout = new QVBoxLayout(this);

    // Top row: Load PTC + Toggle All
    auto* topRow = new QHBoxLayout;
    loadBtn_ = new QPushButton("Load PTC...");
    connect(loadBtn_, &QPushButton::clicked, this, &ClassificationTableWidget::onLoadPtc);
    topRow->addWidget(loadBtn_);

    toggleAllBtn_ = new QPushButton("Toggle All");
    toggleAllBtn_->setCheckable(true);
    toggleAllBtn_->setChecked(true);
    connect(toggleAllBtn_, &QPushButton::toggled, this, &ClassificationTableWidget::onToggleAll);
    topRow->addWidget(toggleAllBtn_);

    topRow->addStretch();
    mainLayout->addLayout(topRow);

    // Table
    table_ = new QTableWidget(this);
    table_->setColumnCount(4);
    table_->setHorizontalHeaderLabels({"Code", "Description", "Color", "Visible"});
    table_->horizontalHeader()->setStretchLastSection(true);
    table_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    table_->verticalHeader()->hide();
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_->setSelectionMode(QAbstractItemView::SingleSelection);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    connect(table_, &QTableWidget::cellChanged, this, &ClassificationTableWidget::onVisibilityChanged);
    mainLayout->addWidget(table_);

    populateTable();
}

void ClassificationTableWidget::updatePalette(const display::ClassPalette& palette) {
    palette_ = palette;
    populateTable();
}

void ClassificationTableWidget::populateTable() {
    table_->blockSignals(true);
    table_->setRowCount(0);

    // Sort entries by code for a stable display.
    std::vector<std::pair<int, display::ClassEntry>> sorted(palette_.begin(), palette_.end());
    std::sort(sorted.begin(), sorted.end(),
              [](const auto& a, const auto& b) { return a.first < b.first; });

    table_->setRowCount(static_cast<int>(sorted.size()));

    for (int row = 0; row < static_cast<int>(sorted.size()); ++row) {
        const auto& [code, entry] = sorted[row];

        // Code
        auto* codeItem = new QTableWidgetItem(QString::number(code));
        codeItem->setTextAlignment(Qt::AlignCenter);
        table_->setItem(row, 0, codeItem);

        // Description
        table_->setItem(row, 1, new QTableWidgetItem(
            QString::fromStdString(entry.description)));

        // Color swatch
        auto* colorItem = new QTableWidgetItem();
        QColor c(entry.color[0], entry.color[1], entry.color[2]);
        colorItem->setBackground(c);
        colorItem->setData(Qt::UserRole, code);
        table_->setItem(row, 2, colorItem);

        // Visible checkbox
        auto* visItem = new QTableWidgetItem();
        visItem->setFlags(visItem->flags() | Qt::ItemIsUserCheckable);
        visItem->setCheckState(entry.visible ? Qt::Checked : Qt::Unchecked);
        visItem->setData(Qt::UserRole, code);
        table_->setItem(row, 3, visItem);
    }

    table_->blockSignals(false);
}

void ClassificationTableWidget::onLoadPtc() {
    QString path = QFileDialog::getOpenFileName(
        this, "Load Classification PTC", QString(),
        "PTC Files (*.ptc);;All Files (*)");
    if (path.isEmpty()) return;

    display::ClassPalette newPalette;
    std::string error;
    if (!display::PtcFileReader::load(path.toStdString(), newPalette, &error)) {
        QMessageBox::warning(this, "PTC Load Failed",
                             QString::fromStdString(error));
        return;
    }

    palette_ = newPalette;
    populateTable();

    // Push the palette to the renderer.
    if (viewport_) {
        viewport_->LoadClassificationPTC(path);
    }

    toggleAllBtn_->setChecked(true);
}

void ClassificationTableWidget::onToggleAll(bool checked) {
    table_->blockSignals(true);
    for (int row = 0; row < table_->rowCount(); ++row) {
        auto* visItem = table_->item(row, 3);
        if (visItem) {
            visItem->setCheckState(checked ? Qt::Checked : Qt::Unchecked);
            int code = visItem->data(Qt::UserRole).toInt();
            palette_[code].visible = checked;
            if (viewport_) {
                viewport_->UpdateClassificationVisibility(code, checked);
            }
        }
    }
    table_->blockSignals(false);
}

void ClassificationTableWidget::onVisibilityChanged(int row, int column) {
    if (column != 3) return;

    auto* visItem = table_->item(row, 3);
    if (!visItem) return;

    int code = visItem->data(Qt::UserRole).toInt();
    bool visible = (visItem->checkState() == Qt::Checked);
    palette_[code].visible = visible;

    if (viewport_) {
        viewport_->UpdateClassificationVisibility(code, visible);
    }
}

} // namespace ui
} // namespace workstation
