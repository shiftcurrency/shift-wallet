#include "gshiftlog.h"
#include <QDebug>
#include <QSettings>
#include <QApplication>
#include <QClipboard>

namespace ShiftWallet {

    GshiftLog::GshiftLog() :
        QAbstractListModel(0), fList(), fProcess(0)
    {
    }

    QHash<int, QByteArray> GshiftLog::roleNames() const {
        QHash<int, QByteArray> roles;
        roles[MsgRole] = "msg";

        return roles;
    }

    int GshiftLog::rowCount(const QModelIndex & parent __attribute__ ((unused))) const {
        return fList.length();
    }

    QVariant GshiftLog::data(const QModelIndex & index, int role __attribute__ ((unused))) const {
        return fList.at(index.row());
    }

    void GshiftLog::saveToClipboard() const {
        QString text;

        foreach ( const QString& info, fList ) {
            text += (info + QString("\n"));
        }

        QApplication::clipboard()->setText(text);
    }

    void GshiftLog::attach(QProcess* process) {
        fProcess = process;
        connect(fProcess, &QProcess::readyReadStandardOutput, this, &GshiftLog::readStdout);
        connect(fProcess, &QProcess::readyReadStandardError, this, &GshiftLog::readStderr);
    }

    void GshiftLog::append(const QString& line) {
        fList.append(line);
    }

    void GshiftLog::readStdout() {
        const QByteArray ba = fProcess->readAllStandardOutput();
        const QString str = QString::fromUtf8(ba);
        beginInsertRows(QModelIndex(), 0, 0);
        fList.insert(0, str);
        endInsertRows();
        overflowCheck();
    }

    void GshiftLog::readStderr() {
        const QByteArray ba = fProcess->readAllStandardError();
        const QString str = QString::fromUtf8(ba);
        beginInsertRows(QModelIndex(), 0, 0);
        fList.insert(0, str);
        endInsertRows();
        overflowCheck();
    }

    void GshiftLog::overflowCheck() {
        if ( fList.length() > 100 ) {
            beginRemoveRows(QModelIndex(), fList.length() - 1, fList.length() - 1);
            fList.removeAt(fList.length() - 1);
            endRemoveRows();
        }
    }

}
