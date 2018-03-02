// Class http server can be used as a base class to provide simple
// http server functionality to facilitate data exploration and debugging.
// The derived class only has to override
// function processRequest.

#ifndef CZI_EXPRESSION_MATRIX2_HTTP_SERVER_HPP
#define CZI_EXPRESSION_MATRIX2_HTTP_SERVER_HPP

#include <boost/asio/ip/tcp.hpp>
#include "boost_lexical_cast.hpp"

#include "iosfwd.hpp"
#include "set.hpp"
#include "string.hpp"
#include "vector.hpp"

namespace ChanZuckerberg {
    namespace ExpressionMatrix2 {
        class HttpServer;
    }
}



class ChanZuckerberg::ExpressionMatrix2::HttpServer {
public:

    // This function puts the server into an endless loop
    // of processing requests.
    void explore(uint16_t port);

    // The destructor needs to be virtual for clean destruction of
    // the derived class.
    virtual ~HttpServer()
    {
    }

protected:

    // The derived class should override this.
    // It is passed the string of the GET request,
    // already parsed using "?=&" as separators.
    // It should write the response to the given request on the stream passed as a second argument.
    // The request is guaranteed not to be empty.
    class BrowserInformation;
    virtual void processRequest(const vector<string>& request, ostream& html, const BrowserInformation&) = 0;


    // This function can be used by the derived class to get the value of a parameter.
    // If the parameter is missing, returns false and the value is not touched.
    template<class T> static bool getParameterValue(const vector<string>& request, const string& name, T& value)
    {
        for(size_t i = 0; i < request.size() - 1; i++) {
            if(request[i] == name) {
                try {
                    value = lexical_cast<T>(request[i + 1]);
                } catch (...) {
                    return false;
                }
                return true;
            }
        }
        return false;
    }

    // Return all values assigned to a parameter.
    // For example, if the request has ...&a=xyz&a=uv,
    // when called with argument "a" returns a set containing "xyz" and "uv".
    static void getParameterValues(const vector<string>& request, const string& name, vector<string>& values);
    static void getParameterValues(const vector<string>& request, const string& name, set<string>& values);

    static void writeStyle(ostream& html);

    static ostream& writeJQuery(ostream& html);
    static ostream& writeTableSorter(ostream& html);

    // This takes care of percent encoding in the url.
    // I adapted it from boost asio example http/server3.
    // This is necessary if we want to be able to accept form data that
    // contain characters that are forbidden in an URL.
    bool urlDecode(const string& in, string& out);


    // Function to do URL encoding.
    // This is necessary when we create a hyperlink that contains special characters.
    // Adapted from here:
    // https://stackoverflow.com/questions/154536/encode-decode-urls-in-c
    static string urlEncode(const string&);


protected:

    // Class used to identify the browser that issued a request.
    class BrowserInformation {
    public:
        bool isChrome = false;
        bool isFirefox = false;
        bool isEdge = false;
        void set(const string& userAgentHeader);
    };



private:
    void processRequest(boost::asio::ip::tcp::iostream&);

};

#endif

