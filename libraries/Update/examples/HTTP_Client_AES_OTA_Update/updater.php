<?php
  /* Updater Server-side Example */
  $brand_codes = array("20", "21");
  $commands = array("check", "download");

  function verify($valid){
    if(!$valid){
      http_response_code(404);
      echo "Sorry, page not found";
      die();
    }
  }

  $headers = array();
  foreach (getallheaders() as $name => $value) {
    $headers += [$name => $value];
  }
  verify( in_array($headers['Brand-Code'], $brand_codes) );

  $GetArgs = filter_input_array(INPUT_GET);
  verify( in_array($GetArgs['cmd'], $commands) );

  if($GetArgs['cmd'] == "check" || $GetArgs['cmd'] == "download"){
/*********************************************************************************/
/* $firmware version & filename definitions for different Brands, Models & Firmware versions */
    if($headers['Brand-Code'] == "21"){
      if($headers['Model'] == "HTTP_Client_AES_OTA_Update"){

        if($headers['Firmware'] < "0.9"){//ie. update to latest of this major version
          $firmware = array('version'=>"0.9", 'filename'=>"HTTP_Client_AES_OTA_Update-v0.9.xbin");
        }
        elseif($headers['Firmware'] == "0.9"){//ie. update between major versions
          $firmware = array('version'=>"1.0", 'filename'=>"HTTP_Client_AES_OTA_Update-v1.0.xbin");
        }
        elseif($headers['Firmware'] <= "1.4"){//ie. update to latest version
          $firmware = array('version'=>"1.4", 'filename'=>"HTTP_Client_AES_OTA_Update-v1.4.xbin");
        }

      }
    }
/*  end of $firmware definitions for firmware update images on server                */
/*********************************************************************************/

    if( !$firmware['filename'] || !file_exists($firmware['filename']) ){
      header('update: 0' );//no update available
      exit;
    }else{
      header('update: 1' );//update available
      header('version: ' . $firmware['version'] );
      if($GetArgs['cmd'] == "download"){
//Get file type and set it as Content Type
        $finfo = finfo_open(FILEINFO_MIME_TYPE);
        header('Content-Type: ' . finfo_file($finfo, $firmware['filename']));//application/octet-stream for binary file
        finfo_close($finfo);
//Define file size
        header('Content-Length: ' . filesize($firmware['filename']));
        readfile($firmware['filename']); //send file
      }
      exit;
    }
  }

  verify(false);
?>
