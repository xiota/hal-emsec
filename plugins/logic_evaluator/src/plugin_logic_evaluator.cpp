#include "logic_evaluator/plugin_logic_evaluator.h"

#include "hal_core/netlist/netlist_writer/netlist_writer_manager.h"
#include "hal_core/netlist/gate.h"
#include "logic_evaluator/logic_evaluator_dialog.h"
#include "gui/content_manager/content_manager.h"
#include "gui/gui_globals.h"
#include "gui/gui_api/gui_api.h"
#include <QDebug>

namespace hal
{
    LogicEvaluatorPlugin::LogicEvaluatorPlugin()
    {
        m_extensions.push_back(new GuiExtensionLogicEvaluator);
    }

    extern std::unique_ptr<BasePluginInterface> create_plugin_instance()
    {
        return std::make_unique<LogicEvaluatorPlugin>();
    }

    std::string LogicEvaluatorPlugin::get_name() const
    {
        return std::string("waveform_viewer");
    }

    std::string LogicEvaluatorPlugin::get_version() const
    {
        return std::string("0.7");
    }

    std::string LogicEvaluatorPlugin::get_description() const
    {
        return std::string("GUI to control simulation and view resulting waveforms");
    }

    std::set<std::string> LogicEvaluatorPlugin::get_dependencies() const
    {
        std::set<std::string> retval;
        retval.insert("netlist_simulator_controller");
        retval.insert("hal_gui");
        return retval;
    }

    void LogicEvaluatorPlugin::on_load()
    {
        qRegisterMetaType<std::string>();
    }

    void LogicEvaluatorPlugin::on_unload()
    {
    }

    //---------------------------------------

    std::vector<PluginParameter> GuiExtensionLogicEvaluator::get_parameter() const
    {
        std::vector<PluginParameter> retval;
        retval.push_back(PluginParameter(PluginParameter::Label, "help", "",
                                         "Press 'Launch' to launch logic evaluator\n"
                                         "with the gates that are currently selected.\n\n"
                                         "Per default boolean logic gets compiled and\n"
                                         "evaluated by compiled binary code. If 'skip' is\n"
                                         "checked the compile step gets omitted and the\n"
                                         "build-in BooleanFunction class does the evaluation."));
        retval.push_back(PluginParameter(PluginParameter::Boolean, "skip", "Skip compilation", "false"));
        retval.push_back(PluginParameter(PluginParameter::PushButton, "exec", "Launch"));
        return retval;
    }

    void GuiExtensionLogicEvaluator::set_parameter(const std::vector<PluginParameter>& params)
    {
        bool launchPressed = false;
        bool skipCompile = false;
        for (const PluginParameter& pp : params)
        {
            if (pp.get_tagname() == "exec" && pp.get_value() == "clicked")
                launchPressed = true;
            if (pp.get_tagname() == "skip")
                skipCompile = (pp.get_value() == "true");
        }

        if (!launchPressed) return;

        std::unordered_set<Gate*> gates;
        for (Gate* g : GuiApi().getSelectedGates())
            if (acceptGate(g))
                gates.insert(g);
        for (Module* m : GuiApi().getSelectedModules())
            for (Gate* g : m->get_gates(nullptr,true))
                if (acceptGate(g))
                    gates.insert(g);

        if (gates.empty())
        {
            log_warning("logic_evaluator", "No pure logical gates in selection, logic evaluator not launched.");
            return;
        }

        std::vector<Gate*> vgates(gates.begin(), gates.end());
        LogicEvaluatorDialog* led = new LogicEvaluatorDialog(vgates, skipCompile);
        led->show();
    }

    bool GuiExtensionLogicEvaluator::acceptGate(const Gate *g)
    {
        const GateType* gt = g->get_type();
        if (gt->has_property(GateTypeProperty::ff)) return false;
        if (gt->has_property(GateTypeProperty::latch)) return false;
        return true;
    }

}    // namespace hal
