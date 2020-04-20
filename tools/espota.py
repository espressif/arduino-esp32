#!/usr/bin/env python
#
# Original espota.py by Ivan Grokhotkov:
# https://gist.github.com/igrr/d35ab8446922179dc58c
#
# Modified since 2015-09-18 from Pascal Gollor (https://github.com/pgollor)
# Modified since 2015-11-09 from Hristo Gochkov (https://github.com/me-no-dev)
# Modified since 2016-01-03 from Matthew O'Gorman (https://githumb.com/mogorman)
#
# This script will push an OTA update to the ESP
# use it like: python espota.py -i <ESP_IP_address> -I <Host_IP_address> -p <ESP_port> -P <Host_port> [-a password] -f <sketch.bin>
# Or to upload SPIFFS image:
# python espota.py -i <ESP_IP_address> -I <Host_IP_address> -p <ESP_port> -P <HOST_port> [-a password] -s -f <spiffs.bin>
#
# Changes
# 2015-09-18:
# - Add option parser.
# - Add logging.
# - Send command to controller to differ between flashing and transmitting SPIFFS image.
#
# Changes
# 2015-11-09:
# - Added digest authentication
# - Enhanced error tracking and reporting
#
# Changes
# 2016-01-03:
# - Added more options to parser.
#

from __future__ import print_function
import socket
import sys
import os
import optparse
import logging
import hashlib
import random
import errno
from time import time 
import tempfile

# Commands
FLASH = 0
SPIFFS = 100
AUTH = 200
PROGRESS = False
# update_progress() : Displays or updates a console progress bar
## Accepts a float between 0 and 1. Any int will be converted to a float.
## A value under 0 represents a 'halt'.
## A value at 1 or bigger represents 100%
def update_progress(progress):
  if (PROGRESS):
    barLength = 60 # Modify this to change the length of the progress bar
    status = ""
    if isinstance(progress, int):
      progress = float(progress)
    if not isinstance(progress, float):
      progress = 0
      status = "error: progress var must be float\r\n"
    if progress < 0:
      progress = 0
      status = "Halt...\r\n"
    if progress >= 1:
      progress = 1
      status = "Done...\r\n"
    block = int(round(barLength*progress))
    text = "\rUploading: [{0}] {1}% {2}".format( "="*block + " "*(barLength-block), int(progress*100), status)
    sys.stderr.write(text)
    sys.stderr.flush()
  else:
    sys.stderr.write('.')
    sys.stderr.flush()

def serve(remoteAddr, localAddr, remotePort, localPort, password, filename, command = FLASH, md = hashlib.new('MD5'), tsurl = None, amd_type = 'MD5', tskey = None):
  # Create a TCP/IP socket
  sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
  server_address = (localAddr, localPort)
  logging.info('Starting on %s:%s', str(server_address[0]), str(server_address[1]))
  try:
    sock.bind(server_address)
    sock.listen(1)
  except:
    logging.error("Listen Failed")
    return 1

  content_size = os.path.getsize(filename)

  tsr = None
  if tsurl or tskey:
    try:
      from pyasn1.codec.der import encoder
      import rfc3161ng

      timestamper = rfc3161ng.RemoteTimestamper(tsurl,hashname='sha256',include_tsa_certificate=True)
      f = open(filename,'rb')
      tsr = timestamper(data=f.read(), return_tsr=True)
      f.close()

      tsr = encoder.encode(tsr)
      len_tsr = 0 + len(tsr)

      hdr = 'RedWax/1.00 rfc3161=%d payload=%d\n' % (len_tsr, content_size)
      content_size = content_size + len(hdr) + len_tsr

      md.update(hdr)
      md.update(tsr);

    except Exception,e:
      sys.stderr.write('rfc3161ng not installed, falling back to openssl\n')
      sys.stderr.flush()
      rqh,rqn = tempfile.mkstemp(text=True)
      rph,rpn = tempfile.mkstemp(text=True)
 
      cmd = "openssl ts -query -data '%s' -cert -sha256 -no_nonce -out '%s'" % (filename,rqn)
      os.system(cmd)
      if tsurl:
         cmd = "curl -s -H 'Content-type: application/timestamp-query' --data-binary '@{}' '{}' > '{}'".format(rqn, tsurl, rpn)
         os.system(cmd)
      else:
         tmph,tmpn = tempfile.mkstemp(text=True)
         with os.fdopen(tmph,"w") as f:
           f.write("0000\n")
         f.close()
         tsch,tscn = tempfile.mkstemp(text=True)
         with os.fdopen(tsch,"w") as f:
           f.write("""
[ tsa ]
default_tsa = tsa_config

[ tsa_config ]
serial          = {}
crypto_device   = builtin
signer_digest   = sha256    
default_policy  = 1.2
other_policies  = 1.2
digests         = sha256
accuracy        = secs:1
ess_cert_id_alg	= sha1

""".format(tmpn))
         f.close()
         cmd = "openssl ts -reply -queryfile '{}' -signer '{}' -inkey '{}' -out {} -config '{}'".format(rqn, tskey, tskey, rpn, tscn)
         print(cmd)
         os.system(cmd)

         os.unlink(tmpn)
         os.unlink(tscn)

      f = open(rpn,'rb')
      tsr = f.read()
      f.close()

      os.unlink(rqn)
      os.unlink(rpn)

      len_tsr = 0 + len(tsr)
      hdr = 'RedWax/1.00 rfc3161=%d payload=%d\n' % (len_tsr, content_size)
      content_size = content_size + len(hdr) + len_tsr

      md.update(hdr)
      md.update(tsr);

  f = open(filename,'rb')
  md.update(f.read())
  f.close()

  file_md = md.hexdigest()
  logging.info('Upload size: %d (%s: %s)', content_size, md.name, file_md)
  message = '%d %d %d %s\n' % (command, localPort, content_size, file_md)
  try:
     amd = hashlib.new(amd_type)
  except:
    sys.stderr.write('Unknown Authentication hash type\n')
    sys.stderr.flush()
    return 1
  
  if tsr != None or amd.name.upper() != 'MD5':
    message = 'RedWax/1.00 cmd=%d port=%d size=%d md=%s digest=%s' %  (command, localPort, content_size, md.name.upper(), file_md.upper())
    if amd.name.upper() != 'MD5':
      message = message + ' authmd=%s' % (md.name.upper())
    if tsr != None:
      message = message + ' rfc3161=%d' % (len(tsr))
    message = message + '\n'
    logging.debug('Header: %s' % (message.strip()))

  # Wait for a connection
  inv_trys = 0
  data = ''
  msg = 'Sending invitation to %s ' % (remoteAddr)
  sys.stderr.write(msg)
  sys.stderr.flush()
  while (inv_trys < 10):
    inv_trys += 1
    sock2 = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    remote_address = (remoteAddr, int(remotePort))
    try:
      sent = sock2.sendto(message.encode(), remote_address)
    except:
      sys.stderr.write('failed\n')
      sys.stderr.flush()
      sock2.close()
      logging.error('Host %s Not Found', remoteAddr)
      return 1
    sock2.settimeout(TIMEOUT)
    try:
      data = sock2.recv(37).decode()
      break;
    except:
      sys.stderr.write('.')
      sys.stderr.flush()
      sock2.close()
  sys.stderr.write('\n')
  sys.stderr.flush()
  if (inv_trys == 10):
    logging.error('No response from the ESP')
    return 1
  if (data != "OK"):
    if(data.startswith('AUTH')):
      nonce = data.split()[1]
      cnonce_text = '%s%u%s%s' % (filename, content_size, file_md, remoteAddr)

      amd = hashlib.new(amd_type)
      amd.update(cnonce_text.encode())
      cnonce = amd.hexdigest()

      amd = hashlib.new(amd_type)
      amd.update(password.encode())
      passmd5 = amd.hexdigest()

      result_text = '%s:%s:%s' % (passmd5 ,nonce, cnonce)

      amd = hashlib.new(amd_type)
      amd.update(result_text.encode())
      result = amd.hexdigest()

      sys.stderr.write('Authenticating...')
      sys.stderr.flush()
      message = '%d %s %s\n' % (AUTH, cnonce, result)
      sock2.sendto(message.encode(), remote_address)
      sock2.settimeout(10)
      try:
        data = sock2.recv(32).decode()
      except:
        sys.stderr.write('FAIL\n')
        logging.error('No Answer to our Authentication')
        sock2.close()
        return 1
      if (data != "OK"):
        sys.stderr.write('FAIL\n')
        logging.error('%s', data)
        sock2.close()
        sys.exit(1);
        return 1
      sys.stderr.write('OK\n')
    else:
      logging.error('Bad Answer: %s', data)
      sock2.close()
      return 1
  sock2.close()

  logging.info('Waiting for device to connect to us...')
  try:
    sock.settimeout(10)
    connection, client_address = sock.accept()
    sock.settimeout(None)
    connection.settimeout(1)
    
  except:
    logging.error('No response from device')
    sock.close()
    return 1
  logging.debug('device contacted us over TCP and is connected.')

  if tsr != None:
    connection.sendall(hdr)
    logging.debug("Send Payload Header: {}".format(hdr.strip()))

  try:
    f = open(filename, "rb")
    if (PROGRESS):
      update_progress(0)
    else:
      sys.stderr.write('Uploading')
      sys.stderr.flush()
    offset = 0

    if tsr != None:
      try:
        connection.settimeout(20)
        l=connection.sendall(tsr)
        logging.info("Send {} bytes TSR, payload is next".format(len(tsr)))
      except:
        sys.stderr.write('\n')
        logging.error('Error Uploading TSR')
        connection.close()
        f.close()
        sock.close()
        return 1

    # 10 second timeout; nonblocking
    t = time()
    lastResponseContainedOK = 0
    while time() - t < 10:
      chunk = f.read(1024)
      if not chunk: 
        break

      offset += len(chunk)
      update_progress(offset/float(content_size))
      try:
        connection.setblocking(1)
        connection.sendall(chunk)
        connection.setblocking(0)

        res = connection.recv(10)
        if res != None:
           t = time()
           lastResponseContainedOK = 'OK' in res.decode()

      except socket.error, e:
        if e.args[0] != errno.EWOULDBLOCK:
          sys.stderr.write('\n')
          logging.error('Error Uploading: {} '.format(e))
          connection.close()
          f.close()
          sock.close()
          return 1

    if lastResponseContainedOK:
      logging.info('Success')
      connection.close()
      f.close()
      sock.close()
      return 0

    sys.stderr.write('\n')
    logging.info('Waiting for result...')
    connection.setblocking(1)
    connection.settimeout(1)
    t = time()
    try:
      while time() - t < 60:
        data = connection.recv(32).decode()

        if "OK" in data:
          logging.info('Success')
          connection.close()
          f.close()
          sock.close()
          return 0;

      logging.error('Timeout from device / no OK received')
      connection.close()
      f.close()
      sock.close()
      return 1

    except e:
      logging.error('No Result!')
      connection.close()
      f.close()
      sock.close()
      return 1

  finally:
    connection.close()
    f.close()

  sock.close()
  return 1
# end serve


def parser(unparsed_args):
  parser = optparse.OptionParser(
    usage = "%prog [options]",
    description = "Transmit image over the air to the esp32 module with OTA support."
  )

  # destination ip and port
  group = optparse.OptionGroup(parser, "Destination")
  group.add_option("-i", "--ip",
    dest = "esp_ip",
    action = "store",
    help = "ESP32 IP Address.",
    default = False
  )
  group.add_option("-I", "--host_ip",
    dest = "host_ip",
    action = "store",
    help = "Host IP Address.",
    default = "0.0.0.0"
  )
  group.add_option("-p", "--port",
    dest = "esp_port",
    type = "int",
    help = "ESP32 ota Port. Default 3232",
    default = 3232
  )
  group.add_option("-P", "--host_port",
    dest = "host_port",
    type = "int",
    help = "Host server ota Port. Default random 10000-60000",
    default = random.randint(10000,60000)
  )
  parser.add_option_group(group)

  # auth
  group = optparse.OptionGroup(parser, "Authentication")
  group.add_option("-a", "--auth",
    dest = "auth",
    help = "Set authentication password.",
    action = "store",
    default = ""
  )
  group.add_option("-M", "--auth-code",
    dest = "auth",
    help = "Set authentication password in hex-digest form; i.e. the obfuscated format",
    action = "store",
    default = ""
  )
  group.add_option("-A", "--authentication-digest",
     action = "store",
     help = "Digest to use for authentication (e.g. md5(default), sha256, etc)",
     dest = 'amd',
     default = "md5"
  )
  parser.add_option_group(group)

  # image
  group = optparse.OptionGroup(parser, "Image")
  group.add_option("-f", "--file",
    dest = "image",
    help = "Image file.",
    metavar="FILE",
    default = None
  )
  group.add_option("-s", "--spiffs",
    dest = "spiffs",
    action = "store_true",
    help = "Use this option to transmit a SPIFFS image and do not flash the module.",
    default = False
  )
  parser.add_option_group(group)

  # output group
  group = optparse.OptionGroup(parser, "Output")
  group.add_option("-d", "--debug",
    dest = "debug",
    help = "Show debug output. And override loglevel with debug.",
    action = "store_true",
    default = False
  )
  group.add_option("-r", "--progress",
    dest = "progress",
    help = "Show progress output. Does not work for ArduinoIDE",
    action = "store_true",
    default = False
  )
  group.add_option("-t", "--timeout",
    dest = "timeout",
    type = "int",
    help = "Timeout to wait for the ESP32 to accept invitation",
    default = 10
  )
  parser.add_option_group(group)

  group = optparse.OptionGroup(parser, "Firmware checksum/signature")
  group.add_option("-m", "--digest",
     action = "store",
     help = "Digest to use to protect the firmware; (e.g. md5(default), sha256, etc)",
     default = "md5"
  )
  group.add_option("-S", "--sign",
     dest = 'tsurl',
     action = "store",
     help = "Sign the firmware using the rfc3161 timeserver URL provided. E.g. https://interop.redwax.eu/test/timestamp. Cannot be combined with --signing-key",
  )
  group.add_option("-K", "--signing-key",
     dest = 'tskey',
     action = "store",
     help = "Sign the firmware using the key (PEM format) provided. Cannot be combined with --sign"
  )
  group.add_option("-G", "--generate-signing-key",
     dest = 'tsgen',
     action = "store_true",
     help = "Generate a PEM signing key, requires the --signing-key to be also specified"
  )

  parser.add_option_group(group)

  (options, args) = parser.parse_args(unparsed_args)

  return options
# end parser


def main(args):
  options = parser(args)
  loglevel = logging.WARNING
  if (options.debug):
    loglevel = logging.DEBUG

  logging.basicConfig(level = loglevel, format = '%(asctime)-8s [%(levelname)s]: %(message)s', datefmt = '%H:%M:%S')
  logging.debug("Options: %s", str(options))

  # check options
  global PROGRESS
  PROGRESS = options.progress

  global TIMEOUT
  TIMEOUT = options.timeout
  
  if (options.tsgen and not options.tskey):
    logging.critical("--signing-key required when using --generate-signing-key")
    return 1

  if (options.tsgen and options.tsurl):
    logging.critical("options --signing-key and --sign (url) cannot be combined.")
    return 1

  if (options.tsgen):
    cmd = "openssl req -x509 -subj=/CN=ArduinoOTA -keyout '{}' -out '{}' -nodes -sha256 --addext 'extendedKeyUsage=critical,timeStamping' ".format(options.tskey,options.tskey)
    os.system(cmd)

    tmph,tmpn= tempfile.mkstemp()
    cmd = "openssl x509 -in '{}' -outform DER -out '{}'".format(options.tskey, tmpn)
    os.system(cmd)

    with open(tmpn,'rb') as f:
      keybytes = f.read()

    print("/* DER encoded signing certificate with (just the) public key */")
    print("#define             signing_cert_len ((size_t){})".format(len(keybytes)))
    print("const unsigned char signing_cert[{}] = {{ ".format(len(keybytes)))
    for chunk in [keybytes[i:i+16] for i in range(0, len(keybytes), 16)]:
       l = ','
       if len(chunk) != 16:
         l = ''
       print('\t{}{}'.format(','.join('0x{:02x}'.format(ord(b)) for b in chunk),l))
    print("}; // end of signing cert")

    os.unlink(tmpn)

    if (not options.esp_ip or not options.image):
       return 0
    return 1

  if (not options.esp_ip or not options.image):
    logging.critical("Not enough arguments.")
    return 1

  command = FLASH
  if (options.spiffs):
    command = SPIFFS

  md = None
  if (options.digest):
    try:
       md = hashlib.new(options.digest)
    except:
       logging.critical("Digest <%s> unknown.", options.digest)
       return 1

  return serve(options.esp_ip, options.host_ip, options.esp_port, options.host_port, options.auth, options.image, command, md, options.tsurl, options.amd, options.tskey)
# end main


if __name__ == '__main__':
  sys.exit(main(sys.argv))

