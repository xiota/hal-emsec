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

#include "hal_core/defines.h"
#include "hal_core/netlist/gate.h"
#include "hal_core/netlist/net.h"
#include "gui/gui_utils/sort.h"
#include "gui/pin_model/pin_model.h"
#include "gui/pin_model/pin_item.h"
#include "gui/gui_globals.h"


#include <QAbstractItemModel>
#include <QModelIndex>
#include <QVariant>
#include <set>
#include <array>

namespace hal
{


    class PinModel : public BaseTreeModel
    {
        Q_OBJECT

    public:

        explicit PinModel(QObject* parent = nullptr);

        /**
         * Returns the item flags for the given index.
         *
         * @param index - The model index to get the flags for
         * @returns the item flags for the given index
         */
        Qt::ItemFlags flags(const QModelIndex& index) const override;

        /**
         * sets the gate from which the pins are to be displayed
         * @param gate - The gate from which the pins are to be displayed
         */
        void setGate(GateType* gate);

        /**
         * should be called when the items name is changed via the delegate
         * @param index index of the PinItem
         * @param input new name
         */
        void handleEditName(QModelIndex index, const QString& input);

        /**
         * should be called when the items direction is changed via the delegate
         * @param index index of the PinItem
         * @param direction new direction
         */
        void handleEditDirection(QModelIndex index, const QString& direction);

        /**
         * should be called when the items type is changed via the delegate
         * @param index index of the PinItem
         * @param type new type
         */
        void handleEditType(QModelIndex index, const QString& type);

        /**
         * Checks whether a Pin or a Group does exist in the Gate
         * @param item the PinItem which should be checked
         * @return true if the pinItem or pinGroup has a corresponding item within the gate - false otherwise
         */
        bool assertionTestForEntry(PinItem* item);


    private:
        QList<PinItem*> mInvalidPins = QList<PinItem*>();
        QList<PinItem*> mInvalidGroups = QList<PinItem*>();

        bool isNameAvailable(const QString& name, PinItem* pinItem) const;
        void handleInvalidPinUpdate(PinItem* pinItem);
        void handleInvalidGroupUpdate(PinItem* groupItem);
        bool renamePin(PinItem* pinItem, const QString& newName) const;
        bool renamePinGroup(PinItem* groupItem, const QString& newName) const;
        void addPinToPinGroup(u32 pinId, u32 groupId);
        void handleGroupDirectionUpdate(PinItem* groupItem ,PinDirection direction = PinDirection::none);

        Result<GatePin*> createPin(PinItem* pinItem, bool addToGroup = false);



        GateType* mGate;

    };
}   // namespace hal
