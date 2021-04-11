#################################################################################################################################################################################
#Certificate generator for TLS encryption																	#
#################################################################################################################################################################################
openssl req -new -x509 -days 365 -extensions v3_ca -keyout ca.key -out ca.crt -passout pass:1234 -subj '/CN=TrustedCA.net'
#If you generating self-signed certificates the CN can be anything

openssl genrsa -out mosquitto.key 2048
openssl req -out mosquitto.csr -key mosquitto.key -new -subj '/CN=locsol.dynamic-dns.net'
openssl x509 -req -in mosquitto.csr -CA ca.crt -CAkey ca.key -CAcreateserial -out mosquitto.crt -days 365 -passin file:PEMpassphrase.pass
#Mostly the client verifies the adress of the mosquitto server, so its necessary to set the CN to the correct adress (eg. yourserver.com)!!!


#################################################################################################################################################################################
#These certificates are only needed if the mosquitto broker requires a certificate for client autentithication (require_certificate is set to true in mosquitto config).      	#
#################################################################################################################################################################################
openssl genrsa -out esp.key 2048
openssl req -out esp.csr -key esp.key -new -subj '/CN=localhost'
openssl x509 -req -in esp.csr -CA ca.crt -CAkey ca.key -CAcreateserial -out esp.crt -days 365 -passin pass:1234
#If the server (mosquitto) identifies the clients based on CN key, its necessary to set it to the correct value, else it can be blank.

#################################################################################################################################################################################
#For ESP32. Formatting for source code. The output is the esp_certificates.c                                                                                                    #
#################################################################################################################################################################################
echo -en "#include \"esp_certificates.h\"\n\nconst char CA_cert[] = \\\\\n" > esp_certificates.c;
sed -z '0,/-/s//"-/;s/\n/\\n" \\\n"/g;s/\\n" \\\n"$/";\n\n/g' ca.crt >> esp_certificates.c     #replace first occurance of - with "-  ;  all newlnies with \n " \ \n", except last newline where just add ;"
echo "const char ESP_CA_cert[] = \\" >> esp_certificates.c;
sed -z '0,/-/s//"-/;s/\n/\\n" \\\n"/g;s/\\n" \\\n"$/";\n\n/g' esp.crt >> esp_certificates.c     #replace first occurance of - with "-  ;  all newlnies with \n " \ \n", except last newline where just add ;"
echo "const char ESP_RSA_key[] = \\" >> esp_certificates.c;
sed -z '0,/-/s//"-/;s/\n/\\n" \\\n"/g;s/\\n" \\\n"$/";/g' esp.key >> esp_certificates.c     #replace first occurance of - with "-  ;  all newlnies with \n " \ \n", except last newline where just add ;"
