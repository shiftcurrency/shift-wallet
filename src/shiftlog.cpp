#include "shiftlog.h"
#include <QDebug>
#include <QSettings>
#include <QApplication>
#include <QClipboard>

namespace Etherwall {

    static ShiftLog* sLog = NULL;

    ShiftLog::ShiftLog() :
        QAbstractListModel(0), fList()
    {
        sLog = this;
        const QSettings settings;
        fLogLevel = (LogSeverity)settings.value("log/level", LS_Info).toInt();
    }

    QHash<int, QByteArray> ShiftLog::roleNames() const {
        QHash<int, QByteArray> roles;
        roles[MsgRole] = "msg";
        roles[DateRole] = "date";
        roles[SeverityRole] = "severity";

        return roles;
    }

    int ShiftLog::rowCount(const QModelIndex & parent __attribute__ ((unused))) const {
        return fList.length();
    }

    QVariant ShiftLog::data(const QModelIndex & index, int role) const {
        return fList.at(index.row()).value(role);
    }

    void ShiftLog::saveToClipboard() const {
        QString text;

        foreach ( const LogInfo info, fList ) {
            text += (info.value(MsgRole).toString() + QString("\n"));
        }

        QApplication::clipboard()->setText(text);
    }

    void ShiftLog::logMsg(const QString &msg, LogSeverity sev) {
        sLog->log(msg, sev);
    }

    void ShiftLog::log(QString msg, LogSeverity sev) {
        if ( sev < fLogLevel ) {
            return; // skip due to severity setting
        }

        beginInsertRows(QModelIndex(), 0, 0);
        fList.insert(0, LogInfo(msg, sev));
        endInsertRows();
    }

    int ShiftLog::getLogLevel() const {
        return fLogLevel;
    }

    void ShiftLog::setLogLevel(int ll) {
        fLogLevel = (LogSeverity)ll;
        QSettings settings;
        settings.setValue("log/level", ll);
    }

}
