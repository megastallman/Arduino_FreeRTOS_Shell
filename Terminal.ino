#include <Arduino_FreeRTOS.h>
#include <TFT_HX8357.h>
#include <queue.h>

TFT_HX8357 tft = TFT_HX8357();
const uint8_t LED_PIN = 13;
volatile uint32_t count = 0;
int lcdLineLength = 37;
int lineNoMax = 30;
int lcdRotation = 2;
int lcdFont = 2;
int lcdBufferTimeout = 50; //milliseconds

// handles for all tasks
TaskHandle_t blink;
TaskHandle_t ttyI;
TaskHandle_t ttyO;
TaskHandle_t graph;
TaskHandle_t shell;

// Declare a variable of type QueueHandle_t.
QueueHandle_t LCDQueue;
QueueHandle_t Serial2SHELLQueue;
QueueHandle_t SHELL2SerialQueue;

//------------------------------------------------------------------------------
static void vLEDFlashTask(void *pvParameters) {

  pinMode(LED_PIN, OUTPUT);

  for (;;) {
    digitalWrite(LED_PIN, HIGH);
    vTaskDelay( 500 / portTICK_PERIOD_MS );
    digitalWrite(LED_PIN, LOW);
    vTaskDelay( 500 / portTICK_PERIOD_MS );
  }
}
//------------------------------------------------------------------------------
static void vTTYITask(void *pvParameters) {
  int incomingByte = 0;
  for (;;) {
    if (Serial.available() > 0) {
      incomingByte = Serial.read();
      xQueueSendToBack( Serial2SHELLQueue, &incomingByte, 0 );
    }
  }
}
//------------------------------------------------------------------------------
static void vTTYOTask(void *pvParameters) {
  int incomingByte = 0;
  Serial.write('#');
  Serial.write(' ');
  // Output bytes to serial
  for (;;) {
    xQueueReceive( SHELL2SerialQueue, &incomingByte, portMAX_DELAY );
    Serial.write(incomingByte);
  }
}
//------------------------------------------------------------------------------
static void vGraphicsTask(void *pvParameters) {
  int incomingByte;
  tft.init();
  tft.setRotation(lcdRotation);
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_CYAN);
  tft.setTextFont(lcdFont);
  tft.print('#');
  tft.print(' ');
  // Output bytes to LCD
  for (;;) {
    xQueueReceive( LCDQueue, &incomingByte, portMAX_DELAY );
    tft.print(char(incomingByte));
  }
}
//------------------------------------------------------------------------------
void setup() {
  Serial.begin(9600);
  while (!Serial) {}
  //Serial.println("Begin...");

  // create queue
  LCDQueue = xQueueCreate( lcdLineLength, sizeof( char * ) ); //just 1 message in a queue
  Serial2SHELLQueue = xQueueCreate( lcdLineLength, sizeof( char * ) ); //just 1 message in a queue
  SHELL2SerialQueue = xQueueCreate( lcdLineLength, sizeof( char * ) ); //just 1 message in a queue

  // create blink task
  xTaskCreate(vLEDFlashTask,
              "blink",
              configMINIMAL_STACK_SIZE,
              NULL,
              tskIDLE_PRIORITY + 1,
              &blink);

  // create graphics task
  xTaskCreate(vGraphicsTask,
              "graph",
              configMINIMAL_STACK_SIZE + 200,
              NULL,
              tskIDLE_PRIORITY + 1,
              &graph);

  // create TTY output task
  xTaskCreate(vTTYOTask,
              "tty_o",
              configMINIMAL_STACK_SIZE + 300,
              NULL,
              tskIDLE_PRIORITY + 1,
              &ttyO);

  // create TTY input task
  xTaskCreate(vTTYITask,
              "tty_i",
              configMINIMAL_STACK_SIZE,
              NULL,
              tskIDLE_PRIORITY + 1,
              &ttyI);

  // create shell task
  xTaskCreate(vShellTask,
              "shell",
              configMINIMAL_STACK_SIZE + 600,
              NULL,
              tskIDLE_PRIORITY + 1,
              &shell);

  // start FreeRTOS
  vTaskStartScheduler();

  // should never return
  Serial.println(F("Die"));
  while (1);
}
//------------------------------------------------------------------------------
// WARNING idle loop has a very small stack (configMINIMAL_STACK_SIZE)
// loop must never block
void loop() {
  while (1) {
    // must insure increment is atomic
    // in case of context switch for print
    noInterrupts();
    count++;
    interrupts();
  }
}
//------------------------------------------------------------------------------
static void vPrSym(int cmdByte) {
  xQueueSendToBack( LCDQueue, &cmdByte, 0 );
  xQueueSendToBack( SHELL2SerialQueue, &cmdByte, 0 );
}
//------------------------------------------------------------------------------
static void vPrPs(TaskHandle_t taskHandle){         
   
          char *taskName = pcTaskGetName(taskHandle);
          int padCount = 10 - strlen(taskName);

          for(int i = 0; i<strlen(taskName); i++){
            vPrSym(taskName[i]);
          }
          for(int i = 0; i<padCount; i++){
            vPrSym(32); //Space padding
          }
          char watermarkStr[] = "          ";
          itoa(uxTaskGetStackHighWaterMark(taskHandle), watermarkStr, 10);
          for(int i = 0; i<10; i++){
            vPrSym(watermarkStr[i]);
          }
          vPrSym(10); //Send a "newline" symbol
          vTaskDelay(lcdBufferTimeout/portTICK_PERIOD_MS);
          
}         
//------------------------------------------------------------------------------
static void vShellTask(void *pvParameters) {

    int cmdByte = 0;
    int space = 32;
    int newline = 10;
    int gateSymbol = 35;
    int backspace = 8;
    int lineNo = 0;
    int cmdLength = 0;
    int cmdLengthMax = lcdLineLength;
    int helpTextHeight = 12;
    char cmd[cmdLengthMax]; //Max command length should be limited to "lcdLineLength" symbols
    char lsCmd[] = "ls";
    char resetCmd[] = "reset";
    char picviewCmd[] = "picview";
    char psCmd[] = "ps";
    char helpCmd[] = "help";
    char rmCmd[] = "rm";
    char commandNotFound[] = "Command not found";
    char helpText[helpTextHeight][lcdLineLength - 1] = {
      {"Shell commands list:               "},
      {"  # help                           "},
      {"  # reset                          "},
      {"  # ps                             "},
      {"                                   "},
      {"In case you have a microSD flash   "},
      {"drive, you can also run these:     "},
      {"  # ls                             "},
      {"  # rm <filename>                  "},
      {"  # picview <filename>             "},
      {"                                   "},
      {"Enjoy!!!                           "},
    };
  

    // Command detection(on Enter press)
    for (;;) {
      xQueueReceive( Serial2SHELLQueue, &cmdByte, portMAX_DELAY );
      if (lineNo >= lineNoMax) {
        lineNo = 0;
        tft.fillScreen(TFT_BLACK);
        tft.setCursor(0, 0);
        if (cmdByte != newline) {
          xQueueSendToBack( LCDQueue, &gateSymbol, 0 );
          xQueueSendToBack( LCDQueue, &space, 0 );
        }
      }
      cmd[cmdLength] = cmdByte;
      cmdLength++;
      if (cmdLength >= cmdLengthMax) {
       cmdLength = 0;
       lineNo++;
      }
      vPrSym(cmdByte);
      if (cmdByte == newline) {


        // Echo the input command back
        //for(int i = 0; i<cmdLength; i++){
        //  vPrSym(cmd[i]);
        //}
        //lineNo++;

        int commandnotfoundFlag = 1;

        // Processing the reset command
        int resetFlag=1;
        for(int i = 0; i<(sizeof(resetCmd)-1); i++){
          if(cmd[i] != resetCmd[i]){
            resetFlag=0;
          }
        }
        if(resetFlag == 1){
          lineNo = 0;
          tft.fillScreen(TFT_BLACK);
          tft.fillScreen(TFT_BLACK);
          tft.setCursor(0, 0);
          commandnotfoundFlag = 0;
        }

        // Processing the help command
        int helpFlag=1;
        for(int i = 0; i<(sizeof(helpCmd)-1); i++){
          if(cmd[i] != helpCmd[i]){
            helpFlag=0;
          }
        }
        if(helpFlag == 1){
          lineNo = 0;
          tft.fillScreen(TFT_BLACK);
          tft.fillScreen(TFT_BLACK);
          tft.setCursor(0, 0);
          for(int i = 0; i<helpTextHeight; i++){
            for(int j = 0; j<(lcdLineLength - 1); j++){
              vPrSym(helpText[i][j]);
            }
            vPrSym(newline);
            lineNo++;
            vTaskDelay(lcdBufferTimeout/portTICK_PERIOD_MS);
          }
          commandnotfoundFlag = 0;
        }

        // Processing the ps command
        int psFlag=1;
        for(int i = 0; i<sizeof(psCmd)-1; i++){
          if(cmd[i] != psCmd[i]){
            psFlag=0;
          }
        }
        if(psFlag == 1){       
          vPrPs(blink);
          vPrPs(ttyI);
          vPrPs(ttyO);
          vPrPs(graph);
          vPrPs(shell);  
          lineNo = lineNo + 5;
          commandnotfoundFlag = 0;
        }
        
        // Processing the ls command
        // Processing the rm <filename> command
        // Processing the picview <fielname> command

        // Command not found
        if(commandnotfoundFlag == 1){
          for(int i = 0; i<sizeof(commandNotFound)-1; i++){
            vPrSym(commandNotFound[i]);
          }
          vPrSym(newline);
          lineNo++;
        }

        // Command epilog
        lineNo++;
        xQueueSendToBack( LCDQueue, &gateSymbol, 0 );
        xQueueSendToBack( LCDQueue, &space, 0 );
        xQueueSendToBack( SHELL2SerialQueue, &gateSymbol, 0 );
        xQueueSendToBack( SHELL2SerialQueue, &space, 0 );
        cmdLength = 0;
      }
    }
}
