#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <Servo.h>
#include <time.h>

#define SERVO_PIN 4 // 舵机 D2
#define ACTION_LED_PIN LED_BUILTIN //LED D0

// Timer
#define TIMER_START 0
#define TIMER_END 180

const char* mqttServerURL = "iot.eclipse.org"; //mqtt公用服务器
const char* ssid = "wifi ssid"; //wifi ssid
const char* password = "wifi password"; //wifi passwd

Servo myservo;
WiFiClient espClient;
PubSubClient mqttClient(espClient);

//设置时区
int timezone = 8; //东8区
int summertime = 0;
char actTime_str[20]; //现在的时间
char actDate_str[20]; //现在的日期

//秒计数
int numSeconds = 0;
bool numFlag = true;
int lastTime = 0;

//获得日期和天数
int destYear = 0;
int destMonth = 0;
int destDay = 0;
int destDays = 0;
bool countDown = false;
int startYear = 0;
int startMonth = 0;
int startDay = 0;
int startDays = 0;

//当前舵机位置
int currentServoPosition = 0;
double percentComplete = 0;
double lastPosition = 0;

// 获取天数
int days_in_month[13] = {0,31,28,31,30,31,30,31,31,30,31,30,31};

int leap_year(int year)
{
    if(year%400==0) return 1;

    if(year%4==0 && year%100!=0) return 1;

    return 0;
}

int number_of_days(int year, int day, int month)
{
    int result = 0;
    int i;

    for(i=1900; i < year; i++) {
        if(leap_year(i))
            result += 366;
        else
            result += 365;
    }

    for(i=1; i < month; i++) {
        result += days_in_month[i];

        if(leap_year(year) && i == 2) result++;
    }

    result += day;
    return result;
}

//设定舵机转到指定位置 add舵机初始化和断开
void MoveServoToPosition(int position, int speed)
{
  myservo.attach(SERVO_PIN); //初始化
  if (position < currentServoPosition)
  {
    for (int i = currentServoPosition; i > position; i--)
    {
      myservo.write(i);
      delay(speed);
    }
  }
  else if (position > currentServoPosition)
  {
    for (int i = currentServoPosition; i < position; i++)
    {
      myservo.write(i);
      delay(speed);
    }
  }

  currentServoPosition = position;
  myservo.detach();//断开 舵机不会发出声音
}

void setup()
{
  //初始化LED
  pinMode(ACTION_LED_PIN, OUTPUT);

  //wifi未连接
  digitalWrite(ACTION_LED_PIN, LOW);

  Serial.begin(115200);
  Serial.println();
  Serial.print("Connecting to: ");
  Serial.println(ssid);

  //wifi
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }

  randomSeed(micros());
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

  mqttClient.setServer(mqttServerURL, 1883);
  mqttClient.setCallback(callback);

  //wifi连接
  digitalWrite(ACTION_LED_PIN, HIGH);

  //舵机初始化位置
  MoveServoToPosition(90, 10);

  // NTP 获得时间
  configTime(timezone * 3600, 0, "0.europe.pool.ntp.org", "pool.ntp.org", "time.nist.gov");
  Serial.println("\nWaiting for time");
  while (!time(nullptr))
  {
    Serial.print(".");
    delay(1000);
  }
  Serial.println("");

}

void loop()
{
  time_t now = time(nullptr)+(summertime*3600);
  struct tm *tmp = localtime(&now);
  if (lastTime != tmp->tm_sec)
  {
    numSeconds++;
    lastTime = tmp->tm_sec;
    Serial.println(tmp->tm_sec);
  }
  if (numSeconds > 360)
  {

    //1小时获取时间一次
    sprintf (actTime_str, "%02d:%02d:%02d", tmp->tm_hour , tmp->tm_min, tmp->tm_sec);
    sprintf (actDate_str, "%02d.%02d.%04d", tmp->tm_mday , tmp->tm_mon+1, 1900+tmp->tm_year);
    if (tmp->tm_isdst > 0)
    {
      summertime = 1;
    }
    else
    {
      summertime = 0;
    }

    if (countDown == true)
    {
      digitalWrite(ACTION_LED_PIN, HIGH);
      int curYear = (actDate_str[6] - '0') * 1000 + (actDate_str[7] - '0') * 100 + (actDate_str[8] - '0') * 10 + (actDate_str[9] - '0');
      int curMonth = (actDate_str[3] - '0') * 10 + (actDate_str[4] - '0');
      int curDay = (actDate_str[0] - '0') * 10 + (actDate_str[1] - '0');
      int curDays = curDays = number_of_days(curYear, curDay, curMonth);

      if (startDay == 0 && startMonth == 0 && startYear == 0)
      {
        // 开始日期
        startYear = (actDate_str[6] - '0') * 1000 + (actDate_str[7] - '0') * 100 + (actDate_str[8] - '0') * 10 + (actDate_str[9] - '0');
        startMonth = (actDate_str[3] - '0') * 10 + (actDate_str[4] - '0');
        startDay = (actDate_str[0] - '0') * 10 + (actDate_str[1] - '0');
        startDays = number_of_days(startYear, startDay, startMonth);
        Serial.print("TimeStamp Year: "); Serial.print(startYear); Serial.print("; Month: "); Serial.print(startMonth); Serial.print("; Day: "); Serial.println(startDay);
      }

      int totalDays = destDays - startDays;
      int daysLeft = destDays - curDays;

      Serial.print("Days Left: "); Serial.println(daysLeft);

      if(daysLeft <= 0) // Countdown
      {
        Serial.println("Countdown Complete");
        MoveServoToPosition(TIMER_START, 10);
        MoveServoToPosition(TIMER_END, 20);
        MoveServoToPosition(TIMER_START, 10);
      }
      else
      {
        percentComplete = 1.0 - ((double)daysLeft / (double)totalDays);
        if (lastPosition != percentComplete)
        {
          lastPosition = percentComplete;
          MoveServoToPosition((int)(percentComplete * TIMER_END),1000);
        }
        Serial.print("Percent Complete: "); Serial.println(percentComplete);
      }
    }

    numSeconds = 0;
  }

  if (!mqttClient.connected())
  {
    connectToMQTT();
  }
  mqttClient.loop();
}

// mqtt反馈执行
void callback(char* topic, byte* payload, unsigned int length)
{
  Serial.print("Command from MQTT broker is : ");
  Serial.println(topic);

  if (payload[0] == '0') //倒计时模式
  {
    //获得秒数
    int seconds = 0;

    for (int i = 1; i < length; i++)
    {
      seconds = seconds * 10 + (payload[i] - '0');
    }

    int h = seconds / 3600;
    int m = (seconds - h * 3600) / 60;
    int s = (seconds - h * 3600) % 60;

    Serial.print("Timer: ");
    Serial.print(h);
    Serial.print(" : ");
    Serial.print(m);
    Serial.print(" : ");
    Serial.println(s);

    //执行
    int totalDistance = abs(TIMER_END - TIMER_START);
    int speed = (seconds * 1000) / totalDistance;
    MoveServoToPosition(TIMER_START, 0);
    MoveServoToPosition(TIMER_END, speed);

    MoveServoToPosition(TIMER_START, 10);
    MoveServoToPosition(TIMER_END, 20);
    MoveServoToPosition(TIMER_START, 10);

    Serial.println();
  }
  else if (payload[0] = '1') //日期倒计时模式
  {
    countDown = true;
    // 目标日期// 116.12.2017
    if (destYear == 0 && destMonth == 0 && destDay == 0)
    {
      destYear = (payload[6 + 1] - '0') * 1000 + (payload[7 + 1] - '0') * 100 + (payload[8 + 1] - '0') * 10 + (payload[9 + 1] - '0');
      destMonth = (payload[3 + 1] - '0') * 10 + (payload[4 + 1] - '0');
      destDay = (payload[0 + 1] - '0') * 10 + (payload[1 + 1] - '0');
      destDays = number_of_days(destYear, destDay, destMonth);
      Serial.print("Dest Year: "); Serial.print(destYear); Serial.print("; Month: "); Serial.print(destMonth); Serial.print("; Day: "); Serial.println(destDay);
      numSeconds = 3600;
      MoveServoToPosition(TIMER_START, 0);
    }
  }
  else if (payload[0] = '2')
  {
    digitalWrite(ACTION_LED_PIN, LOW);
    countDown = false;
    destYear == 0;
    destMonth == 0;
    destDay == 0;
    startDay == 0;
    startMonth == 0;
    startYear == 0;
  }
}

// 与MQtt服务器连接
void connectToMQTT()
{
  while (!mqttClient.connected())
  {
    Serial.print("Attempting MQTT connection...");

    // Create a random Client ID
    String clientId = "ESP8266Client-";
    clientId += String(random(0xffff), HEX);

    // Attempt to connect
    // if your MQTT broker has ClientID, Username and Password then change following line to // if (mqttClient.connect(clientId,userName,passWord))
    if (mqttClient.connect(clientId.c_str()))
    {
      Serial.println("Connected.!");

      // Subscribe to a topic
      mqttClient.subscribe("/lamp/status/"); // 改变订阅
    }
    else
    {
      Serial.print("failed, rc=");
      Serial.print(mqttClient.state());

      Serial.println(" try again in 6 seconds");
      delay(6000);
    }
  }
}


