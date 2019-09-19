/*
 * Copyright (C) 2019 Zilliqa
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "IsolatedServer.h"
#include "JSONConversion.h"

using namespace jsonrpc;
using namespace std;

IsolatedServer::IsolatedServer(Mediator& mediator,
                               AbstractServerConnector& server,
                               const uint64_t& blocknum)
    : LookupServer(mediator, server),
      jsonrpc::AbstractServer<IsolatedServer>(server,
                                              jsonrpc::JSONRPC_SERVER_V2),
      m_blocknum(blocknum) {
  AbstractServer<IsolatedServer>::bindAndAddMethod(
      jsonrpc::Procedure("CreateTransaction", jsonrpc::PARAMS_BY_POSITION,
                         jsonrpc::JSON_STRING, "param01", jsonrpc::JSON_OBJECT,
                         NULL),
      &IsolatedServer::CreateTransactionI);
  AbstractServer<IsolatedServer>::bindAndAddMethod(
      jsonrpc::Procedure("IncreaseBlocknum", jsonrpc::PARAMS_BY_POSITION,
                         jsonrpc::JSON_OBJECT, "param01", jsonrpc::JSON_INTEGER,
                         NULL),
      &IsolatedServer::IncreaseBlocknumI);
  AbstractServer<IsolatedServer>::bindAndAddMethod(
      jsonrpc::Procedure("GetBalance", jsonrpc::PARAMS_BY_POSITION,
                         jsonrpc::JSON_OBJECT, "param01", jsonrpc::JSON_STRING,
                         NULL),
      &LookupServer::GetBalanceI);
  AbstractServer<IsolatedServer>::bindAndAddMethod(
      jsonrpc::Procedure(
          "GetSmartContractSubState", jsonrpc::PARAMS_BY_POSITION,
          jsonrpc::JSON_OBJECT, "param01", jsonrpc::JSON_STRING, "param02",
          jsonrpc::JSON_STRING, "param03", jsonrpc::JSON_ARRAY, NULL),
      &LookupServer::GetSmartContractSubStateI);
  AbstractServer<IsolatedServer>::bindAndAddMethod(
      jsonrpc::Procedure("GetSmartContractState", jsonrpc::PARAMS_BY_POSITION,
                         jsonrpc::JSON_OBJECT, "param01", jsonrpc::JSON_STRING,
                         NULL),
      &LookupServer::GetSmartContractStateI);
  AbstractServer<IsolatedServer>::bindAndAddMethod(
      jsonrpc::Procedure("GetSmartContractCode", jsonrpc::PARAMS_BY_POSITION,
                         jsonrpc::JSON_OBJECT, "param01", jsonrpc::JSON_STRING,
                         NULL),
      &LookupServer::GetSmartContractCodeI);
  AbstractServer<IsolatedServer>::bindAndAddMethod(
      jsonrpc::Procedure("GetMinimumGasPrice", jsonrpc::PARAMS_BY_POSITION,
                         jsonrpc::JSON_STRING,
                         NULL),
      &IsolatedServer::GetMinimumGasPriceI);
  AbstractServer<IsolatedServer>::bindAndAddMethod(
      jsonrpc::Procedure("SetMinimumGasPrice", jsonrpc::PARAMS_BY_POSITION,
                         jsonrpc::JSON_STRING,"param01", jsonrpc::JSON_STRING,
                         NULL),
      &IsolatedServer::SetMinimumGasPriceI);
  AbstractServer<IsolatedServer>::bindAndAddMethod(
      jsonrpc::Procedure("GetSmartContracts", jsonrpc::PARAMS_BY_POSITION,
                         jsonrpc::JSON_ARRAY, "param01", jsonrpc::JSON_STRING,
                         NULL),
      &LookupServer::GetSmartContractsI);
}

Json::Value IsolatedServer::CreateTransaction(const Json::Value& _json) {
  try {
    if (!JSONConversion::checkJsonTx(_json)) {
      throw JsonRpcException(RPC_PARSE_ERROR, "Invalid Transaction JSON");
    }

    LOG_GENERAL(INFO, "On the isolated server ");

    Transaction tx = JSONConversion::convertJsontoTx(_json);

    Json::Value ret;

    const Address fromAddr = tx.GetSenderAddr();
    const Account* sender = AccountStore::GetInstance().GetAccount(fromAddr);

    if (!ValidateTxn(tx, fromAddr, sender)) {
      return ret;
    }

    if (sender->GetNonce() + 1 != tx.GetNonce()) {
      throw JsonRpcException(
          RPC_INVALID_PARAMETER,
          "Expected Nonce: " + to_string(sender->GetNonce() + 1));
    }

    if (sender->GetBalance() < tx.GetAmount()) {
      throw JsonRpcException(
          RPC_INVALID_PARAMETER,
          "Insufficient Balance: " + sender->GetBalance().str());
    }

    if(m_gasPrice > tx.GetGasPrice())
    {
      throw JsonRpcException(RPC_INVALID_PARAMETER, "Minimum gas price greater: "+m_gasPrice.str());
    }

    TransactionReceipt txreceipt;
    AccountStore::GetInstance().UpdateAccountsTemp(m_blocknum,
                                                   3  // Arbitrary values
                                                   ,
                                                   true, tx, txreceipt);

    AccountStore::GetInstance().SerializeDelta();
    AccountStore::GetInstance().CommitTemp();

    return txreceipt.GetJsonValue();

  } catch (const JsonRpcException& je) {
    throw je;
  } catch (exception& e) {
    LOG_GENERAL(INFO,
                "[Error]" << e.what() << " Input: " << _json.toStyledString());
    throw JsonRpcException(RPC_MISC_ERROR, "Unable to Process");
  }

  return Json::Value();
}

string IsolatedServer::IncreaseBlocknum(const uint32_t& delta) {
  m_blocknum += delta;

  return to_string(m_blocknum);
}

string IsolatedServer::SetMinimumGasPrice(const string& gasPrice)
{
  uint128_t newGasPrice;
  try
  { 
    newGasPrice = move(uint128_t(gasPrice));
  }
  catch(exception& e)
  {
    throw JsonRpcException(RPC_INVALID_PARAMETER, "Gas price should be numeric");
  }
  if(newGasPrice < 1)
  {
    throw JsonRpcException(RPC_INVALID_PARAMETER, "Gas price cannot be less than 1");
  }

  m_gasPrice = move(newGasPrice);

  return m_gasPrice.str();
}

string IsolatedServer::GetMinimumGasPrice()
{
  return m_gasPrice.str();
}