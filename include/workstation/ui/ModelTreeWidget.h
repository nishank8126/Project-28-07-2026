#pragma once
#include <QWidget>

class QTreeWidget;

namespace workstation {
class Document;
namespace ui {

// Left dock content: shows the document/model tree.
//
// GUI Shell 1 shows the truthful empty state ("No document loaded") rather
// than fabricating CAD elements. It reads only the existing Document/Model
// public API; it creates no file-loading or element-inference behavior.
class ModelTreeWidget : public QWidget {
    Q_OBJECT
public:
    explicit ModelTreeWidget(QWidget* parent = nullptr);

    // Populate from the existing core Document. With no models the tree shows
    // the empty state. No elements are fabricated.
    void setDocument(const Document* document);

private:
    QTreeWidget* m_tree = nullptr;
};

} // namespace ui
} // namespace workstation
