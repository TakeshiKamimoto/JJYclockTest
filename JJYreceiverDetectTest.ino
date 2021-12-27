#define PIN 14
#define LED 25

volatile boolean flag = false;


int8_t d_year, d_week, d_month, d_day, d_hour, d_min;
const byte month_day[]={0,31,28,31,30,31,30,31,31,30,31,30,31};

uint8_t hh, mm, ss, MM, DD;
bool  markerCheckOk;


void IRAM_ATTR jjysignaldetect() {
  if (!flag) {
    flag = true;//割り込みが発生したことをメインループに知らせるフラグ
  }
}


void setup() {
  pinMode(PIN, INPUT);
  pinMode(LED, OUTPUT);
  
  Serial.begin(115200);
  delay(200);


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
      Serial.printf("%02d:%02d:%02d\n", hh, mm, ss);
      
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


  //通算日数から月、日の算出
  d_month = 0;
  do{
    dayCount -= month_day[d_month];//dayCountから経過月の日数を減算
    d_month++;
  } while( dayCount > month_day[d_month]);//最後の経過月に到達するまで
  d_day = dayCount;
}


void loop() {
  int8_t m, p;
  int8_t sec, min = -1, hur;

  //2回連続マーカーの検出
  do {
    p = m;
    m = get_code();
  }while(p * m != 4);// マーカー(2)が２回続くまで繰り返す。
  Serial.println("2markers detected!!\n");
  ss = 0;
  markerCheckOk = true;

  while(markerCheckOk){ //ポジションマーカーをすべて正しく検出できている間は繰り返し。
                        //一つでも検出できていなかったら最初から始めなおす。
    /*秒数カウントクロック
    sec = (sec > 59)? 0 : sec;
    min = (sec == 0)? min+1 : min;
    min = (min > 59)? 0 : min;
    hur = (min == 0)? hur+1 : hur;
    hur = (hur > 23)? 0 : hur;
    //Serial.printf("%02d:%02d:%02d\n", hur, min, sec);
    sec++;
    */

    //デコード実行
    decode();
    hh = d_hour;
    mm = d_min + 1;
    DD = d_day;
    MM = d_month;
  
  }
  delay(1);
}
  




    
