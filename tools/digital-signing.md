# Digital Signing of Over The Air (OTA) updates.

The espota.py tools contains support for signed, over the air, 
firmware updates. This works by

1)	Including a public (i.e. not secret, not sensitve)
	key in your ESP firmware.

2)	Signing your binary with the private key (which you
	keep secure). 

3)	Send the signature and the firmware to the ESP32
	using the normal OTA protocol.

4)	Have the ESP32 check the signature validity against
	its build in public key(s).

	Verify if the signature signed the digest of the
	firmware.	

	Verify if the firmware uploaded had that digest.

	(Optional) Verify that the signature is dated
	later than the current firmware compile time. This
	is to foil a 'downgrade' attack; where one tries
	to go back to an older version of the firmware 
	(that was once signed and) that is attractive as
	it has a security hole in it.

5)	And if that is the case; activate the partition
	and reboot.

The technology used for this is a signed digital timestamp
according to RFC 3161. The signature make use of a PKI
X.509 hierarchy. This allows for long lived keys, federation
and (administrattive) separation of concerns. In particular
it allows you to manage signatures over long periods
of time, even staff comes and leaves, keys need to be
recycled or are lost.

## A) I just want to play. How do I do that ?

Take the example 'SecureOTA'. 

This example has the root public key build of the public, 
redwax.eu interop server (that signs anything you offer it):

	https://interop.redwax.eu/rs/timestamp/

Compile the SecureOTA-RedWax and transfer it to the 
Arduino by Serial or by normal OTA. Enabling debug or verbose
logging will give you a blow by blow account of the process.

Once installed - you can only OTA update it with firmware
that is signed by above demo server. Now as anyone can
get anything signed - this is not particularly secure -- but
this is a good proxy for a well set up CI/CD system; where the
role of that interop.redwax.eu server would be a service
in your own environment (we'll get to why that is a good idea
later). So to install you run:

         ./espota.py -i <IP> -f .../SecureOTA.bin \
            --sign=https://interop.redwax.eu/test/timestamp

while you watch the serial output.

## B) OK - so how do I this secure, without any server noncense ?

So above example is just that; with no real security. Again
take the example 'SecureOTA' and open it.

The next step would be to generate a public/private key; by doing

          ./espota.py --signing-key=secret-signing-key.pem --generate-signing-key

this will write out the secret file to disk; but also show you 
the public (i.e. the non secret key) in an easy to include in
C format. Cut the bit from 'const unsigned char ... { .. }' and
paste it into the SecureOTA.ino example. 

Now change or add a line that says:

         signatureChecker.addTrustedCertAsDER(signing_cert, sizeof(signing_cert));

If you leave in the existing line - then the Uploader will accept both your
key and RedWax signed (insecure ones). Now tranfer this by serial or
over OTA to the ESP32 (if you still have the version of example A in the
ESP32 - then you can use the above  ./espota.py .. --sign=http.. - as the
current firmware will still accept RedWax signed uploads.

Once it is in - you can use your own key (in the file 

         secret-signing-key.pem

) to sign and upload:

         ./espota.py -i <IP> -f .../SecureOTA.bin \
              --signing-key=secret-signing-key.pem

## C) So what happens if I loose that private key.

Well then, if the firmware which is in the ESP32 only has the signing_cert
sent to signatureChecker.addTrustedCertAsDER, then it will only accept
OTA uploads with that key. And as you cannot derive the private key from
the public one (in the code, in the firmware, extratable from the ESP32),
you essentially are blocked from any OTA. It will need a serial update
to a known key pair first.

The first line defense agains this is usually to put 2 or 3 of
such keys into the Fireware (simply run the '-G/--generate command
sever times), print out some of the keys, and hope you do not loose
them or that someone else gets access to it. 

And the latter is always a bit of an issue; as you need this key
everytime you do a test, a compile, etc.

## D) That sounds a bit messy in an enterprise / serious setting.

That is indeed the case. So this is what PKI has been invented for. What
you do here is that you use a hierarchy of signatures. So suppose 
you have a hierarchy of:

         myCorp Certifcate Authority
           |
           +---- production 
           |         |
           |         +--- plant 1
           |
           +---- engineering
                     |
                     +---- development
                     |         |
                     |         +- Fred
                     |         +- Mary
                     |
                     +---- pilot-testing

Where myCorp has signed production and engineering. And production has
signed plant . With engineering having signed development and pilot
testing. And within development Fred and Mary have their own certificate.

Now a piece of firmware that has been given the myCorp certificate can
validate any of its leafs. I.e. it can accept Freds or Marys their
work from their desktop; but also be updated for production.  While it is 
also possible to limit the scope of some firmware to just accepting
firmware that has been signed by production (or at least engineering,
pilot-testing).

Now while it may be fine for Fred and Mary to have those signing keys
on their personal machines; it is generally not desirable to have such
keys on the relatively exposed CI/CD systems. And for keys that carry
a lot of power - you often want to keep those very contained and secure.

This is where the RFC3161 time servers enter into the equation (the URL
used in example A). Such servers are easy to set up (about as hard as
a webserver) and very easy to secure. It then becomes possible to
maintain most top level keys 'off-line' (on paper, on a chipcard or
in an HSM are common choises), have important keys, such as those
for production, well contained in a time-server. Yet easily accessible
for those who need regular access.

## Note

If you use RSA keys - they need to be at least 2048 bits long. MBED TLS
sensibly will reject anything shorter.

## Example output (Method A)

With logging set to 'verbose

1) normal boot

         rst:0xc (SW_CPU_RESET),boot:0x17 (SPI_FAST_FLASH_BOOT)
         configsip: 0, SPIWP:0xee
         clk_drv:0x00,q_drv:0x00,d_drv:0x00,cs0_drv:0x00,hd_drv:0x00,wp_drv:0x00
         mode:DIO, clock div:1
         load:0x3fff0018,len:4
         load:0x3fff001c,len:1044
         load:0x40078000,len:8896
         load:0x40080400,len:5816
         entry 0x400806ac
         Booting Apr 19 2020 21:55:30
         [D][WiFiGeneric.cpp:337] _eventCallback(): Event: 0 - WIFI_READY
         [D][WiFiGeneric.cpp:337] _eventCallback(): Event: 2 - STA_START
         [D][WiFiGeneric.cpp:337] _eventCallback(): Event: 4 - STA_CONNECTED
         [D][WiFiGeneric.cpp:337] _eventCallback(): Event: 7 - STA_GOT_IP
         [D][WiFiGeneric.cpp:381] _eventCallback(): STA IP: 10.11.0.193, MASK: 255.255.255.0, GW: 10.11.0.1
         [I][SecureArduinoOTA.cpp:153] begin(): OTA server at: ota-test.local:3232
         Ready
         IP address: 10.11.0.193
         
2) ESP32 up - inbound oTA rquest

         [I][SecureArduinoOTA.cpp:292] _onRxHeaderPhase(): OTA Outer Digest: MD5: D28374F44E8B358E189FAF4FB6AFB8FF
         [D][SecureUpdater.cpp:169] begin(): OTA Partition: app1
         Updating sketch
         
3) Start of signature validation

         [D][SecureUpdateProcessors.cpp:202] process_header(): RFC 3161 signed payload
         [D][SecureUpdateProcessors.cpp:231] process_header(): Processing RFC 3161 signed payload
         
         PKIStatus: 0
         idSig:   9 1.2.840.113549.1.7.2
         SignedData Version 3
         SignedData DigestAlgorithmIdentifier:algoritm OID: 2.16.840.1.101.3.4.2.1
            MD algoritm internal num: 6 SHA256
         SignedData DigestAlgorithmIdentifier: 6eContentType:  11 1.2.840.113549.1.9.16.1.4
         TSTInfo Version 1
         algoritm OID: 2.16.840.1.101.3.4.2.1
            MD algoritm internal num: 6 SHA256
         The main hash of the payload: len=32: f8baadde2339ebbed...6c31e743496043d33f1f.
         Serial 16883855617417564721
         Signature timestap: 2020-04-19 19:57:58 UTC
         Accuracy 1.0.0 (ignored 0 bytes)
         Extracted 1 certs
         No CRLs
         SID name CN=Redwax Interop Testing Root Certificate Authority 2040, O=Redwax Project
         DigestAlgorithmIdentifier:algoritm OID: 2.16.840.1.101.3.4.2.1
            MD algoritm internal num: 6 SHA256
         Signed attributes:
           1.2.840.113549.1.9.3:
            Not decoded -- skipped
           1.2.840.113549.1.9.5:
            Not decoded -- skipped
           1.2.840.113549.1.9.16.2.12:
             Signing cert hash V1 (4): ff4237eaedc05da815c24db853f0d2bfda34da5c.
           1.2.840.113549.1.9.4:
            Digest (signed section): len=32: 7bc441fcc129d9a4a90e5ed....784fb4c59f4d2216af.
         SignatureAlgorithmIdentifier:algoritm OID: 1.2.840.113549.1.1.1
            PK algoritm internal num: 1
         Signature (512 bytes): 075fc499f569997fbea2d66e7d8c6cdae8....a8912bb3f54ee91f01df8b.
            0 hash: ff4237eaedc05da815c24db853f0d2bfda34da5c.
         Found one that matched
         Indirected signing.
         Attribute Signed Data (s=1629, l=155) -- calculated digest: len=32: 69ef99b2713c41e6...099aeded0c45b6bedb.
         Signature on the Reply OK
     
     

4) Processing of the signature  

         [D][SecureUpdateProcessors.cpp:246] process_header(): Processed RFC 3161 signed payload
         [I][SecureUpdateProcessors.cpp:254] process_header(): Processing plaintext with SHA256 specified RFC3161 digest.
         [D][SecureUpdateProcessors.cpp:265] process_header(): RFC3161 signature verified.     
         [D][SecureUpdateProcessors.cpp:274] process_header(): Signatures in the trust chain:
         [D][SecureUpdateProcessors.cpp:278] process_header():    - cert. version     : 3
          - serial number     : 6F:11:B7:D8:55:D2:7D:9A:14:F3:B6:E9:15:2B:60:CA:8C:4B:E2:AA
          - issuer name       : CN=Redwax Interop Testing Root Certificate Authority 2040, O=Redwax Project
          - subject name      : CN=Redwax Interop Testing Root Certificate Authority 2040, O=Redwax Project
          - issued  on        : 2020-02-11 16:38:56
          - expires on        : 2040-02-06 16:38:56
          - signed using      : RSA with SHA1
          - RSA key size      : 2048 bits
          - basic constraints : CA=true
         
         [D][SecureUpdateProcessors.cpp:281] process_header(): Signatures in the RFC3161 wrapper:
         [D][SecureUpdateProcessors.cpp:285] process_header():    - cert. version     : 3
          - serial number     : 05
          - issuer name       : CN=Redwax Interop Testing Root Certificate Authority 2040, O=Redwax Project
          - subject name      : C=NL, ST=Zuid-Holland, L=Leiden, O=TimeServices, CN=Redwax Interop Test
          - issued  on        : 2020-02-15 20:51:52
          - expires on        : 2040-02-10 20:51:52
          - signed using      : RSA with SHA-256
          - RSA key size      : 4096 bits
          - basic constraints : CA=false
          - key usage         : Digital Signature
          - ext key usage     : Time Stamping
         
         [I][SecureUpdateProcessors.cpp:293] process_header(): RFC3161 signature on timestamp and payload digest verified.
         [D][SecureUpdateProcessors.cpp:303] process_header(): RFC3161: processing payload.
         [D][SecureUpdateProcessors.cpp:51] process_header(): Valid magic at start of flash header
         [D][SecureUpdateProcessors.cpp:362] process_end(): RFC3161 Finalizing payload digest
         [D][SecureUpdateProcessors.cpp:382] process_end(): Payload calculated SHA256 Digest 32:f8baadde2339ebbed1b3afad3d6a9ad3cee69555f6c31e743496043d33f1f
         [D][SecureUpdateProcessors.cpp:387] process_end(): RFC3161 Receveived SHA256 Digest 32:f8baadde2339ebbed1b3afad3d6a9ad3cee69555f6c31e743496043d33f1f
         [I][SecureUpdateProcessors.cpp:402] process_end():  RFC3161 Payload digest matches signed digest.
         [D][SecureUpdater.cpp:400] end(): Reporting an OK back up the chain
         [D][SecureArduinoOTA.cpp:561] _runUpdate(): OTA Outer Digest matched.
         [D][SecureUpdateProcessors.cpp:131] reset(): RESET
         [D][SecureUpdater.cpp:359] activate(): RAW SHA256 Digest 361ab6322fa9e7a7bb23818d839e1bddafdf4735426edd297aedb9f6202bae

5) Final phase - ESP 32 partition activated and rebooted.

         [D][SecureUpdater.cpp:211] abort(): Aborted.
         [D][SecureUpdateProcessors.cpp:131] reset(): RESET
         [D][WiFiGeneric.cpp:337] _eventCallback(): Event: 3 - STA_STOP
         [D][WiFiGeneric.cpp:337] _eventCallback(): Event: 3 - STA_STOP
         ets Jun  8 2016 00:22:57
         
         rst:0xc (SW_CPU_RESET),boot:0x17 (SPI_FAST_FLASH_BOOT)
         configsip: 0, SPIWP:0xee
         clk_drv:0x00,q_drv:0x00,d_drv:0x00,cs0_drv:0x00,hd_drv:0x00,wp_drv:0x00
         mode:DIO, clock div:1
         load:0x3fff0018,len:4
         load:0x3fff001c,len:1044
         load:0x40078000,len:8896
         load:0x40080400,len:5816
         entry 0x400806ac
         Booting Apr 19 2020 21:55:30
         Ready
         IP address: 10.11.0.193


