#include "gui/selection_details_widget/selection_details_icon_provider.h"
#include "gui/gui_utils/graphics.h"
#include "gui/gui_globals.h"
#include "gui/main_window/main_window.h"
#include "gui/settings/settings_items/settings_item_dropdown.h"
#include "gui/module_model/module_model.h"
#include "gui/module_model/module_color_manager.h"
#include <QDebug>
#include <QImage>

namespace hal
{
    SelectionDetailsIconProvider* SelectionDetailsIconProvider::inst = nullptr;

    SettingsItemDropdown* SelectionDetailsIconProvider::sIconSizeSetting = nullptr;

    bool SelectionDetailsIconProvider::sSettingsInitialized = initSettings();

    bool SelectionDetailsIconProvider::initSettings()
    {
        sIconSizeSetting = new SettingsItemDropdown(
            "Right Corner Icon Size",
            "selection_details/icon_size",
            SelectionDetailsIconProvider::IconSize::BigIcon,
            "Appearance:Selection Details",
            "Specifies the size of the icon in the upper right corner of selection details or if the icon is omitted (NoIcon)."
        );
        sIconSizeSetting->setValueNames<IconSize>();
        return true;
    }


    SelectionDetailsIconProvider* SelectionDetailsIconProvider::instance()
    {
        Q_ASSERT(gNetlistRelay); // make sure it does not get called before event relay is installed
        if (!inst) inst = new SelectionDetailsIconProvider();
        return inst;
    }

    SelectionDetailsIconProvider::SelectionDetailsIconProvider(QObject *parent)
        : QObject(parent)
    {
        connect(MainWindow::sSettingStyle, &SettingsItemDropdown::intChanged,this,&SelectionDetailsIconProvider::loadIcons);
        connect(gNetlistRelay->getModuleColorManager(),&ModuleColorManager::moduleColorChanged,this,&SelectionDetailsIconProvider::handleModuleColorChanged);
        loadIcons(MainWindow::sSettingStyle->value().toInt());
    }

    void SelectionDetailsIconProvider::handleModuleColorChanged(u32 id)
    {
        auto it = mModuleIcons.find(id);
        if (it == mModuleIcons.end()) return;
        delete it.value();
        QColor col = gNetlistRelay->getModuleColor(id);
        mModuleIcons[id] = new QIcon(gui_utility::getStyledSvgIcon("all->" + col.name(QColor::HexRgb), ":/icons/ne_module"));
    }

    void SelectionDetailsIconProvider::loadIcons(int istyle)
    {
        MainWindow::StyleSheetOption theme = static_cast<MainWindow::StyleSheetOption>(istyle);
        QString solidColor = (theme == MainWindow::Light) ? "all->#000000" : "all->#ffffff";

        if (!mDefaultIcons.isEmpty())
        {
            for (auto it = mDefaultIcons.begin(); it != mDefaultIcons.end(); ++it)
                delete it.value();
        }

        mDefaultIcons[ModuleIcon] = new QIcon(gui_utility::getStyledSvgIcon(solidColor, ":/icons/ne_module"));
        mDefaultIcons[GateIcon]   = new QIcon(gui_utility::getStyledSvgIcon(solidColor, ":/icons/ne_gate"));
        mDefaultIcons[NetIcon]    = new QIcon(gui_utility::getStyledSvgIcon(solidColor, ":/icons/ne_net"));
        mDefaultIcons[ViewDir]    = new QIcon(gui_utility::getStyledSvgIcon(solidColor, ":/icons/view-dir"));
        mDefaultIcons[ViewCtx]    = new QIcon(gui_utility::getStyledSvgIcon(solidColor, ":/icons/view-ctx"));

        if (!mGateIcons.isEmpty())
        {
            for (auto it = mGateIcons.begin(); it != mGateIcons.end(); ++it)
                delete it.value();
        }

        mGateIcons[(int)GateTypeProperty::c_buffer]   = new QIcon(gui_utility::getStyledSvgIcon(solidColor, ":/icons/ne_gate_buffer"));
        mGateIcons[(int)GateTypeProperty::c_inverter] = new QIcon(gui_utility::getStyledSvgIcon(solidColor, ":/icons/ne_gate_inverter"));
        mGateIcons[(int)GateTypeProperty::c_and]      = new QIcon(gui_utility::getStyledSvgIcon(solidColor, ":/icons/ne_gate_and"));
        mGateIcons[(int)GateTypeProperty::c_nand]     = new QIcon(gui_utility::getStyledSvgIcon(solidColor, ":/icons/ne_gate_nand"));
        mGateIcons[(int)GateTypeProperty::c_or]       = new QIcon(gui_utility::getStyledSvgIcon(solidColor, ":/icons/ne_gate_or"));
        mGateIcons[(int)GateTypeProperty::c_nor]      = new QIcon(gui_utility::getStyledSvgIcon(solidColor, ":/icons/ne_gate_nor"));
        mGateIcons[(int)GateTypeProperty::c_xor]      = new QIcon(gui_utility::getStyledSvgIcon(solidColor, ":/icons/ne_gate_xor"));
        mGateIcons[(int)GateTypeProperty::c_xnor]     = new QIcon(gui_utility::getStyledSvgIcon(solidColor, ":/icons/ne_gate_xnor"));
    }

    const QIcon* SelectionDetailsIconProvider::getIcon(IconCategory catg, u32 itemId)
    {
        Gate* g = nullptr;
        QColor col;

        switch (catg)
        {
        case GateIcon:
            g = gNetlist->get_gate_by_id(itemId);
            if (g)
            {
                std::vector<GateTypeProperty> prop = g->get_type()->get_property_list();
                if (!prop.empty())
                {
                    const QIcon* gateTypeIcon = mGateIcons.value((int)prop.at(0));
                    if (gateTypeIcon) return gateTypeIcon;
                }
            }
            break;
        case ModuleIcon:
            col = gNetlistRelay->getModuleColor(itemId);
            if (col.isValid())
            {
                auto it = mModuleIcons.find(itemId);
                if (it != mModuleIcons.end())
                    return it.value();
                QIcon* newIcon = new QIcon(gui_utility::getStyledSvgIcon("all->" + col.name(QColor::HexRgb), ":/icons/ne_module"));
                mModuleIcons[itemId] = newIcon;
                return newIcon;
            }
            break;
        default:
            break;
        }
        return mDefaultIcons.value(catg);
    }
}
