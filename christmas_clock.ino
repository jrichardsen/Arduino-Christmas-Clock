#include <Adafruit_GFX.h>
#include <MCUFRIEND_kbv.h>
#include <TouchScreen.h>
#include <DS3231.h>
#include <Wire.h>

// Colors
#define WELCOME_BG_COLOR        0x0000
#define WELCOME_HEART_COLOR     0xF800
#define WELCOME_TEXT_COLOR      0xFFFF
#define BACKGROUND_COLOR        0x0033
#define TREE_COLOR              0x00A0
#define TRUNK_COLOR             0x3100
#define NODE_BORDER_COLOR       0x0000
#define NODE_CONTENT_COLOR      0x0000
#define ACTIVE_NODE_COLOR       0xF800
#define INACTIVE_NODE_COLOR     0xAA00
#define ACTIVE_EDGE_COLOR       0xF800
#define INACTIVE_EDGE_COLOR     0x00E0
#define CANCEL_BUTTON_COLOR     0x8800
#define CONFIRM_BUTTON_COLOR    0x0440
#define BUTTON_CONTENT_COLOR    0xFFFF

// Screen
MCUFRIEND_kbv tft;

// Touch
const int XP=6,XM=A2,YP=A1,YM=7; //ID=0x9341
const int TS_LEFT=915,TS_RT=183,TS_TOP=953,TS_BOT=206;
TouchScreen ts = TouchScreen(XP, YP, XM, YM, 300);
bool pressing = false;
#define MINPRESSURE 200
#define MAXPRESSURE 1000

// Time
#define WELCOME_SCREEN_TIME 10000UL
DS3231 rtc;
#define POLLING_INTERVAL 86400000UL
unsigned long lastPoll = 0;
#define MILLIS_PER_DAY 86400000UL
#define TIME_DEVIATION 1.00140625
int displayedTime[] = {0, 0, 0, 0, 0, 0};
unsigned long lastMillis = 0;
unsigned long timeOfDay = 0;
unsigned long lastUpdateTime = 0;

// Graphics
struct node {
    int x; 
    int y;
    int value;
};

const int CIRCLE_RADIUS = 12;
const int ROWS = 6;
const int ROW_SIZE[] = {3, 10, 6, 10, 6, 10};

// stores position and value of all nodes
// actual dimensions of the array are contained within "ROW" and "ROW_SIZE" constants
const node NODES[][10] = {
    {
        {100, 100, 0},
        {160, 100, 1},
        {220, 100, 2},
    },
    {
        { 25, 160, 0},
        { 55, 160, 1},
        { 85, 160, 2},
        {115, 160, 3},
        {145, 160, 4},
        {175, 160, 5},
        {205, 160, 6},
        {235, 160, 7},
        {265, 160, 8},
        {295, 160, 9},
    },
    {
        { 85, 220, 0},
        {115, 220, 1},
        {145, 220, 2},
        {175, 220, 3},
        {205, 220, 4},
        {235, 220, 5},
    },
    {
        { 25, 280, 0},
        { 55, 280, 1},
        { 85, 280, 2},
        {115, 280, 3},
        {145, 280, 4},
        {175, 280, 5},
        {205, 280, 6},
        {235, 280, 7},
        {265, 280, 8},
        {295, 280, 9},
    },
    {
        { 85, 340, 0},
        {115, 340, 1},
        {145, 340, 2},
        {175, 340, 3},
        {205, 340, 4},
        {235, 340, 5},
    },
    {
        { 25, 400, 0},
        { 55, 400, 1},
        { 85, 400, 2},
        {115, 400, 3},
        {145, 400, 4},
        {175, 400, 5},
        {205, 400, 6},
        {235, 400, 7},
        {265, 400, 8},
        {295, 400, 9},
    }
};
// for each node, stores whether the node is stale, i.e. has to be redrawn
bool stale[6][10];

// Edit mode
bool inEditMode = false;
bool pressingTrunk = false;
unsigned long trunkPressStart = 0;
#define LONG_PRESS_DURATION 3000
int editTime[ROWS];

// converts a millisecond time of day to their graphical representation
// and stores that within the given array
void millisToArray(unsigned long ms, int* arr) {
    // using inverse horner
    ms /= 1000;
    for (int i = ROWS - 1; i >= 0; i--) {
        arr[i] = ms % ROW_SIZE[i];
        ms /= ROW_SIZE[i];
    }
}

// converts the array representing the time graphics to
// the time of day (in milliseconds)
unsigned long arrayToMillis(int* arr) {
    // using horner
    unsigned long seconds = 0;
    for (int i = 0; i < ROWS; i++) {
        seconds *= ROW_SIZE[i];
        seconds += arr[i];
    }
    return seconds * 1000;
}

unsigned long corrected(unsigned long time) {
    return time / TIME_DEVIATION;
}

// fetches the time from the rtc and updates timeOfDay
void fetchRTCTime() {
    bool _;
    timeOfDay = rtc.getHour(_, _) * 3600UL;
    timeOfDay += rtc.getMinute() * 60UL;
    timeOfDay += rtc.getSecond();
    timeOfDay *= 1000;
    timeOfDay *= TIME_DEVIATION;
    lastMillis = millis();
    lastPoll = lastMillis;
}

// updates the rtc with the current time
void changeRTCTime() {
    byte hour = corrected(timeOfDay) / 3600000UL;
    byte minute = (corrected(timeOfDay) % 3600000UL) / 60000UL;
    byte second = (corrected(timeOfDay) % 60000UL) / 1000UL;
    rtc.setHour(hour);
    rtc.setMinute(minute);
    rtc.setSecond(second);
}

// calculates euclidian distance of two points
double distance(int x1, int y1, int x2, int y2) {
    return sqrt((x1-x2)*(x1-x2) + (y1-y2)*(y1-y2));
}

void drawWelcomeScreen() {
    tft.fillScreen(WELCOME_BG_COLOR);
    tft.fillCircle(100, 200, 80, WELCOME_HEART_COLOR);
    tft.fillCircle(220, 200, 80, WELCOME_HEART_COLOR);
    tft.fillTriangle(43, 257, 277, 257, 160, 359, WELCOME_HEART_COLOR);
    tft.setTextSize(3);
    tft.setTextColor(WELCOME_TEXT_COLOR);
    tft.setCursor(115, 180);
    tft.print("FROHE");
    tft.setCursor(61, 220);
    tft.print("WEIHNACHTEN");
}

// draws background, christmas tree and trunk
void drawBackground() {
    tft.fillScreen(BACKGROUND_COLOR);
    tft.fillTriangle(160, 0, 0, 175, 320, 175, TREE_COLOR);
    tft.fillTriangle(160, 100, 0, 295, 320, 295, TREE_COLOR);
    tft.fillTriangle(160, 220, 0, 415, 320, 415, TREE_COLOR);
    tft.fillRect(130, 415, 60, 65, TRUNK_COLOR);
}

// draws the node in (r, c), i.e. row r and column c
void drawNode(int r, int c, bool active) {
    node v = NODES[r][c];
    tft.fillCircle(v.x, v.y, CIRCLE_RADIUS, active ? ACTIVE_NODE_COLOR : INACTIVE_NODE_COLOR);
    tft.drawCircle(v.x, v.y, CIRCLE_RADIUS, NODE_BORDER_COLOR);
    tft.drawChar(v.x-5, v.y-7, v.value + '0', NODE_CONTENT_COLOR, NODE_CONTENT_COLOR, 2);
}

// draws the edge between the nodes in (r, c1) and (r+1, c2)
void drawEdge(int r, int c1, int c2, bool active) {
    node v1 = NODES[r][c1];
    node v2 = NODES[r+1][c2];
    int x1 = v1.x;
    int y1 = v1.y;
    int x2 = v2.x;
    int y2 = v2.y;

    // only draw the line from the edges of the nodes
    double dx = x2 - x1;
    double dy = y2 - y1;
    double d = sqrt(dx*dx + dy*dy) / CIRCLE_RADIUS;
    dx /= d;
    dy /= d;
    x1 += ceil(dx);
    y1 += ceil(dy);
    x2 -= ceil(dx);
    y2 -= ceil(dy);

    tft.drawLine(x1, y1, x2, y2, active ? ACTIVE_EDGE_COLOR : INACTIVE_EDGE_COLOR);

    int diffx = (x2 - x1) / 100;
    // if x coordinates of nodes differ to much, their connecting edge will
    // overlap the neighbouring nodes
    // therefore these nodes need to be redrawn
    if (diffx != 0) {
        stale[r][c1+diffx] = true;
        stale[r+1][c2-diffx] = true;
    }
}

// displays the current timeOfDay on the screen
// the old displayedTime is erased in the process
void updateTimeDisplay(bool useEditTime) {
    int* time;
    if (useEditTime)
        time = editTime;
    else {
        int tod[6];
        millisToArray(corrected(timeOfDay), tod);
        time = tod;
    }
    for (int r = 0; r < ROWS; r++) {
        if (r + 1 < ROWS) {
            // if edge has changed
            if (time[r] != displayedTime[r] || time[r+1] != displayedTime[r+1]) {
                // erase old edge (draw as inactive) and draw new edge as active
                drawEdge(r, displayedTime[r], displayedTime[r+1], false);
                drawEdge(r, time[r], time[r+1], true);
            }
        }
        // if node has changed
        if (time[r] != displayedTime[r]) {
            // mark both (old and new) nodes as stale
            stale[r][displayedTime[r]] = true;
            stale[r][time[r]] = true;
        }
        displayedTime[r] = time[r];
    }
    // redraw the stale nodes
    for (int r = 0; r < ROWS; r++) {
        for (int c = 0; c < ROW_SIZE[r]; c++) {
            if (stale[r][c]) {
                stale[r][c] = false;
                drawNode(r, c, displayedTime[r] == c);
            }
        }
    }
}

void startEditMode() {
    inEditMode = true;

    // drawButtons
    tft.setTextSize(3);
    tft.setTextColor(BUTTON_CONTENT_COLOR);
    tft.fillRoundRect(20, 425, 90, 45, 5, CANCEL_BUTTON_COLOR);
    tft.setCursor(29, 435);
    tft.print("BACK");
    tft.fillRoundRect(210, 425, 90, 45, 5, CONFIRM_BUTTON_COLOR);
    tft.setCursor(237, 435);
    tft.print("OK");

    for (int r = 0; r < ROWS; r++) {
        editTime[r] = displayedTime[r];
    }
}

void stopEditMode(bool persist) {
    inEditMode = false;

    if (persist) {
        timeOfDay = arrayToMillis(editTime) * TIME_DEVIATION;
        changeRTCTime();
    }

    // erase Buttons
    tft.fillRect(20, 425, 90, 45, BACKGROUND_COLOR);
    tft.fillRect(210, 425, 90, 45, BACKGROUND_COLOR);

    updateTimeDisplay(false);
}

void setup() {
    // initialize i2c
    Wire.begin();
    // initialize screen
    unsigned int ID = tft.readID();
    Serial.begin(9600);
    Serial.println(String(ID)); 
    tft.begin(ID);

    // set 24 hour mode
    rtc.setClockMode(false);

    fetchRTCTime();
    millisToArray(corrected(timeOfDay), displayedTime);

    drawWelcomeScreen();
    delay(WELCOME_SCREEN_TIME);

    // draw background and initial state
    drawBackground();
    for (int r = 0; r < ROWS; r++) {
        if (r + 1 < ROWS) {
            for (int c1 = 0; c1 < ROW_SIZE[r]; c1++) {
                for (int c2 = 0; c2 < ROW_SIZE[r+1]; c2++) {
                    drawEdge(r, c1, c2, false);
                }
            }
            drawEdge(r, displayedTime[r], displayedTime[r+1], true);
        }
        for (int c = 0; c < ROW_SIZE[r]; c++) {
            stale[r][c] = false;
            drawNode(r, c, displayedTime[r] == c);
        }
    }
}

void loop() {
    // Time logic
    unsigned long currentMillis = millis();
    timeOfDay += currentMillis - lastMillis;
    timeOfDay %= (unsigned long)(MILLIS_PER_DAY * TIME_DEVIATION);
    lastMillis = currentMillis;

    // update graphics whenever the second changes
    if (!inEditMode && corrected(timeOfDay) / 1000 != corrected(lastUpdateTime) / 1000) {
        updateTimeDisplay(false);
        lastUpdateTime = timeOfDay;
    }

    // regularly fetch time from the rtc
    if (currentMillis - lastPoll > POLLING_INTERVAL)
        fetchRTCTime();

    // Touch logic
    TSPoint tp = ts.getPoint();
    // fix direction of shared pins
    pinMode(XM, OUTPUT);
    pinMode(YP, OUTPUT);

    // check if the user touches the screen
    if (tp.z > MINPRESSURE && tp.z < MAXPRESSURE) {
        // map the readings to coordinates on the screen
        unsigned int xpos = map(tp.x, TS_LEFT, TS_RT, 0, tft.width());
        unsigned int ypos = map(tp.y, TS_TOP, TS_BOT, 0, tft.height());
        if (!pressing) {
            pressing = true;
            if (inEditMode) {
                // check every node if it contains the given coordinates
                for (int r = 0; r < ROWS; r++) {
                    for (int c = 0; c < ROW_SIZE[r]; c++) {
                        if (distance(NODES[r][c].x, NODES[r][c].y, xpos, ypos) <= CIRCLE_RADIUS) {
                            int old = editTime[r];
                            editTime[r] = c;
                            unsigned long ms = arrayToMillis(editTime);
                            // only update time if input was valid
                            if (ms < MILLIS_PER_DAY)
                                updateTimeDisplay(true);
                            else
                                editTime[r] = old;
                            return;
                        }
                    }
                }

                // check for buttons   
                if (20 <= xpos && xpos <= 110 && 425 <= ypos && ypos <= 470)
                    // cancel button
                    stopEditMode(false);
                else if (210 <= xpos && xpos <= 300 && 425 <= ypos && ypos <= 470)
                    // confirm button
                    stopEditMode(true);
            } else if (130 <= xpos && xpos <= 190 && 415 <= ypos) {
                // trunk pressed
                pressingTrunk = true;
                trunkPressStart = currentMillis;
            }
        } else {
            if (!(130 <= xpos && xpos <= 190 && 415 <= ypos))
                pressingTrunk = false;
            if (!inEditMode && pressingTrunk && currentMillis - trunkPressStart >= LONG_PRESS_DURATION)
                startEditMode();
        }  
    } else {
        pressing = false;
        pressingTrunk = false;
    }
}
