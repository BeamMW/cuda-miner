#include "pch.hpp"
#include "url.hpp"
#include <string>
#include <algorithm>
#include <cctype>
#include <functional>
#include <iterator>

using namespace std;

void Url::parse(const string& aUrl)
{
    const string prot_end("://");
    string::const_iterator prot_i = search(aUrl.begin(), aUrl.end(), prot_end.begin(), prot_end.end());
    _protocol.reserve(distance(aUrl.begin(), prot_i));
    transform(aUrl.begin(), prot_i, back_inserter(_protocol), ptr_fun<int,int>(tolower)); // protocol is icase
	if (prot_i == aUrl.end()) {
		return;
	}
    advance(prot_i, prot_end.length());
    string::const_iterator path_i = find(prot_i, aUrl.end(), '/');
    _host.reserve(distance(prot_i, path_i));
    transform(prot_i, path_i, back_inserter(_host),  ptr_fun<int,int>(tolower)); // host is icase
    string::const_iterator query_i = find(path_i, aUrl.end(), '?');
    _path.assign(path_i, query_i);
	if (query_i != aUrl.end()) {
		++query_i;
	}
    _query.assign(query_i, aUrl.end());
}
