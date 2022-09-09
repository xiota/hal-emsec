//  MIT License
//
//  Copyright (c) 2019 Ruhr University Bochum, Chair for Embedded Security. All Rights reserved.
//  Copyright (c) 2021 Max Planck Institute for Security and Privacy. All Rights reserved.
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
#include <QThread>
#include <QString>
#include <QMutex>
#include "gui/python/python_context.h"

namespace hal {
    class PythonThread : public QThread, public PythonContextSubscriber
    {
        Q_OBJECT
        QString mScript;
        QString mErrorMessage;
        unsigned long mThreadID;
        QString mInputString;
        QMutex mInputMutex;
    Q_SIGNALS:
        void stdOutput(QString txt);
        void stdError(QString txt);
        void requireInput(QString prompt);
    public:
        PythonThread(const QString& script, QObject* parent = nullptr);
        void run() override;
        void interrupt();
        QString errorMessage() const { return mErrorMessage; }
        void handleStdout(const QString& output) override;
        void handleError(const QString& output) override;
        std::string handleInput(const QString& prompt);
        void clear() override;
        void setInput(const QString& inp);
     };
}
