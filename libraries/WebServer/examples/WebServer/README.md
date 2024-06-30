# Arduino-ESP32 WebServer Example for WebServer Library

This example shows different techniques on how to use and extend the WebServer for specific purposes

It is a small project in it's own and has some files to use on the web server to show how to use simple REST based services.

This example requires some space for a filesystem and runs fine boards with 4 MByte flash using the following options:

* Board: ESP32 Dev Module
* Partition Scheme: Default 4MB with spiffs (1.2MB APP/1.5MB SPIFFS)
    but LittleFS will be used in the partition (not SPIFFS)

It features

* Setup a web server
* redirect when accessing the url with servername only
* get real time by using builtin NTP functionality
* send HTML responses from Sketch (see builtinfiles.h)
* use a LittleFS file system on the data partition for static files
* use http ETag Header for client side caching of static files
* use custom ETag calculation for static files
* extended FileServerHandler for uploading and deleting static files
* uploading files using drag & drop
* serve APIs using REST services (/api/list, /api/sysinfo)
* define HTML response when no file/api/handler was found

## Supported Targets

Currently, this example supports the following targets.

| Supported Targets | ESP32 | ESP32-S2 | ESP32-C3 |
| ----------------- | ----- | -------- | -------- |
|                   | yes   | yes      | yes      |

## Use the Example

How to install the Arduino IDE: [Install Arduino IDE](https://github.com/espressif/arduino-esp32/tree/master/docs/arduino-ide).

* In the file `secrets.h` you can add the home WiFi network name ans password.
* Compile and upload to the device.
* Have a look into the monitoring output.
* Open <http://webserver> or <http://(ip-address)> using a browser.
* You will be redirected to <http://webserver/$upload.htm> as there are no files yet in the file system.
* Drag the files from the data folder onto the drop area shown in the browser.
* See below for more details

## Implementing a web server

The WebServer library offers a simple path to implement a web server on a ESP32 based board.

The advantage on using the WebServer instead of the plain simple NetworkServer is that the WebServer
takes much care about the http protocol conventions and features and allows easily access to parameters.
It offers plug-in capabilities by registering specific functionalities that will be outlined below.

### Initialization

In the setup() function in the webserver.ino sketch file the following steps are implemented to make the webserver available on the local network.

* Create a webserver listening to port 80 for http requests.
* Initialize the access to the filesystem in the free flash memory.
* Connect to the local WiFi network. Here is only a straight-forward implementation hard-coding network name and passphrase. You may consider to use something like the WiFiManager library in real applications.
* Register the device in DNS using a known hostname.
* Registering several plug-ins (see below).
* Starting the web server.

### Running

In the loop() function the web server will be given time to receive and send network packages by calling
`server.handleClient();`.

## Registering simple functions to implement RESTful services

Registering function is the simplest integration mechanism available to add functionality. The server offers the `on(path, function)` methods that take the URL and the function as parameters.

There are 2 functions implemented that get registered to handle incoming GET requests for given URLs.

The JSON data format is used often for such services as it is the "natural" data format of the browser using javascript.

When the **handleSysInfo()** function is registered and a browser requests for <http://webserver/api/sysinfo> the function will be called and can collect the requested information.

> ```CPP
> server.on("/api/sysinfo", handleSysInfo);
> ```

The result in this case is a JSON object that is assembled in the result String variable and the returned as a response to the client also giving the information about the data format.

You can try this request in a browser by opening <http://webserver/api/sysinfo> in the address bar.

> ```CPP
> server.on("/api/sysinfo", handleList);
> ```

The function **handleList()** is registered the same way to return the list of files in the file system also returning a JSON object including name, size and the last modification timestamp.

You can try this request in a browser by opening <http://webserver/api/list> in the address bar.

## Registering a function to send out some static content from a String

This is an example of registering a inline function in the web server.
The 2. parameter of the on() method is a so called CPP lambda function (without a name)
that actually has only one line of functionality by sending a string as result to the client.

> ``` cpp
> server.on("/$upload.htm", []() {
>   server.send(200, "text/html", FPSTR(uploadContent));
> });
> ```

Here the text from a static String with html code is returned instead of a file from the filesystem.
The content of this string can be found in the file `builtinfiles.h`. It contains a small html+javascript implementation
that allows uploading new files into the empty filesystem.

Just open <http://webserver/$upload.htm> and drag some files from the data folder on the drop area.

## Registering a function to handle requests to the server without a path

Often servers are addressed by using the base URL like <http://webserver/> where no further path details is given.
Of course we like the user to be redirected to something usable. Therefore the `handleRoot()` function is registered:

> ``` cpp
> server.on("/$upload.htm", handleRoot);
> ```

The `handleRoot()` function checks the filesystem for the file named **/index.htm** and creates a redirect to this file when the file exists.
Otherwise the redirection goes to the built-in **/$upload.htm** web page.

## Using the serveStatic plug-in

The **serveStatic** plug in is part of the library and handles delivering files from the filesystem to the client. It can be customized in some ways.

> ``` cpp
> server.enableCORS(true);
> server.enableETag(true);
> server.serveStatic("/", LittleFS, "/");
> ```

### Cross-Origin Resource Sharing (CORS)

The `enableCORS(true)` function adds a `Access-Control-Allow-Origin: *` http-header to all responses to the client
to inform that it is allowed to call URLs and services on this server from other web sites.

The feature is disabled by default (in the current version) and when you like to disable this then you should call `enableCORS(false)` during setup.

* Web sites providing high sensitive information like online banking this is disabled most of the times.
* Web sites providing advertising information or reusable scripts / images this is enabled.

### enabling ETag support

To enable this in the embedded web server the `enableETag()` can be used.
(next to enableCORS)

In the simplest version just call `enableETag(true)` to enable the internal ETag generation that calcs the hint using a md5 checksum in base64 encoded form. This is an simple approach that adds some time for calculation on every request but avoids network traffic.

The headers will look like:

``` txt
If-None-Match: "GhZka3HevoaEBbtQOgOqlA=="
ETag: "GhZka3HevoaEBbtQOgOqlA=="
```


### ETag support customization

The enableETag() function has an optional second optional parameter to provide a function for ETag calculation of files.

The function enables eTags for all files by using calculating a value from the last write timestamp:

``` cpp
server.enableETag(true, [](FS &fs, const String &path) -> String {
  File f = fs.open(path, "r");
  String eTag = String(f.getLastWrite(), 16);  // use file modification timestamp to create ETag
  f.close();
  return (eTag);
});
```

The headers will look like:

``` txt
ETag: "63bbaeb5"
If-None-Match: "63bbaeb5"
```


## Registering a full-featured handler as plug-in

The example also implements the class `FileServerHandler` derived from the class `RequestHandler` to plug in functionality
that can handle more complex requests without giving a fixed URL.
It implements uploading and deleting files in the file system that is not implemented by the standard server.serveStatic functionality.

This class has to implements several functions and works in a more detailed way:

* The `canHandle()` method can inspect the given http method and url to decide weather the RequestFileHandler can handle the incoming request or not.

  In this case the RequestFileHandler will return true when the request method is an POST for upload or a DELETE for deleting files.

  The regular GET requests will be ignored and therefore handled by the also registered server.serveStatic handler.

* The function `handle()` then implements the real deletion of the file.

* The `canUpload()`and `upload()` methods work similar while the `upload()` method is called multiple times to create, append data and close the new file.

## File upload

By opening <http://webserver/$upload.htm> you can easily upload files by dragging them over the drop area.

Just take the files from the data folder to create some files that can explore the server functionality.

Files will be uploaded to the root folder of the file system. and you will see it next time using  <http://webserver/files.htm>.

The filesize that is uploaded is not known when the upload mechanism in function
FileServerHandler::upload gets started.

Uploading a file that fits into the available filesystem space
can be found in the Serial output:

``` txt
starting upload file /file.txt...
finished.
1652 bytes uploaded.
```

Uploading a file that doesn't fit can be detected while uploading when writing to the filesystem fails.
However upload cannot be aborted by the current handler implementation.

The solution implemented here is to delete the partially uploaded file and wait for the upload ending.
The following can be found in the Serial output:

``` txt
starting upload file /huge.jpg...
./components/esp_littlefs/src/littlefs/lfs.c:584:error: No more free space 531
  write error!
finished.
```

You can see on the Serial output that one filesystem write error is reported.

Please be patient and wait for the upload ending even when writing to the filesystem is disabled
it maybe take more than a minute.

## Registering a special handler for "file not found"

Any other incoming request that was not handled by the registered plug-ins above can be detected by registering

> ``` cpp
> // handle cases when file is not found
> server.onNotFound([]() {
>   // standard not found in browser.
>   server.send(404, "text/html", FPSTR(notFoundContent));
> });
> ```

This allows sending back an "friendly" result for the browser. Here a simple html page is created from a static string.
You can easily change the html code in the file `builtinfiles.h`.

## customizations

You may like to change the hostname and the timezone in the lines:

> ``` cpp
> #define HOSTNAME "webserver"
> #define TIMEZONE "CET-1CEST,M3.5.0,M10.5.0/3"
> ```

## Troubleshooting

Have a look in the Serial output for some additional runtime information.

## Contribute

To know how to contribute to this project, see [How to contribute.](https://github.com/espressif/arduino-esp32/blob/master/CONTRIBUTING.rst)

If you have any **feedback** or **issue** to report on this example/library, please open an issue or fix it by creating a new PR. Contributions are more than welcome!

Before creating a new issue, be sure to try Troubleshooting and check if the same issue was already created by someone else.

## Resources

* Official ESP32 Forum: [Link](https://esp32.com)
* Arduino-ESP32 Official Repository: [espressif/arduino-esp32](https://github.com/espressif/arduino-esp32)
* ESP32 Datasheet: [Link to datasheet](https://www.espressif.com/sites/default/files/documentation/esp32_datasheet_en.pdf)
* ESP32-S2 Datasheet: [Link to datasheet](https://www.espressif.com/sites/default/files/documentation/esp32-s2_datasheet_en.pdf)
* ESP32-C3 Datasheet: [Link to datasheet](https://www.espressif.com/sites/default/files/documentation/esp32-c3_datasheet_en.pdf)
* Official ESP-IDF documentation: [ESP-IDF](https://idf.espressif.com)
