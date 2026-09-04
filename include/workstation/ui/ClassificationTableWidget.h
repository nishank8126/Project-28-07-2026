#pragma once
#include <QDialog>
#include "workstation/display/DisplayModeManager.h"

class QTableWidget;
class QTableWidgetItem;
class QPushButton;

namespace workstation {
namespace ui {

class ViewportWindow;

class ClassificationTableWidget : public QDialog {
    Q_OBJECT
public:
    ClassificationTableWidget(const display::ClassPalette& palette,
                               ViewportWindow* viewport,
                               QWidget* parent = nullptr);

    void updatePalette(const display::ClassPalette& palette);

signals:
    void classificationVisibilityChanged(int classCode, bool visible);

private slots:
    void onLoadPtc();
    void onToggleAll(bool checked);
    void onVisibilityChanged(int row, int column);

private:
    void buildTable();
    void populateTable();

    QTableWidget* table_ = nullptr;
    QPushButton* loadBtn_ = nullptr;
    QPushButton* toggleAllBtn_ = nullptr;
    display::ClassPalette palette_;
    ViewportWindow* viewport_ = nullptr;
};

} // namespace ui
} // namespace workstation
