    /*
    This file is part of SHIFT Wallet based on etherwall.
    SHIFT Wallet based on etherwall is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.
    SHIFT Wallet based on etherwall is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.
    You should have received a copy of the GNU General Public License
    along with SHIFT Wallet based on etherwall. If not, see <http://www.gnu.org/licenses/>.
*/
/** @file shiftipc.cpp
 * @author Ales Katona <almindor@gmail.com>
 * @date 2015
 *
 * SHIFT IPC client implementation
 */

#include "shiftipc.h"
#include <QSettings>

namespace Etherwall {

// *************************** RequestIPC **************************** //
    int RequestIPC::sCallID = 0;

    RequestIPC::RequestIPC(RequestBurden burden, RequestTypes type, const QString method, const QJsonArray params, int index) :
        fCallID(sCallID++), fType(type), fMethod(method), fParams(params), fIndex(index), fBurden(burden)
    {
    }

    RequestIPC::RequestIPC(RequestTypes type, const QString method, const QJsonArray params, int index) :
        fCallID(sCallID++), fType(type), fMethod(method), fParams(params), fIndex(index), fBurden(Full)
    {
    }

    RequestIPC::RequestIPC(RequestBurden burden) : fBurden(burden)
    {
    }

    RequestTypes RequestIPC::getType() const {
        return fType;
    }

    const QString& RequestIPC::getMethod() const {
        return fMethod;
    }

    const QJsonArray& RequestIPC::getParams() const {
        return fParams;
    }

    int RequestIPC::getIndex() const {
        return fIndex;
    }

    int RequestIPC::getCallID() const {
        return fCallID;
    }

    RequestBurden RequestIPC::burden() const {
        return fBurden;
    }

// *************************** ShiftIPC **************************** //

    ShiftIPC::ShiftIPC() : fFilterID(-1), fClosingApp(false), fAborted(false), fPeerCount(0), fActiveRequest(None)
    {
        connect(&fSocket, (void (QLocalSocket::*)(QLocalSocket::LocalSocketError))&QLocalSocket::error, this, &ShiftIPC::onSocketError);
        connect(&fSocket, &QLocalSocket::readyRead, this, &ShiftIPC::onSocketReadyRead);
        connect(&fSocket, &QLocalSocket::connected, this, &ShiftIPC::connectedToServer);
        connect(&fSocket, &QLocalSocket::disconnected, this, &ShiftIPC::disconnectedFromServer);

        const QSettings settings;

        fTimer.setInterval(settings.value("ipc/interval", 10).toInt() * 1000);
        connect(&fTimer, &QTimer::timeout, this, &ShiftIPC::onTimer);
    }

    bool ShiftIPC::getBusy() const {
        return (fActiveRequest.burden() != None);
    }

    const QString& ShiftIPC::getError() const {
        return fError;
    }

    int ShiftIPC::getCode() const {
        return fCode;
    }

    void ShiftIPC::setInterval(int interval) {
        fTimer.setInterval(interval);
    }

    bool ShiftIPC::closeApp() {
        fClosingApp = true;
        fTimer.stop();

        if ( fSocket.state() == QLocalSocket::UnconnectedState ) {
            return true;
        }

        if ( fSocket.state() == QLocalSocket::ConnectedState && getBusy() ) { // wait for operation first if we're still connected
            return false;
        }

        if ( fSocket.state() == QLocalSocket::ConnectedState && fFilterID >= 0 ) { // remove filter if still connected
            uninstallFilter();
            return false;
        }

        if ( fSocket.state() != QLocalSocket::UnconnectedState ) { // wait for clean disconnect
            fActiveRequest = RequestIPC(Full);
            fSocket.disconnectFromServer();
            return false;
        }

        return true;
    }

    void ShiftIPC::connectToServer(const QString& path) {
        if ( fAborted ) {
            bail();
            return;
        }

        fActiveRequest = RequestIPC(Full);
        emit busyChanged(getBusy());
        fPath = path;
        if ( fSocket.state() != QLocalSocket::UnconnectedState ) {
            setError("Already connected");
            return bail();
        }

        fSocket.connectToServer(path);
        ShiftLog::logMsg("Connecting to IPC socket");

        QTimer::singleShot(2000, this, SLOT(connectionTimeout()));
    }

    void ShiftIPC::connectedToServer() {
        done();

        getClientVersion();
        getBlockNumber(); // initial
        fTimer.start(); // should happen after filter creation, might need to move into last filter response handler

        ShiftLog::logMsg("Connected to IPC socket");
        emit connectToServerDone();
        emit connectionStateChanged();
    }

    void ShiftIPC::connectionTimeout() {
        if ( !fAborted && fSocket.state() != QLocalSocket::ConnectedState ) {
            fSocket.abort();
            setError("Unable to establish IPC connection to gshift. Make sure gshift is running and try again.");
            bail();
        }
    }

    void ShiftIPC::disconnectedFromServer() {
        if ( fClosingApp || fAborted ) { // expected
            return;
        }

        fError = fSocket.errorString();
        bail();
    }

    void ShiftIPC::getAccounts() {
        fAccountList.clear();
        if ( !queueRequest(RequestIPC(GetAccountRefs, "personal_listAccounts", QJsonArray())) ) {
            return bail();
        }
    }

    void ShiftIPC::handleGetAccounts() {
        QJsonValue jv;
        if ( !readReply(jv) ) {
            return bail();
        }

        QJsonArray refs = jv.toArray();
        foreach( QJsonValue r, refs ) {
            const QString hash = r.toString("INVALID");
            fAccountList.append(AccountInfo(hash, QString(), 0));
        }

        emit getAccountsDone(fAccountList);

        // TODO: figure out a way to get account transaction history
        newFilter();

        done();
    }

    bool ShiftIPC::refreshAccount(const QString& hash, int index) {
        if ( getBalance(hash, index) ) {
            return getTransactionCount(hash, index);
        }

        return false;
    }

    bool ShiftIPC::getBalance(const QString& hash, int index) {
        QJsonArray params;
        params.append(hash);
        params.append(QString("latest"));
        if ( !queueRequest(RequestIPC(GetBalance, "shf_getBalance", params, index)) ) {
            bail();
            return false;
        }

        return true;
    }

    bool ShiftIPC::getTransactionCount(const QString& hash, int index) {
        QJsonArray params;
        params.append(hash);
        params.append(QString("latest"));
        if ( !queueRequest(RequestIPC(GetTransactionCount, "shf_getTransactionCount", params, index)) ) {
            bail();
            return false;
        }

        return true;
    }


    void ShiftIPC::handleAccountBalance() {
        QJsonValue jv;
        if ( !readReply(jv) ) {
            return bail();
        }

        const QString decStr = Helpers::toDecStrShift(jv);
        const int index = fActiveRequest.getIndex();
        fAccountList[index].setBalance(decStr);

        emit accountChanged(fAccountList.at(index));
        done();
    }

    void ShiftIPC::handleAccountTransactionCount() {
        QJsonValue jv;
        if ( !readReply(jv) ) {
            return bail();
        }

        std::string hexStr = jv.toString("0x0").remove(0, 2).toStdString();
        const BigInt::Vin bv(hexStr, 16);
        quint64 count = bv.toUlong();
        const int index = fActiveRequest.getIndex();
        fAccountList[index].setTransactionCount(count);

        emit accountChanged(fAccountList.at(index));
        done();
    }

    void ShiftIPC::newAccount(const QString& password, int index) {
        QJsonArray params;
        params.append(password);
        if ( !queueRequest(RequestIPC(NewAccount, "personal_newAccount", params, index)) ) {
            return bail();
        }
    }

    void ShiftIPC::handleNewAccount() {
        QJsonValue jv;
        if ( !readReply(jv) ) {
            return bail();
        }

        const QString result = jv.toString();
        emit newAccountDone(result, fActiveRequest.getIndex());
        done();
    }

    void ShiftIPC::deleteAccount(const QString& hash, const QString& password, int index) {
        QJsonArray params;
        params.append(hash);
        params.append(password);        
        if ( !queueRequest(RequestIPC(DeleteAccount, "personal_deleteAccount", params, index)) ) {
            return bail();
        }
    }

    void ShiftIPC::handleDeleteAccount() {
        QJsonValue jv;
        if ( !readReply(jv) ) {
            return bail();
        }

        const bool result = jv.toBool(false);
        emit deleteAccountDone(result, fActiveRequest.getIndex());
        done();
    }

    void ShiftIPC::getBlockNumber() {
        if ( !queueRequest(RequestIPC(NonVisual, GetBlockNumber, "shf_blockNumber")) ) {
            return bail();
        }
    }

    void ShiftIPC::handleGetBlockNumber() {
        quint64 result;
        if ( !readNumber(result) ) {
             return bail();
        }

        emit getBlockNumberDone(result);
        done();
    }

    void ShiftIPC::getPeerCount() {
        if ( !queueRequest(RequestIPC(NonVisual, GetPeerCount, "net_peerCount")) ) {
            return bail();
        }
    }

    void ShiftIPC::handleGetPeerCount() {
        if ( !readNumber(fPeerCount) ) {
             return bail();
        }

        emit peerCountChanged(fPeerCount);
        done();
    }

    void ShiftIPC::sendTransaction(const QString& from, const QString& to, const QString& valStr, const QString& gas) {
        QJsonArray params;
        const QString valHex = Helpers::toHexWeiStr(valStr);
        ShiftLog::logMsg(QString("Trans Value: ") + valStr + QString(" HexValue: ") + valHex);

        QJsonObject p;
        p["from"] = from;
        p["to"] = to;
        p["value"] = valHex;
        if ( !gas.isEmpty() ) {
            const QString gasHex = Helpers::decStrToHexStr(gas);
            p["gas"] = gasHex;
        }

        params.append(p);

        if ( !queueRequest(RequestIPC(SendTransaction, "shf_sendTransaction", params)) ) {
            return bail(true); // softbail
        }
    }

    void ShiftIPC::handleSendTransaction() {
        QJsonValue jv;
        if ( !readReply(jv) ) {
            return bail(true); // softbail
        }

        const QString hash = jv.toString();
        emit sendTransactionDone(hash);
        done();
    }

    int ShiftIPC::getConnectionState() const {
        if ( fSocket.state() == QLocalSocket::ConnectedState ) {
            return 1; // TODO: add higher states per peer count!
        }

        return 0;
    }

    void ShiftIPC::unlockAccount(const QString& hash, const QString& password, int duration, int index) {
        QJsonArray params;
        params.append(hash);
        params.append(password);

        BigInt::Vin vinVal(duration);
        QString strHex = QString(vinVal.toStr0xHex().data());
        params.append(strHex);

        if ( !queueRequest(RequestIPC(UnlockAccount, "personal_unlockAccount", params, index)) ) {
            return bail();
        }
    }

    void ShiftIPC::handleUnlockAccount() {
        QJsonValue jv;
        if ( !readReply(jv) ) {
            return bail();
        }

        const bool result = jv.toBool(false);

        if ( !result ) {
            setError("Unlock account failure");
            if ( parseVersionNum() == 100002 ) {
                fError += " gshift v1.0.2 has a bug with unlocking empty password accounts! Consider updating";
            }
            emit error();
        }
        emit unlockAccountDone(result, fActiveRequest.getIndex());
        done();
    }

    void ShiftIPC::getGasPrice() {
        if ( !queueRequest(RequestIPC(GetGasPrice, "shf_nrgPrice")) ) {
            return bail();
        }
    }

    void ShiftIPC::handleGetGasPrice() {
        QJsonValue jv;
        if ( !readReply(jv) ) {
            return bail();
        }

        const QString decStr = Helpers::toDecStrShift(jv);

        emit getGasPriceDone(decStr);
        done();
    }

    quint64 ShiftIPC::peerCount() const {
        return fPeerCount;
    }

    void ShiftIPC::estimateGas(const QString& from, const QString& to, const QString& value) {
        QJsonArray params;
        QJsonObject o;
        o["to"] = to;
        o["from"] = from;
        o["value"] = Helpers::toHexWeiStr(value);
        o["gas"] = Helpers::toHexStr(90000);
        params.append(o);
        params.append(QString("latest"));
        if ( !queueRequest(RequestIPC(EstimateGas, "shf_estimateGas", params)) ) {
            return bail();
        }
    }

    void ShiftIPC::handleEstimateGas() {
        QJsonValue jv;
        if ( !readReply(jv) ) {
            return bail();
        }

        const QString price = Helpers::toDecStr(jv);

        emit estimateGasDone(price);

        done();
    }

    void ShiftIPC::newFilter() {
        if ( !queueRequest(RequestIPC(NewFilter, "shf_newBlockFilter")) ) {
            return bail();
        }
    }

    void ShiftIPC::handleNewFilter() {
        BigInt::Vin bv;
        if ( !readVin(bv) ) {
            return bail();
        }
        const QString strDec = QString(bv.toStrDec().data());
        fFilterID = strDec.toInt();

        if ( fFilterID < 0 ) {
            setError("Filter ID invalid");
            return bail();
        }

        done();
    }

    void ShiftIPC::onTimer() {
        getPeerCount();
        getFilterChanges(fFilterID);
    }

    int ShiftIPC::parseVersionNum() const {
        QRegExp reg("^gshift/v([0-9]+)\\.([0-9]+)\\.([0-9]+).*$");
        reg.indexIn(fClientVersion);
        if ( reg.captureCount() == 3 ) try { // it's gshift
            return reg.cap(1).toInt() * 100000 + reg.cap(2).toInt() * 1000 + reg.cap(3).toInt();
        } catch ( ... ) {
            return 0;
        }

        return 0;
    }

    void ShiftIPC::getFilterChanges(int filterID) {
        if ( filterID < 0 ) {
            setError("Filter ID invalid");
            return bail();
        }

        QJsonArray params;
        BigInt::Vin vinVal(filterID);
        QString strHex = QString(vinVal.toStr0xHex().data());
        params.append(strHex);

        if ( !queueRequest(RequestIPC(NonVisual, GetFilterChanges, "shf_getFilterChanges", params, filterID)) ) {
            return bail();
        }
    }

    void ShiftIPC::getClientVersion() {
        if ( !queueRequest(RequestIPC(NonVisual, GetClientVersion, "web3_clientVersion")) ) {
            return bail();
        }
    }

    void ShiftIPC::handleGetFilterChanges() {
        QJsonValue jv;
        if ( !readReply(jv) ) {
            return bail();
        }

        QJsonArray ar = jv.toArray();
        foreach( const QJsonValue v, ar ) {
           const QString hash = v.toString("bogus");
           getBlockByHash(hash);
        }

        done();
    }

    void ShiftIPC::uninstallFilter() {
        QJsonArray params;
        BigInt::Vin vinVal(fFilterID);
        QString strHex = QString(vinVal.toStr0xHex().data());
        params.append(strHex);

        if ( !queueRequest(RequestIPC(UninstallFilter, "shf_uninstallFilter", params)) ) {
            return bail();
        }
    }

    void ShiftIPC::handleUninstallFilter() {
        QJsonValue jv;
        if ( !readReply(jv) ) {
            return bail();
        }

        fFilterID = -1;

        done();
    }

    void ShiftIPC::getTransactionByHash(const QString& hash) {
        QJsonArray params;
        params.append(hash);

        if ( !queueRequest(RequestIPC(GetTransactionByHash, "shf_getTransactionByHash", params)) ) {
            return bail();
        }
    }

    void ShiftIPC::handleGetTransactionByHash() {
        QJsonValue jv;
        if ( !readReply(jv) ) {
            return bail();
        }

        emit newTransaction(TransactionInfo(jv.toObject()));
        done();
    }

    void ShiftIPC::getBlockByHash(const QString& hash) {
        QJsonArray params;
        params.append(hash);
        params.append(true); // get transaction bodies

        if ( !queueRequest(RequestIPC(GetBlock, "shf_getBlockByHash", params)) ) {
            return bail();
        }
    }

    void ShiftIPC::getBlockByNumber(quint64 blockNum) {
        QJsonArray params;
        params.append(Helpers::toHexStr(blockNum));
        params.append(true); // get transaction bodies

        if ( !queueRequest(RequestIPC(GetBlock, "shf_getBlockByNumber", params)) ) {
            return bail();
        }
    }

    void ShiftIPC::handleGetBlock() {
        QJsonValue jv;
        if ( !readReply(jv) ) {
            return bail();
        }

        const QJsonObject block = jv.toObject();
        const quint64 num = Helpers::toQUInt64(block.value("number"));
        emit getBlockNumberDone(num);
        emit newBlock(block);
        done();
    }

    void ShiftIPC::handleGetClientVersion() {
        QJsonValue jv;
        if ( !readReply(jv) ) {
            return bail();
        }

        fClientVersion = jv.toString();

        const int vn = parseVersionNum();
        if ( vn > 0 && vn < 100002 ) {
            setError("gshift version 2.0.0 and older contain a critical bug! Please update immediately.");
            emit error();
        }

        emit clientVersionChanged(fClientVersion);
        done();
    }

    void ShiftIPC::abort() {
        fAborted = true;
        bail();
        fSocket.abort();
        emit connectionStateChanged();
    }

    void ShiftIPC::bail(bool soft) {
        qDebug() << "BAIL[" << soft << "]: " << fError << "\n";

        if ( !soft ) {
            fTimer.stop();
            fRequestQueue.clear();
        }

        fActiveRequest = RequestIPC(None);
        errorOut();
    }

    void ShiftIPC::setError(const QString& error) {
        fError = error;
        ShiftLog::logMsg(error, LS_Error);
    }

    void ShiftIPC::errorOut() {
        emit error();
        emit connectionStateChanged();
        done();
    }

    void ShiftIPC::done() {
        fActiveRequest = RequestIPC(None);
        if ( !fRequestQueue.isEmpty() ) {
            const RequestIPC request = fRequestQueue.first();
            fRequestQueue.removeFirst();
            writeRequest(request);
        } else {
            emit busyChanged(getBusy());
        }
    }

    QJsonObject ShiftIPC::methodToJSON(const RequestIPC& request) {
        QJsonObject result;

        result.insert("jsonrpc", QJsonValue(QString("2.0")));
        result.insert("method", QJsonValue(request.getMethod()));
        result.insert("id", QJsonValue(request.getCallID()));
        result.insert("params", QJsonValue(request.getParams()));

        return result;
    }

    bool ShiftIPC::queueRequest(const RequestIPC& request) {
        if ( fActiveRequest.burden() == None ) {
            return writeRequest(request);
        } else {
            fRequestQueue.append(request);
            return true;
        }
    }

    bool ShiftIPC::writeRequest(const RequestIPC& request) {
        fActiveRequest = request;
        if ( fActiveRequest.burden() == Full ) {
            emit busyChanged(getBusy());
        }

        QJsonDocument doc(methodToJSON(fActiveRequest));
        const QString msg(doc.toJson());

        if ( !fSocket.isWritable() ) {
            setError("Socket not writeable");
            fCode = 0;
            return false;
        }

        const QByteArray sendBuf = msg.toUtf8();
        ShiftLog::logMsg("Sent: " + msg, LS_Debug);
        const int sent = fSocket.write(sendBuf);

        if ( sent <= 0 ) {
            setError("Error on socket write: " + fSocket.errorString());
            fCode = 0;
            return false;
        }

        return true;
    }

    bool ShiftIPC::readData() {
        fReadBuffer += QString(fSocket.readAll());

        if ( fReadBuffer.at(0) == '{' && fReadBuffer.at(fReadBuffer.length() - 1) == '}' && fReadBuffer.count('{') == fReadBuffer.count('}') ) {
            ShiftLog::logMsg("Received: " + fReadBuffer, LS_Debug);
            return true;
        }

        return false;
    }

    bool ShiftIPC::readReply(QJsonValue& result) {
        const QString data = fReadBuffer;
        fReadBuffer.clear();

        if ( data.isEmpty() ) {
            setError("Error on socket read: " + fSocket.errorString());
            fCode = 0;
            return false;
        }

        QJsonParseError parseError;
        QJsonDocument resDoc = QJsonDocument::fromJson(data.toUtf8(), &parseError);

        if ( parseError.error != QJsonParseError::NoError ) {
            qDebug() << data << "\n";
            setError("Response parse error: " + parseError.errorString());
            fCode = 0;
            return false;
        }

        const QJsonObject obj = resDoc.object();
        const int objID = obj["id"].toInt(-1);

        if ( objID != fActiveRequest.getCallID() ) { // TODO
            setError("Call number mismatch " + QString::number(objID) + " != " + QString::number(fActiveRequest.getCallID()));
            fCode = 0;
            return false;
        }

        result = obj["result"];

        if ( result.isUndefined() || result.isNull() ) {
            if ( obj.contains("error") ) {
                if ( obj["error"].toObject().contains("message") ) {
                    fError = obj["error"].toObject()["message"].toString();
                }

                if ( obj["error"].toObject().contains("code") ) {
                    fCode = obj["error"].toObject()["code"].toInt();
                }

                return false;
            }

            if ( fActiveRequest.getType() != GetTransactionByHash ) { // this can happen if out of sync, it's not fatal for transaction get
                setError("Result object undefined in IPC response for request: " + fActiveRequest.getMethod());
                qDebug() << data << "\n";
                return false;
            }
        }

        return true;
    }

    bool ShiftIPC::readVin(BigInt::Vin& result) {
        QJsonValue jv;
        if ( !readReply(jv) ) {
            return false;
        }

        std::string hexStr = jv.toString("0x0").remove(0, 2).toStdString();
        result = BigInt::Vin(hexStr, 16);

        return true;
    }

    bool ShiftIPC::readNumber(quint64& result) {
        BigInt::Vin r;
        if ( !readVin(r) ) {
            return false;
        }

        result = r.toUlong();
        return true;
    }

    void ShiftIPC::onSocketError(QLocalSocket::LocalSocketError err) {
        if ( fAborted ) {
            return; // ignore
        }

        fError = fSocket.errorString();
        fCode = err;
    }

    void ShiftIPC::onSocketReadyRead() {
        if ( !getBusy() ) {
            return; // probably error-ed out
        }

        if ( !readData() ) {
            return; // not finished yet
        }

        switch ( fActiveRequest.getType() ) {
        case NewAccount: {
                handleNewAccount();
                break;
            }
        case DeleteAccount: {
                handleDeleteAccount();
                break;
            }
        case GetBlockNumber: {
                handleGetBlockNumber();
                break;
            }
        case GetAccountRefs: {
                handleGetAccounts();
                break;
            }
        case GetBalance: {
                handleAccountBalance();
                break;
            }
        case GetTransactionCount: {
                handleAccountTransactionCount();
                break;
            } 
        case GetPeerCount: {
                handleGetPeerCount();
                break;
            }
        case SendTransaction: {
                handleSendTransaction();
                break;
            }
        case UnlockAccount: {
                handleUnlockAccount();
                break;
            }
        case GetGasPrice: {
                handleGetGasPrice();
                break;
            }
        case EstimateGas: {
                handleEstimateGas();
                break;
            }
        case NewFilter: {
                handleNewFilter();
                break;
            }
        case GetFilterChanges: {
                handleGetFilterChanges();
                break;
            }
        case UninstallFilter: {
                handleUninstallFilter();
                break;
            }
        case GetTransactionByHash: {
                handleGetTransactionByHash();
                break;
            }
        case GetBlock: {
                handleGetBlock();
                break;
            }
        case GetClientVersion: {
                handleGetClientVersion();
                break;
            }
        default: qDebug() << "Unknown reply: " << fActiveRequest.getType() << "\n"; break;
        }
    }

}
