#ifndef MINERSMANAGER_URL_HHP
#define MINERSMANAGER_URL_HHP

#include <string>

class Url
{
public:
	Url(const std::string& aUrl) { parse(aUrl); }

	const std::string & protocol() const { return _protocol; }
	const std::string & host() const { return _host; }
	const std::string & path() const { return _path; }
	const std::string & query() const { return _query; }

private:
    void parse(const std::string& aUrl);

private:
	std::string _protocol;
	std::string _host;
	std::string _path;
	std::string _query;
};

#endif	// MINERSMANAGER_URL_HHP
