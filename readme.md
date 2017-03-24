# Web Search service

The Web Search Service wraps up the access to a web-based search page and retrieves the results and puts them into the standard Grassroots results format. 

## Installation

To build this service, you need the [grassroots core](https://github.com/TGAC/grassroots-core) and [grassroots build config](https://github.com/TGAC/grassroots-build-config) installed and configured. 

The files to build the Web Search service are in the ```build/<platform>``` directory. 

### Linux

If you enter this directory 

```cd build/linux```

you can then build the service by typing

```make all```

and then 

```make install```

to install the service into the Grassroots system where it will be available for use immediately.


## Configuration options

Each external search engine that you wish to wrap is defined in a JSON-based configuration file. These configuration files are stored in the *references* folder at the root level of the Grassroots server installation directory.

It uses a JSON file that has the standard Grassroots schema describing its operations, parameters, *etc.* along with the following additional keys:

  * **plugin**: This is set to *web_search_service*.
  * **uri**: The web address of the search page to use.
  * **method**: This states the method used to send the query to the search page.
  	* **POST**: To send the search query as an HTTP POST request.
  	* **GET**: To send the search query as an HTTP GET request.
  * **selector**: The CSS selector to get each of the resultant hits from the search 
page's response. 

The referred service accesses this functionality be setting the **plugin** key to *web_search_service*. This web search instance can then be configured for the particular web site that is being wrapped.
 
 For example, a Grassroots server can access the search engine at [Agris](http://agris.fao.org/agris-search/index.do) and it uses the configuration file shown below:
  
~~~{.json}
{
	"schema_version": 1.0,
	"provider": {
		"name": "Agris",
		"description": "A service to search for academic articles.",
		"uri": "http://agris.fao.org/agris-search/index.do"
	},
	"services": {
		"service_name": "Agris Search service",
		"description": "A service to obtain articles using search terms",
		"plugin": "web_search_service",
		"operations": {
			"operation_id": "Agris Web Search service",
			"description": "An operation to obtain matching articles from Agris",
			"about_uri": "http://agris.fao.org/agris-search/index.do",
			"uri": "http://agris.fao.org/agris-search/searchIndex.do",
			"method": "GET",
			"selector": "div.result-item h3 a",
			"parameter_set": {
				"parameters": [{
					"param": "query",
					"name": "Query",
					"default_value": "",
					"current_value": "",
					"type": "string",
					"grassroots_type": "params:keyword",
					"description": "The search term"
				}]
			}
		}
	}
}
~~~