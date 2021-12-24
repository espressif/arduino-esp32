WiFiClientSecure
================

The WiFiClientSecure class implements support for secure connections using TLS (SSL).
It inherits from WiFiClient and thus implements a superset of that class' interface.
There are three ways to establish a secure connection using the WiFiClientSecure class:
using a root certificate authority (CA) cert, using a root CA cert plus a client cert and key,
and using a pre-shared key (PSK).

Using a root certificate authority cert
---------------------------------------
This method authenticates the server and negotiates an encrypted connection.
It is the same functionality as implemented in your web browser when you connect to HTTPS sites.

If you are accessing your own server:
- Generate a root certificate for your own certificate authority
- Generate a cert & private key using your root certificate ("self-signed cert") for your server

If you are accessing a public server:
- Obtain the cert of the public CA that signed that server's cert
Then:
- In WiFiClientSecure use setCACert (or the appropriate connect method) to set the root cert of your
  CA or of the public CA
- When WiFiClientSecure connects to the target server it uses the CA cert to verify the certificate
  presented by the server, and then negotiates encryption for the connection

Please see the WiFiClientSecure example.

Using a root CA cert and client cert/keys
-----------------------------------------
This method authenticates the server and additionally also authenticates
the client to the server, then negotiates an encrypted connection.

- Follow steps above
- Using your root CA generate cert/key for your client
- Register the keys with the server you will be accessing so the server can authenticate your client
- In WiFiClientSecure use setCACert (or the appropriate connect method) to set the root cert of your
  CA or of the public CA, this is used to authenticate the server
- In WiFiClientSecure use setCertificate, and setPrivateKey (or the appropriate connect method) to
  set your client's cert & key, this will be used to authenticate your client to the server
- When WiFiClientSecure connects to the target server it uses the CA cert to verify the certificate
  presented by the server, it will use the cert/key to authenticate your client to the server, and
  it will then negotiate encryption for the connection

Using Pre-Shared Keys (PSK)
---------------------------

TLS supports authentication and encryption using a pre-shared key (i.e. a key that both client and
server know) as an alternative to the public key cryptography commonly used on the web for HTTPS.
PSK is starting to be used for MQTT, e.g. in mosquitto, to simplify the set-up and avoid having to
go through the whole CA, cert, and private key process.

A pre-shared key is a binary string of up to 32 bytes and is commonly represented in hex form. In
addition to the key, clients can also present an id and typically the server allows a different key
to be associated with each client id. In effect this is very similar to username and password pairs,
except that unlike a password the key is not directly transmitted to the server, thus a connection to a
malicious server does not divulge the password. Plus the server is also authenticated to the client.

To use PSK:
- Generate a random hex string (generating an MD5 or SHA for some file is one way to do this)
- Come up with a string id for your client and configure your server to accept the id/key pair
- In WiFiClientSecure use setPreSharedKey (or the appropriate connect method) to
  set the id/key combo
- When WiFiClientSecure connects to the target server it uses the id/key combo to authenticate the
  server (it must prove that it has the key too), authenticate the client and then negotiate
  encryption for the connection

Please see the WiFiClientPSK example.
