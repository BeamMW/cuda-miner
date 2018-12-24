#include "pch.hpp"
#include "Worker.hpp"

using namespace core;

Worker::~Worker()
{
}

const std::string & Worker::GetServer() const
{
	return _server;
}

void Worker::SetServer(const std::string &aServer)
{
	_server = aServer;
}

const std::string & Worker::GetPort() const
{
	return _port;
}

void Worker::SetPort(const std::string &aPort)
{
	_port = aPort;
}

const std::string & Worker::GetUser() const
{
	return _user;
}

void Worker::SetUser(const std::string &aUser)
{
	_user = aUser;
}

const std::string & Worker::GetWorker() const
{
	return _worker;
}

void Worker::SetWorker(const std::string &aWorker)
{
	_worker = aWorker;
}

const std::string & Worker::GetPassword() const
{
	return _password;
}

void Worker::SetPassword(const std::string &aPassword)
{
	_password = aPassword;
}

bool Worker::IsConnected() const
{
	return _connected;
}

bool Worker::IsAuthorized() const
{
	return _authorized;
}

void Worker::SetTarget(const BigInteger &aTarget)
{
	_target = aTarget;
}

const BigInteger & Worker::GetTarget() const
{
	return _target;
}

void Worker::SetExtraNonce(const BigInteger &aNonce)
{
	_extraNonce = aNonce;
}

const BigInteger & Worker::GetExtraNonce() const
{
	return _extraNonce;
}
