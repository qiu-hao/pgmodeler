/*
# PostgreSQL Database Modeler (pgModeler)
#
# Copyright 2006-2013 - Raphael Araújo e Silva <rkhaotix@gmail.com>
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation version 3.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# The complete text of GPLv3 is at LICENSE file on source code root directory.
# Also, you can get the complete GNU General Public License at <http://www.gnu.org/licenses/>
*/

#include "connection.h"

const QString Connection::SSL_DESABLE="disable";
const QString Connection::SSL_ALLOW="allow";
const QString Connection::SSL_PREFER="prefer";
const QString Connection::SSL_REQUIRE="require";
const QString Connection::SSL_CA_VERIF="verify-ca";
const QString Connection::SSL_FULL_VERIF="verify-full";
const QString Connection::PARAM_SERVER_FQDN="host";
const QString Connection::PARAM_SERVER_IP="hostaddr";
const QString Connection::PARAM_PORT="port";
const QString Connection::PARAM_DB_NAME="dbname";
const QString Connection::PARAM_USER="user";
const QString Connection::PARAM_PASSWORD="password";
const QString Connection::PARAM_CONN_TIMEOUT="connect_timeout";
const QString Connection::PARAM_OPTIONS="options";
const QString Connection::PARAM_SSL_MODE="sslmode";
const QString Connection::PARAM_SSL_CERT="sslcert";
const QString Connection::PARAM_SSL_KEY="sslkey";
const QString Connection::PARAM_SSL_ROOT_CERT="sslrootcert";
const QString Connection::PARAM_SSL_CRL="sslcrl";
const QString Connection::PARAM_KERBEROS_SERVER="krbsrvname";
const QString Connection::PARAM_LIB_GSSAPI="gsslib";

Connection::Connection(void)
{
	connection=nullptr;
}

Connection::Connection(const QString &server_fqdn, const QString &port, const QString &user, const QString &passwd, const QString &db_name)
{
	//Configures the basic connection params
	setConnectionParam(PARAM_SERVER_FQDN, server_fqdn);
	setConnectionParam(PARAM_PORT, port);
	setConnectionParam(PARAM_USER, user);
	setConnectionParam(PARAM_PASSWORD, passwd);
	setConnectionParam(PARAM_DB_NAME, db_name);

	//Estabelece a conexão
	connect();
}

Connection::~Connection(void)
{
	if(connection)
		PQfinish(connection);
}

void Connection::setConnectionParam(const QString &param, const QString &value)
{
	//Regexp used to validate the host address
	QRegExp ip_regexp("[0-9]+\\.[0-9]+\\.[0-9]+\\.[0-9]+");

	//Raise an error in case the param name is empty
	if(param=="")
		throw Exception(ERR_ASG_INV_CONN_PARAM, __PRETTY_FUNCTION__, __FILE__, __LINE__);

	/* Set the value to the specified param on the map.

	One special case is treated here, if user use the parameter SERVER_FQDN and the value
	is a IP address, the method will assign the value to the SERVER_IP parameter */
	if(param==PARAM_SERVER_FQDN && ip_regexp.exactMatch(value))
		connection_params[Connection::PARAM_SERVER_IP]=value;
	else
		connection_params[param]=value;

	//Updates the connection string
	generateConnectionString();
}

void Connection::generateConnectionString(void)
{
	map<QString, QString>::iterator itr;

	itr=connection_params.begin();

	//Scans the parameter map concatening the params (itr->first) / values (itr->second)
	connection_str="";
	while(itr!=connection_params.end())
	{
		if(!itr->second.isEmpty())
			connection_str+=itr->first + "=" + itr->second + " ";
		itr++;
	}
}

void Connection::connect(void)
{
	QString str_aux;

	/* If the connection string is not established indicates that the user
		is trying to connect without configuring connection parameters,
		thus an error is raised */
	if(connection_str=="")
		throw Exception(ERR_CONNECTION_NOT_CONFIGURED, __PRETTY_FUNCTION__, __FILE__, __LINE__);

	//Try to connect to the database
	connection=PQconnectdb(connection_str.toStdString().c_str());

	/* If the connection descriptor has not been allocated or if the connection state
		is CONNECTION_BAD it indicates that the connection was not successful */
	if(connection==nullptr || PQstatus(connection)==CONNECTION_BAD)
	{
		//Raise the error generated by the DBMS
		str_aux=QString(Exception::getErrorMessage(ERR_CONNECTION_NOT_STABLISHED))
						.arg(PQerrorMessage(connection));
		throw Exception(str_aux, ERR_CONNECTION_NOT_STABLISHED,
										__PRETTY_FUNCTION__, __FILE__, __LINE__);
	}
}

void Connection::close(void)
{
	//Raise an erro in case the user try to close a not opened connection
	if(!connection)
		throw Exception(ERR_OPR_NOT_ALOC_CONN, __PRETTY_FUNCTION__, __FILE__, __LINE__);

	//Finalizes the connection
	PQfinish(connection);
	connection=nullptr;
}

void Connection::reset(void)
{
	//Raise an erro in case the user try to reset a not opened connection
	if(!connection)
		throw Exception(ERR_OPR_NOT_ALOC_CONN, __PRETTY_FUNCTION__, __FILE__, __LINE__);

	//Reinicia a conexão
	PQreset(connection);
}

QString Connection::getConnectionParam(const QString &param)
{
	return(connection_params[param]);
}

map<QString, QString> Connection::getConnectionParams(void)
{
	return(connection_params);
}

QString Connection::getConnectionString(void)
{
	return(connection_str);
}

bool Connection::isStablished(void)
{
	return(connection!=nullptr);
}

QString  Connection::getPgSQLVersion(void)
{
	QString version;

	if(!connection)
		throw Exception(ERR_OPR_NOT_ALOC_CONN, __PRETTY_FUNCTION__, __FILE__, __LINE__);

	version=QString("%1").arg(PQserverVersion(connection));

	return(QString("%1.%2.%3")
				 .arg(version.mid(0,2).toInt()/10)
				 .arg(version.mid(2,2).toInt()/10)
				 .arg(version.mid(4,1).toInt()));
}

void Connection::executeDMLCommand(const QString &sql, ResultSet &result)
{
	ResultSet *new_res=nullptr;
	PGresult *sql_res=nullptr;

	//Raise an error in case the user try to close a not opened connection
	if(!connection)
		throw Exception(ERR_OPR_NOT_ALOC_CONN, __PRETTY_FUNCTION__, __FILE__, __LINE__);

	//Alocates a new result to receive the resultset returned by the sql command
	sql_res=PQexec(connection, sql.toStdString().c_str());

	//Raise an error in case the command sql execution is not sucessful
	if(strlen(PQerrorMessage(connection))>0)
	{
		throw Exception(QString(Exception::getErrorMessage(ERR_CMD_SQL_NOT_EXECUTED))
										.arg(PQerrorMessage(connection)),
										ERR_CMD_SQL_NOT_EXECUTED, __PRETTY_FUNCTION__, __FILE__, __LINE__, nullptr,
										QString(PQresultErrorField(sql_res, PG_DIAG_SQLSTATE)));
	}

	//Generates the resultset based on the sql result descriptor
	new_res=new ResultSet(sql_res);

	//Copy the new resultset to the parameter resultset
	result=*(new_res);

	//Deallocate the new resultset
	delete(new_res);
}

void Connection::executeDDLCommand(const QString &sql)
{
	PGresult *sql_res=nullptr;

	//Raise an error in case the user try to close a not opened connection
	if(!connection)
		throw Exception(ERR_OPR_NOT_ALOC_CONN, __PRETTY_FUNCTION__, __FILE__, __LINE__);

	sql_res=PQexec(connection, sql.toStdString().c_str());

	//Raise an error in case the command sql execution is not sucessful
	if(strlen(PQerrorMessage(connection)) > 0)
	{
		throw Exception(QString(Exception::getErrorMessage(ERR_CMD_SQL_NOT_EXECUTED))
										.arg(PQerrorMessage(connection)),
										ERR_CMD_SQL_NOT_EXECUTED, __PRETTY_FUNCTION__, __FILE__, __LINE__, nullptr,
										QString(PQresultErrorField(sql_res, PG_DIAG_SQLSTATE)));
	}
}

void Connection::operator = (Connection &conn)
{
	if(this->isStablished())
		this->close();

	this->connection_params=conn.connection_params;
	this->connection_str=conn.connection_str;
	//connect();
}
