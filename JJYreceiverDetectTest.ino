#include <Wire.h>
#include <stdio.h>

#define PIN 14
#define LED 25
#define LCDaddr 0x3e    //  = 0x7C >> 1

volatile boolean flag = false;

int8_t d_year, d_week, d_month, d_day, d_hour, d_min;
const byte month_day[]={0,31,28,31,30,31,30,31,31,30,31,30,31};
uint8_t hh, mm, MM, DD, YY;

uint8_t ss;
bool  markerCheckOk, MparityCheckOk, HparityCheckOk;
bool  firstLoop = true;
char  buff[10];

uint8_t markerOkCount;
bool  P1markerOk, P2markerOk;

void IRAM_ATTR jjysignaldetect() {// GPIO割り込みにより呼ばれるルーチン
  if (!flag) {
    flag = true;//JJYパルスが発生したことをメインループに知らせるフラグ
  }
}

//********* LCD関係 ********* 
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
  LCD_cmd(0x70);//  Power etc    0x56=B01010110 (Boost, const=B10)
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

  // JJYパルスをGPIO割り込みで検出するための設定
  attachInterrupt(PIN, jjysignaldetect, RISING);
}


int8_t get_code(void) {
    
  int8_t scanbit;
  int8_t ret_code;
  bool scan[4];

      while (!flag) {//JJYパルス検出待ち
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
      Serial.printf("%02d:%02d:%02d ", d_hour, d_min, ss);
      Serial.printf(" (%02d/%02d/%02d ", YY, MM, DD);
      Serial.printf("%02d:%02d:%02d)\n", hh, mm, ss);

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
  uint8_t h_parity, m_parity, PA1, PA2;

  markerOkCount = 0;

  //*** 分のデコード はじめ ***********************
  for (int8_t i = 0; i < 8; i++) {
    n = get_code();
    codeOk = (n == 0 || n == 1)? true : false;
    bitcode[i] = n;
  }

  if(codeOk) {
    d_min = bitcode[0]*40 + bitcode[1]*20 + bitcode[2]*10
          + bitcode[4]*8  + bitcode[5]*4  + bitcode[6]*2  + bitcode[7];

    m_parity = (bitcode[0]+bitcode[1]+bitcode[2]+bitcode[4]+bitcode[5]+bitcode[6]+bitcode[7]) % 2;
  }

  if (get_code() == 2){
    Serial.println("Position Marker P1 detected.");
    P1markerOk = true;
    markerOkCount++;
  }
  else {
    Serial.println("Failed to read Position Maker P1");
    P1markerOk = false;
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

    h_parity = (bitcode[2]+bitcode[3]+bitcode[5]+bitcode[6]+bitcode[7]+bitcode[8]) % 2;
  }

  if (get_code() == 2){
    Serial.println("Position Marker P2 detected.");
    P2markerOk = true;
    markerOkCount++;
  }
  else {
    Serial.println("Failed to read Position Maker P2");
    P2markerOk = false;
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
    markerOkCount++;
  }
  else {
    Serial.println("Failed to read Position Maker P3");
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

    PA1 = bitcode[6];// 時パリティビット
    PA2 = bitcode[7];// 分パリティビット
  }

  if (get_code() == 2){
    Serial.println("Position Marker P4 detected.");
    markerOkCount++;
  }
  else {
    Serial.println("Failed to read Position Maker P4");
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
  }
  // 曜日のデコードおわり
  
  Serial.println("End of time code decording sequence.");

  markerCheckOk = (markerOkCount > 1)? true : false;//ポジションマーカー検出が2回以上できていればOKとする。

  if (get_code() == 2){
    Serial.println("Marker M detected.");
  }
  else {
    Serial.println("Failed to read Position Maker M");
  }

  if (PA1 == h_parity){
    Serial.println("Hour parity check OK.");
    HparityCheckOk = true;
  }else {
    Serial.println("Hour parity check NG!!!!");
    HparityCheckOk = false;
  }

  if (PA2 == m_parity){
    Serial.println("Minute parity check OK.");
    MparityCheckOk = true;
  }else {
    Serial.println("Minute parity check NG!!!!");
    MparityCheckOk = false;
  }

}

void LCD_update() {
    // LCDに年/月/日と時:分の表示
    LCD_cursor(0,0);
    sprintf(buff, "%02d/%02d/%02d", YY, MM, DD);
    LCD_print(buff);
    
    LCD_cursor(0,1);
    sprintf(buff, "%02d:%02d:", hh, mm);
    LCD_print(buff);
}

void InternalClockCount(){
      mm++;
      if( mm == 60 ){
        mm = 0;
        hh++;

        if( hh == 24 ){
          hh = 0;
          DD++;

          if( DD > month_day[MM] ){
            DD = 1;
            MM++;

            if( MM == 13 ){
            MM = 1;
            YY++;
            }
          }
        }
      }
}

void loop() {
  int8_t m, p;
  int8_t sec, min = -1, hur;
  bool YYdecodeOk, MMdecodeOk, DDdecodeOk, hhdecodeOk;
  static int8_t YYdecodeOkCount = 0;
  static int8_t MMdecodeOkCount = 0;
  static int8_t DDdecodeOkCount = 0;
  static int8_t hhdecodeOkCount = 0;
  static int8_t YYp, MMp, DDp, hhp, mmp;
  
  //2回連続マーカーの検出ループ
  do {
    Serial.println("Looking for Marker");
    p = m;
    m = get_code();

    if( ss == 00 ){
      InternalClockCount();
      LCD_update();
    }
  }while(p * m != 4);// マーカー(2)が２回続くまで繰り返す。


  Serial.println("2-markers detected!!\n");
  ss = 0;
  InternalClockCount();



  do {//デコードを実行するループ
      //ポジションマーカーをすべて正しく検出できている間は繰り返す。
      //もし、一つでも検出できていなかったらループを止めてマーカ検出からやり直す。

    LCD_update();
    //デコード実行（60秒間のスキャンとデコード）
    decode();

Serial.printf("Previous decode: %d/%02d/%02d %02d:%02d\n",YYp,MMp,DDp,hhp,mmp);
Serial.printf("Current  decode: %d/%02d/%02d %02d:%02d\n",d_year,d_month,d_day,d_hour,d_min);

    // Yearのデコード結果チェック
    if( d_year == YYp ){
      YYdecodeOkCount = (YYdecodeOkCount < 2)? YYdecodeOkCount + 1 : 2;
    }else {
      YYdecodeOkCount = (YYdecodeOkCount > 0)? YYdecodeOkCount - 1 : 0;
    }

    // Monthのデコード結果チェック
    if( d_month == MMp ){
      MMdecodeOkCount = (MMdecodeOkCount < 2)? MMdecodeOkCount + 1 : 2;
    }else {
      MMdecodeOkCount = (MMdecodeOkCount > 0)? MMdecodeOkCount - 1 : 0;
    }

    // Dayのデコード結果チェック
    if( d_day == DDp ){
      DDdecodeOkCount = (DDdecodeOkCount < 2)? DDdecodeOkCount + 1 : 2;
    }else {
      DDdecodeOkCount = (DDdecodeOkCount > 0)? DDdecodeOkCount - 1 : 0;
    }
    
    // hourのデコード結果のチェック（前回デコード結果との比較）
    if( d_hour == hhp ){
        //Serial.println("Successive decode OK.\n");
        hhdecodeOkCount = (hhdecodeOkCount < 2)? hhdecodeOkCount + 1 : 2;// チェックOKのカウント数の上限を２にする
    }else {
        //Serial.println("Successive decode Failed.\n");
        hhdecodeOkCount = (hhdecodeOkCount > 0)? hhdecodeOkCount - 1 : 0;
    }

    YYdecodeOk = (YYdecodeOkCount > 1)? true : false;
    MMdecodeOk = (MMdecodeOkCount > 1)? true : false;
    DDdecodeOk = (DDdecodeOkCount > 1)? true : false;
    hhdecodeOk = (hhdecodeOkCount > 1)? true : false;


    // 今回デコード結果を保存
    YYp = d_year;
    MMp = d_month;
    DDp = d_day;
    hhp = d_hour;
    mmp = d_min;


    //とりあえず内部カウント時計をインクリメント
    InternalClockCount();


    //デコードチェックに応じてデコード結果を内部カウント時計に反映する
    if (MparityCheckOk && P1markerOk){//分デコード結果を反映
      mm = d_min + 1;
      if( mm == 60 ){
        mm = 0;
        hh = d_hour + 1;

        if( hh == 24 ){
          hh = 0;
          DD++;

          if( DD > month_day[MM] ){
            DD = 1;
            MM++;

            if( MM == 13 ){
              MM = 1;
              YY++;
            }
          }
        }
      }
    }

    if( YYdecodeOk ){//Yearデコード結果の反映。
        YY = d_year;
    }

    if( MMdecodeOk ){//Monthデコード結果の反映。月をまたぐときは反映すべきでないが
        MM = d_month;
    }

    if( DDdecodeOk && (hh != 0 || mm != 0) ){ //Dayデコード結果の反映。日をまたぐときは反映しない。
        DD = d_day;
    }

    if( hhdecodeOk && HparityCheckOk && P2markerOk && (mm != 0) ){//時デコード結果の反映。毎正時のときは反映しない
        hh = d_hour;
    }
    

    Serial.printf("******* %d/%02d/%02d ", 2000 + YY, MM, DD);
    Serial.printf("%02d:%02d(%d %d %d %d)\n", hh, mm, YYdecodeOkCount, MMdecodeOkCount, DDdecodeOkCount, hhdecodeOkCount);


    
  }while(markerCheckOk);
  LCD_update();
  delay(1);
}
