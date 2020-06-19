//  MIT License
//
//  Copyright (c) 2019 Ruhr-University Bochum, Germany, Chair for Embedded Security. All Rights reserved.
//  Copyright (c) 2019 Marc Fyrbiak, Sebastian Wallat, Max Hoffmann ("ORIGINAL AUTHORS"). All rights reserved.
//
//  Permission is hereby granted, free of charge, to any person obtaining a copy
//  of this software and associated documentation files (the "Software"), to deal
//  in the Software without restriction, including without limitation the rights
//  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
//  copies of the Software, and to permit persons to whom the Software is
//  furnished to do so, subject to the following conditions:
//
//  The above copyright notice and this permission notice shall be included in all
//  copies or substantial portions of the Software.
//
//  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
//  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
//  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
//  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
//  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
//  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
//  SOFTWARE.

#pragma once

#include "netlist/gate.h"
#include "netlist/net.h"
#include "netlist/module.h"


#include <vector>
#include <tuple>

#include <QObject>

namespace hal
{
    class gui_api : public QObject
    {
        Q_OBJECT

    public:
        gui_api();

        std::vector<u32> get_selected_gate_ids();
        std::vector<u32> get_selected_net_ids();
        std::vector<u32> get_selected_module_ids();
        std::tuple<std::vector<u32>, std::vector<u32>, std::vector<u32>> get_selected_item_ids();

        std::vector<std::shared_ptr<Gate>> get_selected_gates();
        std::vector<std::shared_ptr<Net>> get_selected_nets();
        std::vector<std::shared_ptr<Module>> get_selected_modules();
        std::tuple<std::vector<std::shared_ptr<Gate>>, std::vector<std::shared_ptr<Net>>, std::vector<std::shared_ptr<Module>>> get_selected_items();

        void select_gate(u32 gate_id, bool clear_current_selection = true, bool navigate_to_selection = true);
        void select_gate(const std::vector<u32>& gate_ids, bool clear_current_selection = true, bool navigate_to_selection = true);
        void select_gate(const std::shared_ptr<Gate>& gate, bool clear_current_selection = true, bool navigate_to_selection = true);
        void select_gate(const std::vector<std::shared_ptr<Gate>>& gates, bool clear_current_selection = true, bool navigate_to_selection = true);

        void select_net(u32 net_id, bool clear_current_selection = true, bool navigate_to_selection = true);
        void select_net(const std::vector<u32>& net_ids, bool clear_current_selection = true, bool navigate_to_selection = true);
        void select_net(const std::shared_ptr<Net>& net, bool clear_current_selection = true, bool navigate_to_selection = true);
        void select_net(const std::vector<std::shared_ptr<Net>>& nets, bool clear_current_selection = true, bool navigate_to_selection = true);

        void select_module(u32 module_id, bool clear_current_selection = true, bool navigate_to_selection = true);
        void select_module(const std::vector<u32>& module_ids, bool clear_current_selection = true, bool navigate_to_selection = true);
        void select_module(const std::shared_ptr<Module>& module, bool clear_current_selection = true, bool navigate_to_selection = true);
        void select_module(const std::vector<std::shared_ptr<Module>>& modules, bool clear_current_selection = true, bool navigate_to_selection = true);

        void select(const std::shared_ptr<Gate>& gate, bool clear_current_selection = true, bool navigate_to_selection = true);
        void select(const std::shared_ptr<Net>& net, bool clear_current_selection = true, bool navigate_to_selection = true);
        void select(const std::shared_ptr<Module>& module, bool clear_current_selection = true, bool navigate_to_selection = true);
        void select(const std::vector<std::shared_ptr<Gate>>& gates, bool clear_current_selection = true, bool navigate_to_selection = true);
        void select(const std::vector<std::shared_ptr<Net>>& nets, bool clear_current_selection = true, bool navigate_to_selection = true);
        void select(const std::vector<std::shared_ptr<Module>>& modules, bool clear_current_selection = true, bool navigate_to_selection = true);
        void select(const std::vector<u32>& gate_ids, const std::vector<u32>& net_ids, const std::vector<u32>& module_ids, bool clear_current_selection = true, bool navigate_to_selection = true);
        void select(const std::vector<std::shared_ptr<Gate>>& gates, const std::vector<std::shared_ptr<Net>>& nets, const std::vector<std::shared_ptr<Module>>& modules, bool clear_current_selection = true, bool navigate_to_selection = true);

        void deselect_gate(u32 gate_id);
        void deselect_gate(const std::vector<u32>& gate_ids);
        void deselect_gate(const std::shared_ptr<Gate>& gate);
        void deselect_gate(const std::vector<std::shared_ptr<Gate>>& gates);

        void deselect_net(u32 net_id);
        void deselect_net(const std::vector<u32>& net_ids);
        void deselect_net(const std::shared_ptr<Net>& net);
        void deselect_net(const std::vector<std::shared_ptr<Net>>& nets);

        void deselect_module(u32 module_id);
        void deselect_module(const std::vector<u32>& module_ids);
        void deselect_module(const std::shared_ptr<Module>& module);
        void deselect_module(const std::vector<std::shared_ptr<Module>>& modules);

        void deselect_all_items();
        void deselect(const std::shared_ptr<Gate>& gate);
        void deselect(const std::shared_ptr<Net>& net);
        void deselect(const std::shared_ptr<Module>& module);
        void deselect(const std::vector<std::shared_ptr<Gate>>& gates);
        void deselect(const std::vector<std::shared_ptr<Net>>& nets);
        void deselect(const std::vector<std::shared_ptr<Module>>& modules);
        void deselect(const std::vector<u32>& gate_ids, const std::vector<u32>& net_ids, const std::vector<u32>& module_ids);
        void deselect(const std::vector<std::shared_ptr<Gate>>& gates, const std::vector<std::shared_ptr<Net>>& nets, const std::vector<std::shared_ptr<Module>>& modules);

    Q_SIGNALS:
        void navigation_requested();
    };
}
