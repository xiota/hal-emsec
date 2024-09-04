#include "gui/gatelibrary_management/gatelibrary_manager.h"

#include "gui/gatelibrary_management/gatelibrary_graphics_view.h"
#include "gui/graphics_effects/shadow_effect.h"
#include "gui/plugin_relay/gui_plugin_manager.h"
#include "gui/main_window/main_window.h"
#include "hal_core/netlist/gate_library/gate_library_manager.h"
#include "hal_core/netlist/netlist_factory.h"
#include "hal_core/plugin_system/fac_extension_interface.h"

#include <QGridLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGraphicsView>
#include <QTabWidget>
#include <QTableView>

#include <QDebug>
#include <QDir>
#include <QFileDialog>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QTableWidget>
#include <QSplitter>
#include <QDialogButtonBox>
#include <gui/gui_globals.h>

namespace hal
{
    GateLibraryManager::GateLibraryManager(MainWindow *parent)
        : QFrame(parent), mFrameWidth(0), mLayout(new QGridLayout()), mNonEditableGateLibrary(nullptr), mEditableGatelibrary(nullptr)
    {

        //TODO: GateLibrarymanager will stay in readOnly mode even if closing project and opening a new gateLibrary
        mSplitter = new QSplitter(this);
        QWidget* rightWindow = new QWidget(mSplitter);
        QGridLayout* rlay = new QGridLayout(rightWindow);
        QDialogButtonBox* bbox = new QDialogButtonBox(QDialogButtonBox::Cancel | QDialogButtonBox::Ok, Qt::Horizontal, rightWindow);

        mTableModel = new GatelibraryTableModel(this);
        mContentWidget = new GatelibraryContentWidget(mTableModel,mSplitter);

        //pages for the tab widget
        mGeneralTab = new GateLibraryTabGeneral(this);
        mPinTab = new GateLibraryTabPin(this);
        mBooleanFunctionTab = new GateLibraryTabTruthTable(this);


        //buttons
        mOkBtn = bbox->button(QDialogButtonBox::Ok);
        mOkBtn->setDisabled(true);
        mCancelBtn = bbox->button(QDialogButtonBox::Cancel);
        mCancelBtn->setEnabled(true);

        //adding pages to the tab widget
        mTabWidget = new QTabWidget(this);
        mTabWidget->addTab(mGeneralTab, "Gate Type");
        mTabWidget->addTab(mPinTab, "Pins");
        mTabWidget->addTab(mBooleanFunctionTab, "Truth Table"); // TODO : implement truth table

        mGraphicsView = new GatelibraryGraphicsView(this);
        QGraphicsScene* sc = new QGraphicsScene(mGraphicsView);
        sc->setSceneRect(0,0,300,1200);
        mGraphicsView->setScene(sc);

        rlay->addWidget(mTabWidget,0,0);
        rlay->addWidget(mGraphicsView,0,1);
        rlay->addWidget(bbox,1,0,1,2);

        // Add widgets to the layout
        mSplitter->addWidget(mContentWidget);
        mSplitter->addWidget(rightWindow);

        mLayout->addWidget(mSplitter);

        //signal - slots
//        connect(mAddBtn, &QPushButton::clicked, this, &GateLibraryManager::handleAddWizard);
        //connect(mEditBtn, &QPushButton::clicked, this, &GateLibraryManager::handleEditWizard);
 //       connect(mTableView, &QTableView::clicked, this, &GateLibraryManager::handleSelectionChanged);
        connect(mCancelBtn, &QPushButton::clicked, this, &GateLibraryManager::handleCancelClicked);
        connect(mContentWidget, &GatelibraryContentWidget::triggerCurrentSelectionChanged, this, &GateLibraryManager::handleSelectionChanged);
        connect(mContentWidget->mAddAction, &QAction::triggered, this, &GateLibraryManager::handleAddWizard);
        connect(mContentWidget, &GatelibraryContentWidget::triggerEditType, this, &GateLibraryManager::handleEditWizard);
        connect(mContentWidget, &GatelibraryContentWidget::triggerDeleteType, this, &GateLibraryManager::handleDeleteType);
        connect(mContentWidget, &GatelibraryContentWidget::triggerDoubleClicked, this, &GateLibraryManager::handleEditWizard);

        //connect(mWizard, &QDialog::accepted, this, &GateLibraryManager::handleSelectionChanged);

        setLayout(mLayout);
        repolish();    // CALL FROM PARENT
    }

    void GateLibraryManager::repolish()
    {
        QStyle* s = style();

        s->unpolish(this);
        s->polish(this);
    }

    void GateLibraryManager::handleSaveAction()
    {
        mContentWidget->handleSaveAction();
    }

    void GateLibraryManager::handleSaveAsAction()
    {
        mContentWidget->handleSaveAsAction();
    }

    bool GateLibraryManager::initialize(GateLibrary* gateLibrary, bool readOnly)
    {
        if(!gateLibrary)
        {
            if(gNetlist && gNetlist->get_gate_library()){
                //TODO find better way to handle const/not const gateLibrary
                mNonEditableGateLibrary = gNetlist->get_gate_library();
                mDemoNetlist = netlist_factory::create_netlist(mNonEditableGateLibrary);
                mReadOnly = true;

            }
            else
            {
                QString title = "Load gate library";
                QString text  = "HAL Gate Library (*.hgl *.lib)";
                QString path  = QDir::currentPath();
                QFile gldpath(":/path/gate_library_definitions");
                if (gldpath.open(QIODevice::ReadOnly))
                    path = QString::fromUtf8(gldpath.readAll());

                //TODO fileDialog throws bad window error
                QString fileName = QFileDialog::getOpenFileName(nullptr, title, path, text, nullptr);
                if (fileName == nullptr)
                    return false;

                if (gPluginRelay->mGuiPluginTable)
                    gPluginRelay->mGuiPluginTable->loadFeature(FacExtensionInterface::FacGatelibParser);

                auto gateLibrary = gate_library_manager::load(std::filesystem::path(fileName.toStdString()));

                mEditableGatelibrary = gateLibrary;
                mDemoNetlist = netlist_factory::create_netlist(mEditableGatelibrary);
                mReadOnly = false;

                QDir dir(QDir::home());
                window()->setWindowTitle(QString("GateLibrary %1").arg(dir.relativeFilePath(fileName)));
                mPath = fileName.toStdString();
            }

        }
        else
        {
            mReadOnly = readOnly;
            if(mReadOnly)
            {
                mDemoNetlist = netlist_factory::create_netlist(mNonEditableGateLibrary);
                mNonEditableGateLibrary = gateLibrary;
            }
            else
            {
                mDemoNetlist = netlist_factory::create_netlist(mEditableGatelibrary);
                mEditableGatelibrary = gateLibrary;
            }
        }
        mGraphicsView->showGate(nullptr);
        updateTabs(nullptr);
        mTableModel->loadFile(mReadOnly ? mNonEditableGateLibrary : mEditableGatelibrary);
        mContentWidget->activate(mReadOnly);

        mContentWidget->toggleSelection(false);
        return true;
    }

    void GateLibraryManager::handleEditWizard(const QModelIndex& index)
    {
        if(mReadOnly)
            return;
        mWizard = new GateLibraryWizard(mEditableGatelibrary, mTableModel->getGateTypeAtIndex(index.row()));
        connect(mWizard, &GateLibraryWizard::triggerUnsavedChanges, mContentWidget, &GatelibraryContentWidget::handleUnsavedChanges);

        mWizard->exec();
        initialize(mEditableGatelibrary);

        mContentWidget->mTableView->selectRow(index.row());
        mContentWidget->setGateLibrary(mEditableGatelibrary);
        mContentWidget->setGateLibraryPath(mPath);
    }

    void GateLibraryManager::handleAddWizard()
    {
        mWizard = new GateLibraryWizard(mEditableGatelibrary);
        connect(mWizard, &GateLibraryWizard::triggerUnsavedChanges, mContentWidget, &GatelibraryContentWidget::handleUnsavedChanges);

        mWizard->exec();

        initialize(mEditableGatelibrary);

        for (int r=0; r<mTableModel->rowCount(); r++) {
            if(mTableModel->getGateTypeAtIndex(r) == mWizard->getRecentCreatedGate())
                mContentWidget->mTableView->selectRow(r);
        }
        mContentWidget->setGateLibrary(mEditableGatelibrary);
        mContentWidget->setGateLibraryPath(mPath);
    }

    void GateLibraryManager::handleDeleteType(QModelIndex index)
    {
        GateType* gate = mTableModel->getGateTypeAtIndex(index.row());
        mEditableGatelibrary->remove_gate_type(gate->get_name());
        initialize(mEditableGatelibrary);
        gFileStatusManager->gatelibChanged();
        //qInfo() << "handleDeleteType " << QString::fromStdString(gate->get_name()) << ":" << gate->get_id();
    }

    u32 GateLibraryManager::getNextGateId()
    {
        QSet<u32>* occupiedIds = new QSet<u32>;
        u32 freeId = 1;
        for (auto gt : mEditableGatelibrary->get_gate_types()) {
            occupiedIds->insert(gt.second->get_id());
        }
        while(occupiedIds->contains(freeId))
        {
            freeId++;
        }
        return freeId;
    }

    void GateLibraryManager::handleSelectionChanged(const QModelIndex& index, const QModelIndex &prevIndex)
    {
        Q_UNUSED(prevIndex);
        GateType* gateType;
        //get selected gate
        gateType = mTableModel->getGateTypeAtIndex(index.row());
        //qInfo() << "selected " << QString::fromStdString(gateType->get_name());

        if(!mReadOnly) mContentWidget->toggleSelection(true);
        //update tabs
        updateTabs(gateType);
        if (mDemoNetlist)
        {
            Gate* g = mDemoNetlist->get_gate_by_id(1);
            if (g) mDemoNetlist->delete_gate(g);
            g = mDemoNetlist.get()->create_gate(1,gateType,"Instance of");
            mGraphicsView->showGate(g);
        }
    }

    void GateLibraryManager::handleCancelClicked()
    {
        if(!gFileStatusManager->isGatelibModified())
            Q_EMIT close();
        else
        {
            QMessageBox* msgBox = new QMessageBox(this);
            msgBox->setWindowTitle("Unsaved changes");
            msgBox->setInformativeText("The current gate library has been modified. Do you want to save your changes or discard them?");
            msgBox->setStandardButtons(QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);

            int r = msgBox->exec();
            switch(r)
            {
                case QMessageBox::Save:
                    mContentWidget->handleSaveAsAction();
                    Q_EMIT close();
                break;
                case QMessageBox::Discard:
                    gate_library_manager::remove(std::filesystem::path(mEditableGatelibrary->get_path()));
                    window()->setWindowTitle("HAL");
                    Q_EMIT close();
                break;
                case QMessageBox::Cancel:
                    msgBox->reject();
                break;
            }
        }
    }

    GateType* GateLibraryManager::getSelectedGate()
    {
        PinProxyModel* proxyModel = mContentWidget->mPinProxyModel;
        QModelIndex index = mContentWidget->mTableView->currentIndex();
        QModelIndex sourceIndex = proxyModel->mapToSource(index);

        return mTableModel->getGateTypeAtIndex(sourceIndex.row());
    }

    void GateLibraryManager::updateTabs(GateType* gateType)
    {
        mGeneralTab->update(gateType);
        mBooleanFunctionTab->update(gateType);
        mPinTab->update(gateType);
    }

    void GateLibraryManager::resizeEvent(QResizeEvent* evt)
    {
        QFrame::resizeEvent(evt);
        if (evt->size().width() != mFrameWidth)
        {
            mFrameWidth = evt->size().width();
            QList<int> splitSizes;
            // divide available size in percent
            splitSizes << mFrameWidth * 27 / 100 << mFrameWidth * 73 / 100;
            mSplitter->setSizes(splitSizes);
        }
    }
}
