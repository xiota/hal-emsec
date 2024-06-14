#include "gui/gatelibrary_management/gatelibrary_pages/bool_wizardpage.h"
#include "gui/gatelibrary_management/gatelibrary_wizard.h"
#include "gui/pin_model/pin_item.h"

namespace hal
{
    const char* BooleanFunctionEdit::STATE_EMPTY = "Empty";
    const char* BooleanFunctionEdit::STATE_VALID = "Valid";
    const char* BooleanFunctionEdit::STATE_INVALID = "Invalid";

    BooleanFunctionEdit::BooleanFunctionEdit(std::set<std::string> &legalVar, QWidget *parent)
        : QLineEdit(parent), mState(STATE_VALID), mLegalVariables(legalVar)
    {
        connect(this, &QLineEdit::textChanged, this, &BooleanFunctionEdit::handleEditingFinished);

        setState(STATE_EMPTY); // do an active transition to enforce style
    }

    void BooleanFunctionEdit::setState(const QString &s)
    {
        if (s == mState) return;
        mState = s;
        Q_EMIT stateChanged(s);
        QStyle* sty = style();

        sty->unpolish(this);
        sty->polish(this);
    }

    void BooleanFunctionEdit::handleEditingFinished()
    {
        if (text().isEmpty())
        {
            setState(STATE_EMPTY);
            return;
        }

        QString nextState = STATE_VALID;  // think positive

        auto bfres = BooleanFunction::from_string(text().toStdString());
        if(bfres.is_error())
            nextState = STATE_INVALID;
        else
        {
            BooleanFunction bf = bfres.get();
            std::set<std::string> var_names = bf.get_variable_names();
            std::set<std::string> leg = mLegalVariables;
            for(std::string vname : var_names)
            {
                if (mLegalVariables.find(vname) == mLegalVariables.end())
                {
                    nextState = STATE_INVALID;
                    break;
                }
            }
        }
        if (mState != nextState)
            setState(nextState);
    }
//--------------------------------------------
    BoolWizardPage::BoolWizardPage(QWidget* parent) : QWizardPage(parent)
    {
        setTitle("Boolean functions");
        setSubTitle("Enter the boolean functions");
        mLayout = new QGridLayout(this);
    }

    void BoolWizardPage::initializePage(){
        mWizard = static_cast<GateLibraryWizard*>(wizard());
        QList<PinItem*> pinGroups = mWizard->getPingroups();

        QList<PinItem*> inputPins = mWizard->mPinModel->getInputPins();
        std::set<std::string> input_pins;
        for (PinItem* pi : inputPins)
            input_pins.insert(pi->getName().toStdString());

        if(mGate != nullptr){
            auto boolFunctions = mGate->get_boolean_functions();
            auto list = QList<QPair<QString, BooleanFunction>>();
            int boolFuncCnt = 0;

            for(std::pair<const std::basic_string<char>, BooleanFunction> bf : boolFunctions){
                QLabel* label = new QLabel(QString::fromStdString(bf.first));
                BooleanFunctionEdit* lineEdit = new BooleanFunctionEdit(input_pins, this);
                mLayout->addWidget(label, boolFuncCnt, 0);
                mLayout->addWidget(lineEdit, boolFuncCnt, 1);
                lineEdit->setText(QString::fromStdString(bf.second.to_string()));
                boolFuncCnt++;
            }
        }
        else{
            if(!pinGroups.empty())
            {
                int rowCount = 0;
                for(PinItem* pinGroup : pinGroups){
                    if(pinGroup->getItemType() != PinItem::TreeItemType::GroupCreator && pinGroup->getDirection() == PinDirection::output){
                        for(auto item : pinGroup->getChildren())
                        {
                            PinItem* pin = static_cast<PinItem*>(item);
                            if(pin->getItemType() != PinItem::TreeItemType::PinCreator){
                                QLabel* label = new QLabel(pin->getName());
                                QString name = label->text();
                                BooleanFunctionEdit* lineEdit = new BooleanFunctionEdit(input_pins, this);
                                mLayout->addWidget(label, rowCount, 0);
                                mLayout->addWidget(lineEdit, rowCount, 1);
                                rowCount++;
                            }
                        }
                    }
                }
            }
        }

        setLayout(mLayout);
    }

    void BoolWizardPage::setData(GateType *gate){
        mGate = gate;
    }

    bool BoolWizardPage::isComplete() const{

        for(int i = 0; i<mLayout->rowCount(); i++)
        {
            BooleanFunctionEdit* lineEdit = static_cast<BooleanFunctionEdit*>(mLayout->itemAtPosition(i, 1)->widget());
            connect(lineEdit, &BooleanFunctionEdit::editingFinished, this, &BoolWizardPage::completeChanged);
            if(!lineEdit->isValid())
            {
                return false;
            }
        }
        return true;
    }

}
