// MIT License
//
// Copyright (c) 2019 Ruhr University Bochum, Chair for Embedded Security. All Rights reserved.
// Copyright (c) 2019 Marc Fyrbiak, Sebastian Wallat, Max Hoffmann ("ORIGINAL AUTHORS"). All rights reserved.
// Copyright (c) 2021 Max Planck Institute for Security and Privacy. All Rights reserved.
// Copyright (c) 2021 Jörn Langheinrich, Julian Speith, Nils Albartus, René Walendy, Simon Klix ("ORIGINAL AUTHORS"). All Rights reserved.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#pragma once

#include "hal_core/netlist/gate_library/gate_library.h"
#include "gui/gatelibrary_management/gatelibrary_wizard.h"
#include "gatelibrary_table_model.h"
#include "gatelibrary_tab_widgets/gatelibrary_tab_flip_flop.h"


#include <QFrame>
#include <QGridLayout>
#include <QTableView>
#include <QPushButton>
#include <QTabWidget>

namespace hal
{


    class GateLibraryManager : public QFrame
    {
        Q_OBJECT

    public:
        /**
         * Constructor.
         *
         * @param parent - the parent widget
         */
        explicit GateLibraryManager(QWidget* parent = nullptr);

        /**
         * Reinitializes the appearance of the widget and its children.
         */
        void repolish();

        bool initialize(GateLibrary* gateLibrary = nullptr);

    public Q_SLOTS:
        void handleCallWizard();
        void handleSelectionChanged(const QModelIndex& index);


    private:

        QTabWidget* mTabWidget;
        QGridLayout* mLayout;
        QPushButton* mEditBtn;
        QPushButton* mAddBtn;
        QTableView* mTableView;
        GatelibraryTableModel* mTableModel;

        GateLibraryTabFlipFlop* mFlipFlopTab;

        const GateLibrary* mGateLibrary;

        bool mReadOnly = false;
    };
}
