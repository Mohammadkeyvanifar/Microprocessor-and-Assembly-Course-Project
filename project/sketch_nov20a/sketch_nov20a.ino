#include <FastLED.h>
#include <LiquidCrystal.h>

#define LED_PIN     6
#define NUM_LEDS    256 
#define LED_COLS    32  
#define BRIGHTNESS  250
#define LED_TYPE    WS2812
#define COLOR_ORDER GRB

// Buttons
#define BTN_INT     2 
#define BTN_NEXT    4
#define BTN_START   5

// RS, EN, D4, D5, D6, D7
LiquidCrystal lcd(7, 8, 9, 10, 11, 12);
CRGB leds[NUM_LEDS];


struct Task {
  int id;
  int arrival;
  int duration;
  int deadline;
  int priority;
  

  int remainingTime; 
  int startTime;
  bool isFinished;
};


CRGB taskColors[] = {CRGB::Red, CRGB::Orange, CRGB::Yellow};


const int MAX_TASKS = 3;
const int NUM_TASK_SETS = 6;

Task taskSets[NUM_TASK_SETS][MAX_TASKS] = {
  { {0,0,8,10,3}, {1,1,1,3,1}, {2,2,1,4,1} },
  { {0,0,4,8,1}, {1,1,3,10,3}, {2,3,2,10,3} },
  { {0,0,4,5,1}, {1,0,4,9,1}, {2,0,4,15,1} },
  { {0,0,6,15,1}, {1,1,2,8,10}, {2,2,5,13,5} },
  { {0,0,2,12,1}, {1,0,2,12,1}, {2,1,6,8,2} },
  { {0,0,2,3,1}, {1,0,2,3,2}, {2,0,2,3,3} }

};

Task currentTasks[MAX_TASKS];
int currentSetIndex = 0;
volatile bool isPaused = false; 

void setup() {
  Serial.begin(9600);
  
  FastLED.addLeds<LED_TYPE, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS);
  FastLED.setBrightness(BRIGHTNESS);
  FastLED.clear();
  FastLED.show();

  lcd.begin(16, 2);
  lcd.print("CPU Scheduler");
  lcd.setCursor(0, 1);
  lcd.print("Select Task Set");

  pinMode(BTN_NEXT, INPUT_PULLUP);
  pinMode(BTN_START, INPUT_PULLUP);
  pinMode(BTN_INT, INPUT_PULLUP);

  attachInterrupt(digitalPinToInterrupt(BTN_INT), togglePause, FALLING);
}

void loop() {
  if (digitalRead(BTN_NEXT) == LOW) {
    currentSetIndex = (currentSetIndex + 1) % NUM_TASK_SETS;
    updateLCDInfo();
    FastLED.clear(); FastLED.show(); 
    delay(300);
  }

  if (digitalRead(BTN_START) == LOW) {
    lcd.clear();
    lcd.print("Running...");
    delay(500);
    
    runAllAlgorithms();
    
    lcd.clear();
    lcd.print("Set "); lcd.print(currentSetIndex + 1); lcd.print(" Done");
    lcd.setCursor(0, 1);
    lcd.print("Press Next/Start");
  }
}

void togglePause() {
  isPaused = !isPaused;
}

void checkPause() {
  while (isPaused) {
    lcd.setCursor(15, 0);
    lcd.print("P");
    delay(200);
    lcd.setCursor(15, 0);
    lcd.print(" ");
    delay(200);
  }
}


void resetCurrentTasks() {
  for (int i = 0; i < MAX_TASKS; i++) {
    currentTasks[i] = taskSets[currentSetIndex][i];
    currentTasks[i].remainingTime = currentTasks[i].duration;
    currentTasks[i].isFinished = false;
    currentTasks[i].startTime = -1;
  }
}

void updateLCDInfo() {
  lcd.clear();
  lcd.print("Task Set: ");
  lcd.print(currentSetIndex + 1);
  lcd.setCursor(0, 1);
  lcd.print("Ready to Start");
}

void setLed(int algoRow, int timeCol, int taskId) {
  
  int blockIndex = timeCol / 8; 
  
  int localCol = timeCol;
  
  
  int finalIndex = (blockIndex * 64) + (algoRow * 8) + localCol;
  
  if (finalIndex < NUM_LEDS) {
    leds[finalIndex] = taskColors[taskId % 5];
    FastLED.show();
  }
}

void runAllAlgorithms() {
  resetCurrentTasks();
  runFCFS(0);
  delay(1000);

  resetCurrentTasks();
  runSJF(1);
  delay(1000);

  resetCurrentTasks();
  runEDF(2);
  delay(1000);

  resetCurrentTasks();
  runRR(3);
  delay(1000);

  resetCurrentTasks();
  runPriority(4);
  delay(1000);

}

/* ---------------------
   FCFS 
   (غیر preemptive؛ صف بر اساس زمان ورود)
   --------------------- */
void runFCFS(int row) {
  lcd.clear(); lcd.print("Algo: FCFS");
  
  // ابتدا ترتیب ورود را پیدا می‌کنیم (index array sorted by arrival then id)
  int order[MAX_TASKS];
  bool used[MAX_TASKS] = {false, false, false};
  for (int k=0; k<MAX_TASKS; k++) {
    int bestIdx = -1;
    int bestArrival = 99999;
    for (int i=0;i<MAX_TASKS;i++) {
      if (!used[i]) {
        if (currentTasks[i].arrival < bestArrival || (currentTasks[i].arrival == bestArrival && currentTasks[i].id < (bestIdx==-1?999:currentTasks[bestIdx].id))) {
          bestArrival = currentTasks[i].arrival;
          bestIdx = i;
        }
      }
    }
    used[bestIdx] = true;
    order[k] = bestIdx;
  }

  int time = 0;
  for (int idxPos = 0; idxPos < MAX_TASKS; idxPos++) {
    int i = order[idxPos];
    // اگر هنوز نرسیده، جلو بزن تا زمانی که برسد
    if (time < currentTasks[i].arrival) {
      int idleSteps = currentTasks[i].arrival - time;
      for (int z=0; z<idleSteps; z++) {
        delay(200);
        time++;
        if (time >= LED_COLS) return;
      }
    }
    // اجرا کن به اندازهٔ duration
    for (int t=0; t<currentTasks[i].duration; t++) {
      setLed(row, time, currentTasks[i].id);
      delay(200);
      time++;
      if (time >= LED_COLS) break;
    }
    // علامت اتمام
    if (time > currentTasks[i].deadline) {
      lcd.setCursor(0, 1); lcd.print("Miss: T"); lcd.print(currentTasks[i].id);
    }
    if (time >= LED_COLS) break;
  }
}

/* ---------------------
   SJF -(non-preemptive)
   انتخاب کوتاه‌ترین شغلِ در دسترس در هر گام انتخاب
   --------------------- */
void runSJF(int row) {
  lcd.clear(); lcd.print("Algo: SJF");
  int time = 0;
  int finished = 0;

  while (finished < MAX_TASKS) {
    // پیدا کردن کوتاه‌ترین بین آن‌هایی که رسیده‌اند و تمام نشده‌اند
    int sel = -1;
    int bestDur = 99999;
    for (int i=0;i<MAX_TASKS;i++) {
      if (!currentTasks[i].isFinished && currentTasks[i].arrival <= time) {
        if (currentTasks[i].duration < bestDur) {
          bestDur = currentTasks[i].duration;
          sel = i;
        }
      }
    }

    if (sel == -1) {
      // هیچ کاری نیست، یک واحد زمان جلو می‌رویم
      delay(200);
      time++;
      if (time >= LED_COLS) break;
      continue;
    }

    // اجرای کامل انتخاب (non-preemptive)
    for (int t=0; t<currentTasks[sel].duration; t++) {
      setLed(row, time, currentTasks[sel].id);
      delay(200);
      time++;
      if (time >= LED_COLS) break;
    }
    currentTasks[sel].isFinished = true;
    finished++;
    if (time > currentTasks[sel].deadline) {
      lcd.setCursor(0, 1); lcd.print("Miss: T"); lcd.print(currentTasks[sel].id);
    }
    if (time >= LED_COLS) break;
  }
}

/* ---------------------
   EDF (preemptive, tick-based)
   در هر tick زودترین deadline را انتخاب و یک واحد اجرا می‌کند
   --------------------- */
void runEDF(int row) {
  lcd.clear(); lcd.print("Algo: EDF");
  int time = 0;
  int finished = 0;

  while (finished < MAX_TASKS) {
    int sel = -1;
    int bestDeadline = 999999;
    for (int i=0;i<MAX_TASKS;i++) {
      if (!currentTasks[i].isFinished && currentTasks[i].arrival <= time && currentTasks[i].remainingTime>0) {
        if (currentTasks[i].deadline < bestDeadline) {
          bestDeadline = currentTasks[i].deadline;
          sel = i;
        }
      }
    }

    if (sel == -1) {
      // هیچ تسکی قابل اجرا نیست — زمان را افزایش می‌دهیم
      delay(200);
      time++;
      if (time >= LED_COLS) break;
      continue;
    }

    // اجرای یک واحد از تسک انتخاب شده (preemptive)
    setLed(row, time, currentTasks[sel].id);
    currentTasks[sel].remainingTime--;
    delay(200);
    time++;

    if (currentTasks[sel].remainingTime == 0) {
      currentTasks[sel].isFinished = true;
      finished++;
      if (time > currentTasks[sel].deadline) {
        lcd.setCursor(0, 1); lcd.print("Miss: T"); lcd.print(currentTasks[sel].id);
      }
    }

    if (time >= LED_COLS) break;
  }
}

/* ---------------------
   RR (ساختار صف واقعی)
   از یک queue ساده برای ورود و خروج استفاده می‌کند
   --------------------- */
void runRR(int row) {
  lcd.clear(); lcd.print("Algo: RR (Q=2)");
  int quantum = 2;
  int time = 0;
  int finished = 0;

  // queue implementation (indices)
  int q[64];
  int qhead = 0, qtail = 0;

  bool inQueue[MAX_TASKS] = {false, false, false};

  // تابع کمکی برای enqueue
  auto enqueue = [&](int idx){
    q[qtail++] = idx;
    if (qtail >= 64) qtail = 0;
    inQueue[idx] = true;
  };
  // dequeue با چک خالی
  auto dequeue = [&]() -> int {
    if (qhead == qtail) return -1;
    int v = q[qhead++];
    if (qhead >= 64) qhead = 0;
    inQueue[v] = false;
    return v;
  };

  // شروع: enqueue هر تسکی که arrival<=time
  for (int i=0;i<MAX_TASKS;i++) {
    if (currentTasks[i].arrival <= time) enqueue(i);
  }

  while (finished < MAX_TASKS) {
    // اگر صف خالی، زمان جلو برود و بررسی arrival های جدید
    if (qhead == qtail) {
      // جلو رفتن تا اولین arrival یا یک step
      bool added = false;
      int nextArrival = 999999;
      for (int i=0;i<MAX_TASKS;i++) if (!currentTasks[i].isFinished && !inQueue[i]) nextArrival = min(nextArrival, currentTasks[i].arrival);
      if (nextArrival <= time) {
        for (int i=0;i<MAX_TASKS;i++) {
          if (!currentTasks[i].isFinished && !inQueue[i] && currentTasks[i].arrival <= time) enqueue(i);
          if (qhead != qtail) { added = true; break; }
        }
        if (!added) {
          delay(200);
          time++;
          if (time >= LED_COLS) break;
          // سپس دوباره تلاش برای enqueue
          for (int i=0;i<MAX_TASKS;i++) {
            if (!currentTasks[i].isFinished && !inQueue[i] && currentTasks[i].arrival <= time) enqueue(i);
          }
        }
      } else {
        delay(200);
        time++;
        if (time >= LED_COLS) break;
        for (int i=0;i<MAX_TASKS;i++) {
          if (!currentTasks[i].isFinished && !inQueue[i] && currentTasks[i].arrival <= time) enqueue(i);
        }
      }
    }

    int idx = dequeue();
    if (idx == -1) continue;

    // اجرای تا quantum یا تا پایان تسک، به صورت واحد-واحد برای نمایش
    int run = min(quantum, currentTasks[idx].remainingTime);
    for (int r=0;r<run;r++) {
      setLed(row, time, currentTasks[idx].id);
      currentTasks[idx].remainingTime--;
      delay(200);
      time++;
      // در طول اجرای کوانتوم، اگر تسک های جدید دارای arrival<=time باشند، آنها را به صف اضافه کن
      for (int j=0;j<MAX_TASKS;j++) {
        if (!currentTasks[j].isFinished && !inQueue[j] && currentTasks[j].arrival <= time) enqueue(j);
      }
      if (currentTasks[idx].remainingTime == 0) break;
      if (time >= LED_COLS) break;
    }

    if (currentTasks[idx].remainingTime == 0) {
      currentTasks[idx].isFinished = true;
      finished++;
      if (time > currentTasks[idx].deadline) {
        lcd.setCursor(0, 1); lcd.print("Miss: T"); lcd.print(currentTasks[idx].id);
      }
    } else {
      // اگر تموم نشده، دوباره به صف اضافه کن
      enqueue(idx);
    }

    if (time >= LED_COLS) break;
  }
}

/* ---------------------
   Priority (preemptive tick-based)
   اولویت کمتر = بهتر (همان قرارداد قبلی)
   --------------------- */
void runPriority(int row) {
  lcd.clear(); lcd.print("Algo: Priority");
  int time = 0;
  int finished = 0;

  while (finished < MAX_TASKS) {
    // انتخاب براساس کمترین priority بین وارد شده‌ها
    int sel = -1;
    int bestPri = 999999;
    for (int i=0;i<MAX_TASKS;i++) {
      if (!currentTasks[i].isFinished && currentTasks[i].arrival <= time && currentTasks[i].remainingTime>0) {
        // tie-breaker: arrival then id
        if (currentTasks[i].priority < bestPri ||
           (currentTasks[i].priority == bestPri && currentTasks[i].arrival < (sel==-1?999999:currentTasks[sel].arrival))
           || (currentTasks[i].priority == bestPri && currentTasks[i].arrival == (sel==-1?999999:currentTasks[sel].arrival) && currentTasks[i].id < (sel==-1?999:currentTasks[sel].id))
           ) {
          bestPri = currentTasks[i].priority;
          sel = i;
        }
      }
    }

    if (sel == -1) {
      delay(200);
      time++;
      if (time >= LED_COLS) break;
      continue;
    }

    // اجرای یک tick (preemptive)
    setLed(row, time, currentTasks[sel].id);
    currentTasks[sel].remainingTime--;
    delay(200);
    time++;

    if (currentTasks[sel].remainingTime == 0) {
      currentTasks[sel].isFinished = true;
      finished++;
      if (time > currentTasks[sel].deadline) {
        lcd.setCursor(0, 1);
        lcd.print("Miss: T");
        lcd.print(currentTasks[sel].id);
      }
    }

    if (time >= LED_COLS) break;
  }
}
