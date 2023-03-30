#include "waveform_viewer/wizard.h"
#include "waveform_viewer/wave_widget.h")
#include "gui/module_dialog/gate_select_model.h"
#include "netlist_simulator_controller/plugin_netlist_simulator_controller.h"
#include "hal_core/plugin_system/plugin_manager.h"

#include <QHeaderView>
#include <QFileDialog>

#include "gui/gui_globals.h"

namespace hal {

    Wizard::Wizard(SimulationSettings *settings, WaveformViewer *parent)
        : QWizard(parent), mSettings(settings), m_parent(parent)
    {
        setWindowTitle(tr("Empty Wizard"));

        addPage(createIntroPage());
        addPage(createPage1());
        addPage(createPage2());
        addPage(createPage3());
        mPage4Id = addPage(createPage4());
        mPage5Id = addPage(createPage5());
        addPage(createConclusionPage());
    }

    QWizardPage *Wizard::createIntroPage()
    {
        QWizardPage *page = new IntroPage;
        page->setTitle(tr("Introduction"));
        page->setSubTitle(tr("Introduction about Wizard"));
        return page;
    }

    QWizardPage *Wizard::createPage1()
    {
        QWizardPage *page = new Page1(m_parent);
        page->setTitle(tr("Step 1"));
        page->setSubTitle(tr("Select Gates"));
        return page;
    }

    QWizardPage *Wizard::createPage2()
    {
        QWizardPage *page = new Page2(m_parent);
        page->setTitle(tr("Step 2"));
        page->setSubTitle(tr("Clock settings"));
        return page;
    }

    QWizardPage *Wizard::createPage3()
    {
        QWizardPage *page = new Page3(m_parent, this);
        page->setTitle(tr("Step 3"));
        page->setSubTitle(tr("Engine settings"));
        return page;
    }

    QWizardPage *Wizard::createPage4()
    {
        QWizardPage *page = new Page4(mSettings, m_parent);
        page->setTitle(tr("Step 4"));
        page->setSubTitle(tr("Engine properties"));
        return page;
    }

    QWizardPage *Wizard::createPage5()
    {
        QWizardPage *page = new Page5(m_parent);
        page->setTitle(tr("Step 5"));
        page->setSubTitle(tr("Load input Data"));
        return page;
    }

    QWizardPage *Wizard::createConclusionPage()
    {
        QWizardPage *page = new ConclusionPage;
        page->setTitle(tr("End"));
        page->setSubTitle(tr("Run Simulation"));
        return page;
    }

    IntroPage::IntroPage(QWidget *parent) : QWizardPage(parent)
    {
        label = new QLabel(tr("What is the Waveform Simulation"));
        QVBoxLayout *layout = new QVBoxLayout;
        layout->addWidget(label);
        setLayout(layout);
    }

    Page1::Page1(WaveformViewer *parent)
      : QWizardPage(parent), m_parent(parent)
    {
        QGridLayout* layout = new QGridLayout(this);
        mButAll = new QPushButton("All gates", this);
        connect(mButAll,&QPushButton::clicked,this,&Page1::handleSelectAll);
        layout->addWidget(mButAll,0,0);
        mButSel = new QPushButton("Current GUI selection", this);
        connect(mButSel,&QPushButton::clicked,this,&Page1::handleCurrentGuiSelection);
        layout->addWidget(mButSel,0,1);
        mButNone = new QPushButton("Clear selection", this);
        connect(mButNone,&QPushButton::clicked,this,&Page1::handleClearSelection);
        layout->addWidget(mButNone,0,2);
        mTableView = new QTableView(this);

        mTableView->setSelectionBehavior(QAbstractItemView::SelectRows);
        mTableView->setSelectionMode(QAbstractItemView::MultiSelection);

        GateSelectProxy* prox = new GateSelectProxy(this);
        // connect(sbar, &Searchbar::textEdited, prox, &GateSelectProxy::searchTextChanged);

        GateSelectModel* modl = new GateSelectModel(false,QSet<u32>(),mTableView);
        prox->setSourceModel(modl);
        mTableView->setModel(prox);

        // connect(selectionModel(), &QItemSelectionModel::currentChanged, this, &GateSelectView::handleCurrentChanged);
        mTableView->setSortingEnabled(true);
        mTableView->sortByColumn(2, Qt::AscendingOrder);
        mTableView->resizeColumnsToContents();
        mTableView->horizontalHeader()->setStretchLastSection(true);
        mTableView->verticalHeader()->hide();
        layout->addWidget(mTableView,1,0,1,3);
        //mButtonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
        //connect(mButtonBox, &QDialogButtonBox::accepted, static_cast<QDialog*>(this), &QDialog::accept);
        //connect(mButtonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
        //layout->addWidget(mButtonBox,2,1,1,2);

        // vllt hinzufügen dass Next Button disabled ist wenn noch nix ausgewählt ist
    }

    void Page1::handleSelectAll()
    {
        mTableView->selectAll();
    }

    void Page1::handleCurrentGuiSelection()
    {
        QSet<u32> guiGateSel = gSelectionRelay->selectedGates();

        const QAbstractItemModel* modl = mTableView->model(); // proxy model
        int nrows = modl->rowCount();
        mTableView->clearSelection();

        bool ok;

        for (int irow = 0; irow<nrows; irow++)
        {
            u32 gid = modl->data(modl->index(irow,0)).toUInt(&ok);
            if (!ok) continue;
            if (guiGateSel.contains(gid))
                mTableView->selectRow(irow);
        }
    }

    void Page1::handleClearSelection()
    {
        mTableView->clearSelection();
    }

    std::vector<hal::Gate*> Page1::selectedGates() const
    {
        std::vector<Gate*> retval;
        QItemSelectionModel *sm = mTableView->selectionModel();
        if (!sm->hasSelection()) return retval;
        QSet<u32> selGates;
        bool ok;
        for (const QModelIndex& inx : sm->selectedRows(0) )
        {
            u32 gid = mTableView->model()->data(inx).toUInt(&ok);
            if (!ok) continue;
            selGates.insert(gid);
        }

        for (u32 gid: selGates)
        {
            Gate* g = gNetlist->get_gate_by_id(gid);
            if (g) retval.push_back(g);
        }
        return retval;
    }

    bool Page1::validatePage()
    {
        m_parent->setGates(selectedGates());

        // wurde gate ausgewählt???
        return true;
    }


    Page2::Page2(WaveformViewer *parent)
        : QWizardPage(parent), m_parent(parent)
    {

        for (const Net* n : m_parent->mCurrentWaveWidget->controller()->get_input_nets())
            mInputs.append(n);

        // was wenn inputs leer?

        QGridLayout* layout = new QGridLayout(this);
        mComboNet = new QComboBox(this);
        int j = 0;
        int iclk = -1;

        for (const Net* n : mInputs)
        {
            QString netName = QString::fromStdString(n->get_name());
            QString upcase = netName.toUpper();
            if (upcase == "CLK" || upcase == "CLOCK")
                iclk = j;
            else if ((upcase.contains("CLK") || upcase.contains("CLOCK")) && j<0 )
                iclk = j;
            mComboNet->insertItem(j++,QString("%1[%2]").arg(netName).arg(n->get_id()));
        }
        if (iclk >= 0) mComboNet->setCurrentIndex(iclk);
        layout->addWidget(new QLabel("Select clock net:",this),0,0);
        layout->addWidget(mComboNet,0,1);

        layout->addWidget(new QLabel("Clock period:",this),1,0);
        mSpinPeriod = new QSpinBox(this);
        mSpinPeriod->setMinimum(0);
        mSpinPeriod->setMaximum(1000000);
        mSpinPeriod->setValue(10);
        layout->addWidget(mSpinPeriod,1,1);

        layout->addWidget(new QLabel("Start value:",this),2,0);
        mSpinStartValue = new QSpinBox(this);
        mSpinStartValue->setMinimum(0);
        mSpinStartValue->setMaximum(1);
        layout->addWidget(mSpinStartValue,2,1);

        layout->addWidget(new QLabel("Duration:",this),3,0);
        mSpinDuration = new QSpinBox(this);
        mSpinDuration->setMinimum(0);
        mSpinDuration->setMaximum(1000000);
        mSpinDuration->setValue(2000);
        layout->addWidget(mSpinDuration,3,1);

        mDontUseClock = new QCheckBox("Do not use clock in simulation",this);
        mDontUseClock->setCheckState(Qt::Unchecked);
        connect(mDontUseClock,&QCheckBox::stateChanged,this,&Page2::dontUseClockChanged);
        layout->addWidget(mDontUseClock,4,0,1,2);

        //mButtonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
        //connect(mButtonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
        //connect(mButtonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
        //layout->addWidget(mButtonBox,5,1);
    }

    void Page2::dontUseClockChanged(bool state)
    {
        mComboNet->setDisabled(state);
        mSpinPeriod->setDisabled(state);
        mSpinStartValue->setDisabled(state);
        mSpinDuration->setDisabled(state);
    }

    bool Page2::validatePage()
    {

        if (mDontUseClock->isChecked())
        {
            m_parent->mCurrentWaveWidget->controller()->set_no_clock_used();
        }
        else
        {
            int period = mSpinPeriod->value();
            if (period <= 0) return false;

            const Net* clk = mInputs.at(mComboNet->currentIndex());
            m_parent->mCurrentWaveWidget->controller()->add_clock_period(
                clk, period, mSpinStartValue->value()==0, mSpinDuration->value()
            );
        }

        // wurde clock ausgewählt???
        return true;
    }


    Page3::Page3(WaveformViewer *parent, Wizard *wizard)
        : QWizardPage(parent), m_parent(parent), m_wizard(wizard)
    {
        mLayout = new QVBoxLayout(this);

        for (SimulationEngineFactory* sef : *SimulationEngineFactories::instance())
        {
            QRadioButton *radioButton = new QRadioButton(QString::fromStdString(sef->name()), this);
            mLayout->addWidget(radioButton);

            if (QString::fromStdString(sef->name()) == "verilator")
            {
                radioButton->setChecked(true);
            }
        }

        setLayout(mLayout);
    }

    bool Page3::validatePage()
    {

        QString selectedEngineName;

        for (int i = 0; i < mLayout->count(); ++i)
        {
            QRadioButton *radioButton = qobject_cast<QRadioButton *>(mLayout->itemAt(i)->widget());
            if (radioButton && radioButton->isChecked())
            {
                selectedEngineName = radioButton->text();
                break;
            }
        }
        if (selectedEngineName.toStdString() == "verilator")
        {
            mVerilator = true;
        }
        else
        {
            mVerilator = false;
        }

        m_parent->mCurrentWaveWidget->createEngine(selectedEngineName);
        std::cout << selectedEngineName.toStdString() << std::endl;


        // wurde clock ausgewählt???
        return true;
    }

    int Page3::nextId() const
    {
        if (mVerilator)
        {
            return m_wizard->mPage4Id;
        }
        else
        {
            return m_wizard->mPage5Id;
        }
    }



    Page4::Page4(SimulationSettings *settings, WaveformViewer *parent)
        : QWizardPage(parent), mSettings(settings), m_parent(parent)
    {
        mTableWidget = new QTableWidget(this);

        QMap<QString,QString> engProp = settings->engineProperties();
        mTableWidget->setColumnCount(2);
        mTableWidget->setColumnWidth(0,250);
        mTableWidget->setColumnWidth(1,350);
        mTableWidget->setRowCount(engProp.size()+3);
        mTableWidget->setHorizontalHeaderLabels(QStringList() << "Property" << "Value");

        int irow = 0;
        for (auto it = engProp.constBegin(); it != engProp.constEnd(); ++it)
        {
            QTableWidgetItem wi(it.key());
            mTableWidget->setItem(irow, 0, new QTableWidgetItem(it.key()));
            mTableWidget->setItem(irow, 1, new QTableWidgetItem(it.value()));
            ++irow;
        }
        mTableWidget->horizontalHeader()->setStretchLastSection(true);

        mTableWidget->horizontalHeader()->setStretchLastSection(true);
        connect(mTableWidget, &QTableWidget::cellChanged, this, &Page4::handleCellChanged);


        QVBoxLayout *layout = new QVBoxLayout;
        layout->addWidget(mTableWidget);
        setLayout(layout);

    }

    void Page4::handleCellChanged(int irow, int icolumn)
    {
        if ((icolumn == 1 && irow >= mTableWidget->rowCount()-2) ||
            (icolumn == 0 && irow >= mTableWidget->rowCount()-1))
            mTableWidget->setRowCount(mTableWidget->rowCount()+1);
    }

    bool Page4::validatePage()
    {

        QMap<QString,QString> engProp;
        for (int irow=0; irow < mTableWidget->rowCount(); ++irow)
        {
            const QTableWidgetItem* wi = mTableWidget->item(irow,0);
            if (!wi) continue;
            QString key = wi->text().trimmed();
            if (key.isEmpty()) continue;
            wi = mTableWidget->item(irow,1);
            QString value = wi ? wi->text().trimmed() : QString();
            engProp[key] = value;
        }
        mSettings->setEngineProperties(engProp);
        mSettings->sync();

        return true;
    }


    Page5::Page5(WaveformViewer *parent)
        : QWizardPage(parent), m_parent(parent)
    {
        QString filter = QString("Saved data (%1)").arg(NetlistSimulatorController::sPersistFile);
        if (m_parent->mCurrentWaveWidget)
            filter += ";; VCD files (*.vcd);; CSV files (*.csv)";

        QString filename =
                QFileDialog::getOpenFileName(this, "Load input wave file", ".", filter);
        if (filename.isEmpty()) return;
        if (filename.endsWith(NetlistSimulatorController::sPersistFile))
        {
            NetlistSimulatorControllerPlugin* ctrlPlug = static_cast<NetlistSimulatorControllerPlugin*>(plugin_manager::get_plugin_instance("netlist_simulator_controller"));
            if (ctrlPlug)
            {
                std::unique_ptr<NetlistSimulatorController> ctrlRef = ctrlPlug->restore_simulator_controller(gNetlist,filename.toStdString());
                //takeControllerOwnership(ctrlRef, true);
            }
        }
        else if (m_parent->mCurrentWaveWidget && m_parent->mCurrentWaveWidget->controller()->can_import_data() && filename.toLower().endsWith(".vcd"))
            m_parent->mCurrentWaveWidget->controller()->import_vcd(filename.toStdString(),NetlistSimulatorController::GlobalInputs);
        else if (m_parent->mCurrentWaveWidget && m_parent->mCurrentWaveWidget->controller()->can_import_data() && filename.toLower().endsWith(".csv"))
            m_parent->mCurrentWaveWidget->controller()->import_csv(filename.toStdString(),NetlistSimulatorController::GlobalInputs);
        else if (m_parent->mCurrentWaveWidget)
            log_warning(m_parent->mCurrentWaveWidget->controller()->get_name(), "Cannot parse file '{}' (unknown extension ore wrong state).", filename.toStdString());
        else
            log_warning("simulation_plugin", "Unable to restore saved data from file '{}'.", filename.toStdString());
    }

    ConclusionPage::ConclusionPage(QWidget *parent): QWizardPage(parent)
    {
        label = new QLabel(tr("Run Simulation"));
        QVBoxLayout *layout = new QVBoxLayout;
        layout->addWidget(label);
        setLayout(layout);
    }

}
