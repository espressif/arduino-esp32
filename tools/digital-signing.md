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
takethe example 'SecureOTA' and open it.

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
       |         +--- product 1
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

