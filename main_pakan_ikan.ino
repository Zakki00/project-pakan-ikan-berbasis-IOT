  #include <Keypad.h>
  #include <Wire.h>
  #include <LiquidCrystal_I2C.h>
  #include <ESP32Servo.h>
  #include <RTClib.h>
  #include <Preferences.h>

  // prefernces menyimpan data
  Preferences preferences;

  // buzser
  int buzzerPin = 12; // pin buzzer

  // RTC
  RTC_DS3231 rtc;


  // Servo
  Servo servoPintu;
  int pinservo = 13;
  bool statusPakanAktif = false;

  // LCD
  LiquidCrystal_I2C lcd(0x27, 16, 2);  // alamat bisa 0x27 atau 0x3F tergantung modul

  // Keypad setup
  const byte ROWS = 4;
  const byte COLS = 4;
  char keys[ROWS][COLS] = {
    {'1','2','3','A'},
    {'4','5','6','B'},
    {'7','8','9','C'},
    {'*','0','#','D'}
  };

  byte rowPins[ROWS] = {18, 19, 23, 5};     // R1 - R4
  byte colPins[COLS] = {25, 26, 27, 14};    // C1 - C4
  Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

  // Jadwal
  struct Jadwal {
    int jamMulai;
    int menitMulai;
    int jamAkhir;
    int menitAkhir;
  };

  Jadwal pagi = {7, 0, 7, 30};
  Jadwal siang = {12, 0, 12, 30};
  Jadwal sore = {16, 0, 16, 30};

  // Mode input
  enum Mode {
    MODE_IDLE,
    ATUR_PAGI_MULAI, ATUR_PAGI_AKHIR,
    ATUR_SIANG_MULAI, ATUR_SIANG_AKHIR,
    ATUR_SORE_MULAI, ATUR_SORE_AKHIR,
    MODE_LIHAT_JADWAL,
    MODE_SET_JAM
  };

  Mode mode = MODE_IDLE;
  String inputTime = "";

  // Waktu dari RTC
  int jam = 0;
  int menit = 0;

  // dinamo
  // Motor driver BTS7960
const int pinMotorPWM = 32;   // R_PWM
const int pinMotorL = 4;      // L_PWM, diset LOW terus
const int pinMotorEN = 15;    // EN (bisa R_EN dan L_EN digabung)
 
  // ------------------- SETUP -------------------

  void setup() {
    Serial.begin(115200);

    // I2C pin ESP32
    Wire.begin(21, 22); // SDA, SCL
    
    lcd.init();
    lcd.backlight();
    lcd.setCursor(0, 0);
    lcd.print("Inisialisasi...");
    delay(1000);
    lcd.clear();

    //buzser setup
    pinMode(buzzerPin, OUTPUT);
    digitalWrite(buzzerPin, LOW); // pastikan awalnya mati
    // RTC
    if (!rtc.begin()) {
      lcd.print("RTC tidak ditemukan!");
      while (1);
    }

    if (rtc.lostPower()) {
      rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
      Serial.println("RTC diatur ke waktu kompilasi.");
    }
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    Serial.println("RTC diatur ulang ke waktu kompilasi.");
    preferences.begin("jadwal", false);

  // Load jadwal dari memori (kalau belum ada → pakai default)
    pagi.jamMulai   = preferences.getInt("pagiMulaiJam", pagi.jamMulai);
    pagi.menitMulai = preferences.getInt("pagiMulaiMenit", pagi.menitMulai);
    pagi.jamAkhir   = preferences.getInt("pagiAkhirJam", pagi.jamAkhir);
    pagi.menitAkhir = preferences.getInt("pagiAkhirMenit", pagi.menitAkhir);

    siang.jamMulai   = preferences.getInt("siangMulaiJam", siang.jamMulai);
    siang.menitMulai = preferences.getInt("siangMulaiMenit", siang.menitMulai);
    siang.jamAkhir   = preferences.getInt("siangAkhirJam", siang.jamAkhir);
    siang.menitAkhir = preferences.getInt("siangAkhirMenit", siang.menitAkhir);

    sore.jamMulai   = preferences.getInt("soreMulaiJam", sore.jamMulai);
    sore.menitMulai = preferences.getInt("soreMulaiMenit", sore.menitMulai);
    sore.jamAkhir   = preferences.getInt("soreAkhirJam", sore.jamAkhir);
    sore.menitAkhir = preferences.getInt("soreAkhirMenit", sore.menitAkhir);

    servoPintu.attach(pinservo);
    servoPintu.write(0);

  
    lcd.print("Pakan Ikan Aktif");
    delay(1000);
    lcd.clear();

    pinMode(pinMotorPWM, OUTPUT);
    pinMode(pinMotorL, OUTPUT);
    pinMode(pinMotorEN, OUTPUT);

    digitalWrite(pinMotorL, LOW);      // cuma 1 arah
    digitalWrite(pinMotorEN, HIGH);    // aktifkan driver

  }

  // ------------------- LOOP -------------------

  void loop() {
    updateWaktu();
    if (mode == MODE_SET_JAM) {
      updatejam();   // <-- di sini baru jalan terus-terusan
    } else {
      prosesKeypad();
    }
    updateLCD();

    bool aktif = waktuPakanAktif();

    if (aktif && !statusPakanAktif) {
      servoPintu.write(100);
      Serial.println("Pakan dimulai - Servo dibuka");
      analogWrite(pinMotorPWM, 255); // PWM 0-255, atur kecepatan
      Serial.println("Motor ON");
      buzzerBeep(2, 200, 150); // bunyi 2x, tiap bunyi 200ms, jeda 150ms
      statusPakanAktif = true;
    } else if (!aktif && statusPakanAktif) {
      servoPintu.write(0);
      Serial.println("Pakan selesai - Servo ditutup");
    //  for (int speed = 255; speed >= 0; speed -= 25) {
    //     analogWrite(pinMotorPWM, speed);
    //     delay(100); // jeda biar smooth
    //   } 
      analogWrite(pinMotorPWM, 0); // motor off
      Serial.println("Motor OFF");
      buzzerBeep(1, 300, 0);   // bunyi 1x panjang
      statusPakanAktif = false;
    }

    delay(200); // debounce ringan

    // char key = keypad.getKey();
    // if (!key) return;
    // Serial.print("Keypad: "); Serial.println(key);

  }
  //buzer
  void buzzerBeep(int jumlah, int durasi, int jeda) {
    for (int i = 0; i < jumlah; i++) {
      digitalWrite(buzzerPin, HIGH);
      delay(durasi);
      digitalWrite(buzzerPin, LOW);
      delay(jeda);
    }
  }

  // ------------------- UPDATE WAKTU -------------------

  void updateWaktu() {
    DateTime now = rtc.now();
    jam = now.hour();
    menit = now.minute();

    Serial.print("Waktu RTC: ");
    Serial.print(jam); Serial.print(":"); Serial.println(menit);
  }

  // ------------------- PROSES KEY -------------------

  void prosesKeypad() {
    
    char key = keypad.getKey();
    if (!key) return;
    Serial.println(key);
    // Kalau sedang mode LIHAT JADWAL
    if (mode == MODE_LIHAT_JADWAL) {
      if (key == 'A') {
        lcd.clear();
        lcd.print("Pagi");
        lcd.setCursor(0, 1);
        lcd.print(formatJam(pagi.jamMulai, pagi.menitMulai));
        lcd.print("-");
        lcd.print(formatJam(pagi.jamAkhir, pagi.menitAkhir));
        delay(6000);
        lcd.clear();
        mode = MODE_IDLE;
      } else if (key == 'B') {
        lcd.clear();
        lcd.print("Siang");
        lcd.setCursor(0, 1);
        lcd.print(formatJam(siang.jamMulai, siang.menitMulai));
        lcd.print("-");
        lcd.print(formatJam(siang.jamAkhir, siang.menitAkhir));
        delay(6000);
        lcd.clear();
        mode = MODE_IDLE;
      } else if (key == 'C') {
        lcd.clear();
        lcd.print("Sore");
        lcd.setCursor(0, 1);
        lcd.print(formatJam(sore.jamMulai, sore.menitMulai));
        lcd.print("-");
        lcd.print(formatJam(sore.jamAkhir, sore.menitAkhir));
        delay(6000);
        lcd.clear();
        mode = MODE_IDLE;
      } else if (key == 'D') {
        // batal lihat jadwal
        lcd.clear();
        lcd.print("Batal lihat");
        delay(1000);
        lcd.clear();
        mode = MODE_IDLE;
      }
      return;  // <== penting, biar nggak lanjut ke logika bawah
    }

    // ========================================
    // KALAU MODE NORMAL (IDLE)
    // ========================================
    if (key == 'A') {
      lcd.clear();
      lcd.print("Set Pagi");
      mode = ATUR_PAGI_MULAI;
      inputTime = "";
    } else if (key == 'B') {
      lcd.clear();
      lcd.print("Set Siang");
      mode = ATUR_SIANG_MULAI;
      inputTime = "";
    } else if (key == 'C') {
      lcd.clear();
      lcd.print("Set Sore");
      mode = ATUR_SORE_MULAI;
      inputTime = "";
    }else if (key == 'D') {
      if (mode != MODE_IDLE && mode !=MODE_SET_JAM) {
        mode = MODE_IDLE;
        inputTime = "";
        lcd.clear();
        lcd.print("Input dibatalkan");
        delay(1000);
        lcd.clear();
      } else {
        // Masuk ke mode lihat jadwal
        mode = MODE_LIHAT_JADWAL;
        lcd.clear();
        lcd.print("Pilih A/B/C");
        lcd.setCursor(0, 1);
        lcd.print("Masukan: ");
      }
    }else if (key == '*'){
      if(mode != MODE_IDLE){
         if (inputTime.length() > 0) {
          inputTime.remove(inputTime.length() - 1);
          lcd.setCursor(0, 1);
          lcd.print("Input: " + formatWaktu(inputTime)  + "   ");
        }
      }else{
        mode = MODE_SET_JAM;
        inputTime = "";
        lcd.clear();
        lcd.print("Set Waktu");
        lcd.setCursor(0,1);
        lcd.print("Input: ");
      }
     
    }


  
    /// Mengecek Apakah Dalam Mode Tertentu? ///
    
    if (mode != MODE_IDLE && mode != MODE_LIHAT_JADWAL && mode != MODE_SET_JAM) {
      lcd.setCursor(0, 1);
      lcd.print("Input: " + formatWaktu(inputTime) + "   ");
    }
    ///=====================================///
    /// Untuk Menginputkan Jam Waktu Pakan ///
    ///====================================///

    if (isdigit(key)) {
      if (inputTime.length() < 4) {
        inputTime += key;
        lcd.setCursor(0, 1);
        lcd.print("Input: " + formatWaktu(inputTime)  + "                ");
      }
    } 
    else if (key == '#') {
      if (inputTime.length() == 4) {
        int jamInput = inputTime.substring(0, 2).toInt();
        int menitInput = inputTime.substring(2, 4).toInt();

        if (jamInput < 0 || jamInput > 23 || menitInput < 0 || menitInput > 59) {
          lcd.clear();
          lcd.print("Format jam salah!");
          delay(2000);
          lcd.clear();
          inputTime = "";
          mode = MODE_IDLE;
          return;
        }
        switch (mode) {
          case ATUR_PAGI_MULAI:
            pagi.jamMulai = jamInput;
            lcd.setCursor(0, 1); lcd.print("Mulai: " + formatWaktu(inputTime));
            pagi.menitMulai = menitInput;
            mode = ATUR_PAGI_AKHIR;
            preferences.putInt("pagiMulaiJam", pagi.jamMulai);
            preferences.putInt("pagigMulaiMenit", pagi.menitMulai);
            break;
  
          case ATUR_PAGI_AKHIR:
            pagi.jamAkhir = jamInput;
            lcd.setCursor(0, 1); lcd.print("Selesai: " + formatWaktu(inputTime));
            pagi.menitAkhir = menitInput;
            mode = MODE_IDLE;
            preferences.putInt("pagiAkhirJam", pagi.jamAkhir);
            preferences.putInt("pagiAkhirMenit", pagi.menitAkhir);
            break;
  
          case ATUR_SIANG_MULAI:
            siang.jamMulai = jamInput;
            lcd.setCursor(0, 1); lcd.print("Mulai: " + formatWaktu(inputTime));
            siang.menitMulai = menitInput;
            mode = ATUR_SIANG_AKHIR;
            preferences.putInt("siangeMulaiJam", siang.jamMulai);
            preferences.putInt("siangMulaiMenit", siang.menitMulai);
            break;
  
          case ATUR_SIANG_AKHIR:
            siang.jamAkhir = jamInput;
            lcd.setCursor(0, 1); lcd.print("Selesai: " + formatWaktu(inputTime));
            siang.menitAkhir = menitInput;
            mode = MODE_IDLE;
            preferences.putInt("siangAkhirJam", siang.jamAkhir);
            preferences.putInt("siangAkhirMenit", siang.menitAkhir);
            break;
  
          case ATUR_SORE_MULAI:
            sore.jamMulai = jamInput;
            lcd.setCursor(0, 1); lcd.print("Mulai: " + formatWaktu(inputTime));
            sore.menitMulai = menitInput;
            mode = ATUR_SORE_AKHIR;
            preferences.putInt("soreMulaiJam", sore.jamMulai);
            preferences.putInt("soreMulaiMenit", sore.menitMulai);
            break;
  
          case ATUR_SORE_AKHIR:
            sore.jamAkhir = jamInput;
            lcd.setCursor(0, 1); lcd.print("Selesai: " + formatWaktu(inputTime));
            sore.menitAkhir = menitInput;
            mode = MODE_IDLE;
            preferences.putInt("soreAkhirJam", sore.jamAkhir);
            preferences.putInt("soreAkhirMenit", sore.menitAkhir);
            break;
  
          default:
          break;
        }
        inputTime = "";
      }
    }
  }

  // -------------------- Fungsi Unutuk Set Ulang jam----//
  void updatejam(){
    char key = keypad.getKey();
    if(MODE_SET_JAM){
      if (key) {
      if (key >= '0' && key <= '9') {
        if (inputTime.length() < 4) {
          inputTime += key;
          lcd.setCursor(0,1);
          lcd.print("Input: " + formatWaktu(inputTime));
        }
      } else if (key == '#') {
       // di MODE_SET_JAM saat tombol '#' ditekan
        if (inputTime.length() == 4) {
          int hh = inputTime.substring(0,2).toInt();
          int mm = inputTime.substring(2,4).toInt();

         
          if (hh < 0 || hh > 23 || mm < 0 || mm > 59) {
            lcd.clear();
            lcd.print("Format Jam Salah!");
            delay(2000);
            lcd.clear();
            mode = MODE_IDLE;
            return;                      
          }
          DateTime now = rtc.now();
          rtc.adjust(DateTime(now.year(), now.month(), now.day(), hh, mm, 0));
          mode = MODE_IDLE;
          inputTime = "";
        }else {
          lcd.clear();
          lcd.print("4 Digit perlu!");
        }
        mode = MODE_IDLE;
        inputTime = "";
      }
      else if (key == 'D') {
        inputTime = "";
        lcd.clear();
        lcd.print("Batal Set Jam");
        delay(3000);
        lcd.clear();
        mode = MODE_IDLE;
      }else if (key == '*') {
        if (inputTime.length() > 0) {
          inputTime.remove(inputTime.length() - 1);
          lcd.setCursor(0, 1);
          lcd.print("Input: " + formatWaktu(inputTime)  + "   ");
        }
      }
    }
  }
}
  // ------------------- LCD DISPLAY -------------------

  void updateLCD() {
  if (mode == MODE_IDLE) {
    if (dalamJadwal(pagi)) {
      lcd.setCursor(0, 0);
      lcd.print("Mode Pagi " + formatJam(jam, menit) + "   "); // spasi ekstra buat hapus sisa
      lcd.setCursor(0, 1);
      lcd.print(formatJam(pagi.jamMulai, pagi.menitMulai));
      lcd.print("-");
      lcd.print(formatJam(pagi.jamAkhir, pagi.menitAkhir));
      lcd.print("   "); // hapus sisa
    } 
    else if (dalamJadwal(siang)) {
      lcd.setCursor(0, 0);
      lcd.print("Mode Siang " + formatJam(jam, menit) + "   ");
      lcd.setCursor(0, 1);
      lcd.print(formatJam(siang.jamMulai, siang.menitMulai));
      lcd.print("-");
      lcd.print(formatJam(siang.jamAkhir, siang.menitAkhir));
      lcd.print("   ");
    } 
    else if (dalamJadwal(sore)) {
      lcd.setCursor(0, 0);
      lcd.print("Mode Sore " + formatJam(jam, menit) + "   ");
      lcd.setCursor(0, 1);
      lcd.print(formatJam(sore.jamMulai, sore.menitMulai));
      lcd.print("-");
      lcd.print(formatJam(sore.jamAkhir, sore.menitAkhir));
      lcd.print("   ");
    } 
    else {
      lcd.setCursor(0, 0);
      lcd.print("Waktu: " + formatJam(jam, menit) + "                ");
      lcd.setCursor(0, 1);
      lcd.print("Mode: Standby   ");
    }
  }
}


  //Format Jam
  String formatJam(int j, int m) {
    String str = "";
    if (j < 10) str += "0";
    str += String(j) + ":";
    if (m < 10) str += "0";
    str += String(m);
    return str;
  }

  String formatWaktu(String raw) {
  if (raw.length() == 4) {
    return raw.substring(0,2) + ":" + raw.substring(2,4);
  } else {
    return raw; // kalau belum 4 digit, tampilkan apa adanya
  }
}


  // ------------------- KONTROL -------------------

  bool dalamJadwal(Jadwal j) {
    int now = jam * 60 + menit;
    int start = j.jamMulai * 60 + j.menitMulai;
    int end = j.jamAkhir * 60 + j.menitAkhir;
    return now >= start && now <= end;
  }

  bool waktuPakanAktif() {
    return dalamJadwal(pagi) || dalamJadwal(siang) || dalamJadwal(sore);
  }
