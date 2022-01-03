#include <Wire.h>
#include <stdio.h>

#define PIN 14
#define LED 25
#define LCDaddr 0x3e    //  = 0x7C >> 1

volatile boolean flag = false;


int8_t d_year, d_week, d_month, d_day, d_hour, d_min;
const byte month_day[]={0,31,28,31,30,31,30,31,31,30,31,30,31};

uint8_t ss;
bool  markerCheckOk;
bool  firstLoop = true;
char buff[10];


void IRAM_ATTR jjysignaldetect() {
  if (!flag) {
    flag = true;//割り込みが発生したことをメインループに知らせるフラグ
  }
}

//********* LCD関係
void LCD_cmd(byte cmd) {
  //  Write command = {LCDaddr, 0x00, cmd}
  Wire.beginTransmission(LCDaddr);
  Wire.write(0x00);
  Wire.write(cmd);
  Wire.endTransmission();
  delay(2);
}

void LCD_data(byte dat) {
  //  Write data = {LCDaddr, 0x40, dat}
  Wire.beginTransmission(LCDaddr);
  Wire.write(0x40);
  Wire.write(dat);
  Wire.endTransmission();
  delay(1);
}

void LCD_init() {
  delay(40);
  //  Function set 0x39=B00111001 (extension-mode)
  LCD_cmd(0x39);//  Internal OSC 0x14=B00010100 (BS=0, FR=4)
  LCD_cmd(0x14);//  Contrast set 0x72=B01110010 (level=2)
  LCD_cmd(0x72);//  Power etc    0x56=B01010110 (Boost, const=B10)
  LCD_cmd(0x56);//  Follower     0x6c=B01101100 (on, amp=B100)
  LCD_cmd(0x6c);//  Function set 0x38=B00111001 (normal-mode)
  LCD_cmd(0x38);//  clear display
  LCD_cmd(0x01);//  display on   0x0c=B00001100 (On, no-cursor, no-blink)
  LCD_cmd(0x0c);
}

void LCDclear() {
  //  画面クリア
  LCD_cmd(0x01);
}

void LCD_cursor(int x, int y) {
  //  カーソルを x 行 y 字目に移動
  if (y == 0) {
    LCD_cmd(0x80 + x);
  }
  else {
    LCD_cmd(0xc0 + x);
  }
}

void putch(byte ch){
  LCD_data(ch);
}

void LCD_print(char *str) {
  //  カーソル位置に文字列を表示（表示後にカーソルは移動）
  for (int i = 0; i < strlen(str); i++) {
    LCD_data(str[i]);
  }
}









void setup() {
  pinMode(PIN, INPUT);
  pinMode(LED, OUTPUT);
  
  Serial.begin(115200);
  delay(200);

  Wire.begin();
  LCD_init();
  LCD_cursor(0,0);
  LCD_print("LCD init");


  attachInterrupt(PIN, jjysignaldetect, RISING);
}


int8_t get_code(void) {
    
  int8_t scanbit;
  int8_t ret_code;
  bool scan[4];

    
  while (!flag) {//ビットスキャン開始待ち
  }

    // スキャン開始
      digitalWrite(LED,1);
      scanbit = 0;
      
      delay(350);
      scanbit += digitalRead(PIN); //350msec

      scanbit <<= 1;
      delay(100);
      scanbit += digitalRead(PIN); //450msec

      scanbit <<= 1;
      delay(100);
      scanbit += digitalRead(PIN); //550msec
  
      scanbit <<= 1;
      delay(100);
      scanbit += digitalRead(PIN); //650msec

      digitalWrite(LED, 0);
      delay(250);                  //850msec
      

      Serial.printf("scanbit=%x, ", scanbit);

      flag = false;//処理が終わったらフラグをリセットし、次の割り込み発生を待つ

      switch( scanbit ) {// スキャンしたビット列パターンからH/L/Mを判定する。
        case 0:  // 0000
          ret_code = 2;//マーカー
          break;
        case 0x4:// 0100
        case 0x8:// 1000
        case 0xA:// 1010
        case 0xC:// 1100(理想パターン)
        case 0xE:// 1110
          ret_code = 1;// H
          break;
        case 0x7:// 0111
        case 0x9:// 1001
        case 0xB:// 1011
        case 0xD:// 1101
        case 0xF:// 1111(理想パターン)
          ret_code = 0;// L
        break;
        default:
          ret_code = 3;// エラーコード
          break;
      }
      Serial.printf("*** CODE=%d\n", ret_code);

      ss++;
      ss %= 60;
      Serial.printf("%d/%02d/%02d ", d_year+2000, d_month, d_day);
      Serial.printf("%02d:%02d:%02d\n", d_hour, d_min, ss);

      LCD_cursor(6,1);
      sprintf(buff, "%02d", ss);
      LCD_print(buff);
      
  return(ret_code);
}


void decode() {
  uint16_t longDayCount, dayCount;
  int8_t  bitcount = 0;
  int8_t  bitcode[10];
  uint8_t n;
  bool  codeOk, longDayOk;

  //*** 分のデコード はじめ ***********************
  for (int8_t i = 0; i < 8; i++) {
    n = get_code();
    codeOk = (n == 0 || n == 1)? true : false;
    bitcode[i] = n;
  }

  if(codeOk) {
    d_min = bitcode[0]*40 + bitcode[1]*20 + bitcode[2]*10
          + bitcode[4]*8  + bitcode[5]*4  + bitcode[6]*2  + bitcode[7];
  }

  if (get_code() == 2){
    Serial.println("Position Marker P1 detected.");
  }
  else {
    Serial.println("Failed to read Position Maker P1");
    markerCheckOk = false;
  }
  //*** 分のデコード おわり
  


  //*** 時のデコード はじめ ***********************
  for (int8_t i = 0; i < 9; i++) {
    n = get_code();
    codeOk = (n == 0 || n == 1)? true : false;
    bitcode[i] = n;
  }

  if(codeOk) {
    d_hour  = bitcode[2]*20 + bitcode[3]*10
            + bitcode[5]*8  + bitcode[6]*4  + bitcode[7]*2  + bitcode[8];
  }

  if (get_code() == 2){
    Serial.println("Position Marker P2 detected.");
  }
  else {
    Serial.println("Failed to read Position Maker P2");
    markerCheckOk = false;
  }
  //*** 時のデコード おわり




  // 通算日数（前半）のデコード はじめ ***********************
  for (int8_t i = 0; i < 9; i++) {
    n = get_code();
    codeOk = (n == 0 || n == 1)? true : false;
    bitcode[i] = n;
  }

  if(codeOk) {
    longDayCount  = bitcode[2]*200 + bitcode[3]*100
                  + bitcode[5]*80  + bitcode[6]*40  + bitcode[7]*20  + bitcode[8]*10;
    longDayOk = true;
  }

  if (get_code() == 2){
    Serial.println("Position Marker P3 detected.");
  }
  else {
    Serial.println("Failed to read Position Maker P3");
    markerCheckOk = false;
  }
  // 通算日数（前半）のデコード おわり



  // 通算日数（後半）のデコード はじめ ***********************
  for (int8_t i = 0; i < 9; i++) {
    n = get_code();
    codeOk = (n == 0 || n == 1)? true : false;
    bitcode[i] = n;
  }

  if(codeOk && longDayOk) {
    dayCount = bitcode[0]*8 + bitcode[1]*4 + bitcode[2]*2 + bitcode[3];
    dayCount += longDayCount;
  }

  if (get_code() == 2){
    Serial.println("Position Marker P4 detected.");
  }
  else {
    Serial.println("Failed to read Position Maker P4");
    markerCheckOk = false;
  }
  // 通算日数（後半）のデコード おわり

  //通算日数から月、日の算出
  d_month = 0;
  do{
    dayCount -= month_day[d_month];//dayCountから経過月の日数を減算
    d_month++;
  } while( dayCount > month_day[d_month]);//最後の経過月に到達するまで
  d_day = dayCount;
  

  // 年のデコードはじめ  ***********************
  for (int8_t i = 0; i < 9; i++) {
    n = get_code();
    codeOk = (n == 0 || n == 1)? true : false;
    bitcode[i] = n;
  }

  if(codeOk) {
    d_year  = bitcode[1]*80 + bitcode[2]*40 + bitcode[3]*20
            + bitcode[4]*10 + bitcode[5]*8  + bitcode[6]*4
            + bitcode[7]*2  + bitcode[8];
  }

  if (get_code() == 2){
    Serial.println("Position Marker P5 detected.");
  }
  else {
    Serial.println("Failed to read Position Maker P5");
    markerCheckOk = false;
  }
   // 年のデコードおわり

   


  // 曜日のデコードはじめ  ***********************
  for (int8_t i = 0; i < 9; i++) {
    n = get_code();
    codeOk = (n == 0 || n == 1)? true : false;
    bitcode[i] = n;
  }

  if(codeOk) {
    d_week  = bitcode[0]*4 + bitcode[1]*2 + bitcode[2];
  }
  
  if (get_code() == 2){
    Serial.println("Position Marker P0 detected.");
  }
  else {
    Serial.println("Failed to read Position Maker P0");
    markerCheckOk = false;
  }
  // 曜日のデコードおわり
  Serial.println("End of time code decording sequence.");



  if (get_code() == 2){
    Serial.println("Marker M detected.");
  }
  else {
    Serial.println("Failed to read Position Maker M");
  }



}


void loop() {
  int8_t m, p;
  int8_t sec, min = -1, hur;
  bool decodeOk;
  static int8_t decodeOkCount = 0;

  static uint8_t hh, mm, MM, DD, YY;
  static uint8_t hhp, mmp, MMp, DDp, YYp;
  
  //2回連続マーカーの検出
  do {
    Serial.println("Looking for Marker");
    p = m;
    m = get_code();
  }while(p * m != 4);// マーカー(2)が２回続くまで繰り返す。
  Serial.println("2-markers detected!!\n");
  ss = 0;
  markerCheckOk = true;


  do {//ポジションマーカーをすべて正しく検出できている間は繰り返すループ
      //もし、一つでも検出できていなかったらマーカ検出からやり直す。
   

    //デコード実行（60秒間のスキャンとデコード）
    decode();

Serial.printf("Previous decode: %d/%02d/%02d %02d:%02d\n",YYp,MMp,DDp,hhp,mmp);
Serial.printf("Current  decode: %d/%02d/%02d %02d:%02d\n",d_year,d_month,d_day,d_hour,d_min);



    
    //デコード結果のチェック（前回デコード結果との比較）
    if( (d_year == YYp) && (d_month == MMp) && (d_day == DDp) && (d_hour == hhp) ){
        Serial.println("Successive decode OK.\n");
        //decodeOkCount++;
        decodeOkCount = (decodeOkCount < 2)? decodeOkCount + 1 : 2;// チェックOKのカウント数の上限を２にする
    }else {
        Serial.println("Successive decode Failed.\n");
        decodeOkCount = (decodeOkCount > 0)? decodeOkCount - 1 : 0;
    }

    decodeOk = (decodeOkCount > 1)? true : false;

    YYp = d_year;
    MMp = d_month;
    DDp = d_day;
    hhp = d_hour;
    mmp = d_min;

     
    if (markerCheckOk && decodeOk) {//デコードOKの場合、デコード結果を反映
      mm = d_min + 1;
      hh = d_hour;
      DD = d_day;
      MM = d_month;
      YY = d_year;
    } else {            //デコードNGの場合、前回時刻をインクリメント
      mm++;
      if( mm == 60 ){
        mm = 0;
        hh++;
      }
      if( hh == 24 ){
        hh = 0;
        DD++;
      }
      if( DD > month_day[MM] ){
        DD = 1;
        MM++;
      }
      if( MM == 13 ){
        MM = 1;
        YY++;
      }
    }

    Serial.printf("******* %d/%02d/%02d ", 2000 + YY, MM, DD);
    Serial.printf("%02d:%02d(%d)\n", hh, mm, decodeOkCount);

    LCD_cursor(0,0);
    sprintf(buff, "%02d/%02d/%02d", YY, MM, DD);
    LCD_print(buff);
    
    LCD_cursor(0,1);
    sprintf(buff, "%02d:%02d:", hh, mm);
    LCD_print(buff);
    
  }while(markerCheckOk);
  delay(1);
}
