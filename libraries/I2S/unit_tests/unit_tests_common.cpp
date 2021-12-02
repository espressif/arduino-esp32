#include "unit_tests_common.h"

void send_and_print(enum msg_t msg){
  Serial1.write(msg);
  Serial.printf("Sent msg num %d = \"%s\"\n", msg, txt_msg[msg]);
}

enum msg_t read_and_print(){
  while(!Serial1.available()){
    ; // wait
  }
  int recv = Serial1.read();
  Serial.print("Received response number ");
  Serial.print(recv);
  Serial.print(" = ");
  Serial.println(txt_msg[recv]);
  return (enum msg_t)recv;
}

enum msg_t wait_and_read(){
  while(!Serial1.available()){
    ; // wait
  }
  int recv = Serial1.read();
  return (enum msg_t)recv;
}

void halt(){
  Serial.print("Halt...");
  while(1){
    ; // halt
  }
}
