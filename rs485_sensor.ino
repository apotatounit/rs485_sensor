// #include <Arduino.h>
#include <SoftwareSerial.h>


SoftwareSerial mySerial(D6, D7); // RX, TX

unsigned long lastSent = 0;
uint period = 1000;

byte temp_request[] = {0x02, 0x03, 0x00, 0x01, 0x00, 0x01, 0xd5, 0xca};
byte set_slave_id_request[] = {0x02, 0x06, 0x07, 0xD0, 0x00, 0x02, 0x08, 0x86};

// Function to calculate the CRC
uint16_t calculateCRC(byte* data, uint8_t length) {
  uint16_t crc = 0xFFFF;
  for (uint8_t i = 0; i < length; i++) {
    crc ^= (uint16_t)data[i];
    for (uint8_t j = 0; j < 8; j++) {
      if (crc & 0x0001) {
        crc >>= 1;
        crc ^= 0xA001;
      } else {
        crc >>= 1;
      }
    }
  }
  return crc;
}

// Address, Function code, Start Address Hi, Start Address Lo, ID_0, ID_1, Error_Lo, Error_hi

byte temp_req_sz = 8;

byte address = 2;
byte function_code = 3;
byte state = 0;

byte data_len = 0;
byte data[] = { 0, 0};
byte data_counter = 0;
byte checksum_arr[] = { 0, 0 };
byte checksum_counter = 0;

void setup() {
  // start serial port at 9600 bps:
  Serial.begin(9600);
  while (!Serial) { }
  Serial.println("Goodnight moon!");

  mySerial.begin(4800);
  pinMode(LED_BUILTIN, OUTPUT);

  // RequestSetSlaveId();
}

void RequestSetSlaveId() {
  Serial.println("requesting set slave id...");
  // Serial.print("tx: ");
  // Serial.println(set_slave_id_request);
  mySerial.write(set_slave_id_request, temp_req_sz);
  Serial.println("requested.");
  Serial.println("receiving confirmation...");

  String raw_msg_data = "";
  while (mySerial.available() > 0) {
    byte inByte = mySerial.read();
    if (inByte < 0x10)
      raw_msg_data += String("0");
    raw_msg_data += String(inByte, HEX);
  }
  Serial.print ("rx: ");
  Serial.println(raw_msg_data);
    
}

void loop() {
  if (millis() - lastSent > period) {
    mySerial.write(temp_request, temp_req_sz);

  uint16_t crc = calculateCRC(temp_request, sizeof(temp_request) - 2);
  byte crc_low = crc & 0xFF;
  byte crc_high = (crc >> 8) & 0xFF;
  // Use crc_low and crc_high as the last two bytes of the array
  Serial.print("Old CRC Low: ");
  Serial.println(temp_request[sizeof(temp_request) - 2], HEX);
  Serial.print("Old CRC High: ");
  Serial.println(temp_request[sizeof(temp_request) - 1], HEX);

  temp_request[sizeof(temp_request) - 2] = crc_low;
  temp_request[sizeof(temp_request) - 1] = crc_high;

  Serial.print("New CRC Low: ");
  Serial.println(crc_low, HEX);
  Serial.print("New CRC High: ");
  Serial.println(crc_high, HEX);

    lastSent = millis();
    digitalWrite(LED_BUILTIN, LOW);
    delay(100);
    digitalWrite(LED_BUILTIN, HIGH);
    delay(100);
  }

  ReadSearchSerialResponse();
  ParseTemp();
}

void ParseTemp()
{
  if (state == 5) {
    float temp = (data[1] + 255 * data[0]) / 10.0;
    Serial.print(temp);
    Serial.println(" °C");
    digitalWrite(LED_BUILTIN, LOW);
    delay(200);
    digitalWrite(LED_BUILTIN, HIGH);
    delay(100);

    state = 0;
  }
}

void ReadSearchSerialResponse()
{
  String raw_msg_data = "";
  while (mySerial.available() > 0) {
    byte inByte = mySerial.read();
    if (inByte < 0x10)
      raw_msg_data += String("0");
    raw_msg_data += String(inByte, HEX);
    Serial.print ("parsing response: ");
    Serial.println(inByte, HEX);
    switch (state) {
      case 0:
        Serial.println("case 0");
        if (inByte == address)
        {
          state = 1;
          checksum_counter = 0;
          data_counter = 0;
          raw_msg_data = String("0");
          raw_msg_data += String(inByte);
        }
        else
          state = 0;
        break;
      case 1:
        Serial.println("case 1");
        if (inByte == function_code) {
          state = 2;
        }
        else
          state = 0;
        break;
      case 2:
        Serial.println("case 2");
        data_len = inByte;
        state = 3;
        break;
      case 3:
        Serial.println("case 3");
        data[data_counter++] = inByte;
        if (data_counter == data_len) {
          state = 4;
        }
        break;
      case 4:
        Serial.println("case 4");
        checksum_arr[checksum_counter++] = inByte;
        if (checksum_counter == 2)
          state = 5;
        break;
    }
    if (state == 5) {
        Serial.println("case 5");
        Serial.print("received data: ");
        Serial.println(raw_msg_data);
        break;
    }
  }
}

// Compute the MODBUS RTU CRC
String ModRTU_CRC(String raw_msg_data) {
  //Calc raw_msg_data length
  byte raw_msg_data_byte[raw_msg_data.length()/2];
  //Convert the raw_msg_data to a byte array raw_msg_data
  for (int i = 0; i < raw_msg_data.length() / 2; i++) {
    raw_msg_data_byte[i] = StrtoByte(raw_msg_data.substring(2 * i, 2 * i + 2));
  }

  //Calc the raw_msg_data_byte CRC code
  uint16_t crc = 0xFFFF;
  String crc_string = "";
  for (int pos = 0; pos < raw_msg_data.length()/2; pos++) {
    crc ^= (uint16_t)raw_msg_data_byte[pos];          // XOR byte into least sig. byte of crc
    for (int i = 8; i != 0; i--) {    // Loop over each bit
      if ((crc & 0x0001) != 0) {      // If the LSB is set
        crc >>= 1;                    // Shift right and XOR 0xA001
        crc ^= 0xA001;
      }
      else                            // Else LSB is not set
        crc >>= 1;                    // Just shift right
    }
  }
  // Note, this number has low and high bytes swapped, so use it accordingly (or swap bytes)

  //Become crc byte to a capital letter String 
  crc_string = String(crc, HEX);
  crc_string.toUpperCase();
  
  //The crc should be like XXYY. Add zeros if need it
  if(crc_string.length() == 1){
    crc_string = "000" + crc_string;
  }else if(crc_string.length() == 2){
    crc_string = "00" + crc_string;
  }else if(crc_string.length() == 3){
    crc_string = "0" + crc_string;
  }else{
    //OK
  }

  //Invert the byte positions
  crc_string = crc_string.substring(2, 4) + crc_string.substring(0, 2);
  return crc_string;  
}

//String to byte --> Example: String = "C4" --> byte = {0xC4}
byte StrtoByte (String str_value){
  char char_buff[3];
  str_value.toCharArray(char_buff, 3);
  byte byte_value = strtoul(char_buff, NULL, 16);
  return byte_value;  
}