// De-RAS - ui/View.cpp
#include "View.h"

#include "Common.h"

namespace deras::ui {

QList<QPair<QString, QString>> View::crumbs() const { return {{tr2("Beranda", "Home"), QStringLiteral("home")}}; }

QWidget* View::actions() {
    if (!actionsMade_) {
        actionsMade_ = true;
        actions_ = createActions();
    }
    return actions_;
}

}  // namespace deras::ui
