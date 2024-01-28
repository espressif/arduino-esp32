/**
 * @file builtinfiles.h
 * @brief This file is part of the WebServer example for the ESP8266WebServer.
 *  
 * This file contains long, multiline text variables for  all builtin resources.
 */

// used for $upload.htm
static const char uploadContent[] PROGMEM =
R"==(
<!doctype html>
<html lang='en'>

<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>Upload</title>
</head>

<body style="width:300px">
  <h1>Upload</h1>
  <div><a href="/">Home</a></div>
  <hr>
  <div id='zone' style='width:16em;height:12em;padding:10px;background-color:#ddd'>Drop files here...</div>

  <script>
    // allow drag&drop of file objects 
    function dragHelper(e) {
      e.stopPropagation();
      e.preventDefault();
    }

    // allow drag&drop of file objects 
    function dropped(e) {
      dragHelper(e);
      var fls = e.dataTransfer.files;
      var formData = new FormData();
      for (var i = 0; i < fls.length; i++) {
        formData.append('file', fls[i], '/' + fls[i].name);
      }
      fetch('/', { method: 'POST', body: formData }).then(function () {
        window.alert('done.');
      });
    }
    var z = document.getElementById('zone');
    z.addEventListener('dragenter', dragHelper, false);
    z.addEventListener('dragover', dragHelper, false);
    z.addEventListener('drop', dropped, false);
  </script>
</body>
)==";

// used for $upload.htm
static const char notFoundContent[] PROGMEM = R"==(
<html>
<head>
  <title>Resource not found</title>
</head>
<body>
  <p>The resource was not found.</p>
  <p><a href="/">Start again</a></p>
</body>
)==";
