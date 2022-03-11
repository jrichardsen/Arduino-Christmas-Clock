#include <Adafruit_GFX.h>
#include <MCUFRIEND_kbv.h>
#include <TouchScreen.h>
#include <DS3231.h>
#include <Wire.h>



// Colors (16 bits, RGB 565) ---------------------------------------------------
// see https://trolsoft.ru/en/articles/rgb565-color-picker

#define WELCOME_BG_COLOR        0x0000  // black
#define WELCOME_HEART_COLOR     0xF800  // red
#define WELCOME_TEXT_COLOR      0xFFFF  // white
#define BACKGROUND_COLOR        0x0033  // dark blue
#define TREE_COLOR              0x00A0  // dark green
#define TRUNK_COLOR             0x3100  // brown
#define NODE_BORDER_COLOR       0x0000  // black
#define NODE_CONTENT_COLOR      0x0000  // black
#define ACTIVE_NODE_COLOR       0xF800  // red
#define INACTIVE_NODE_COLOR     0xAA00  // yellow
#define ACTIVE_EDGE_COLOR       0xF800  // red
#define INACTIVE_EDGE_COLOR     0x00E0  // green
#define CANCEL_BUTTON_COLOR     0x8800  // dark red
#define CONFIRM_BUTTON_COLOR    0x0440  // green
#define BUTTON_CONTENT_COLOR    0xFFFF  // white



// Touch -----------------------------------------------------------------------

// Model specific constants for the LCD display
const int XP=6,XM=A2,YP=A1,YM=7; //ID=0x9341

// calibration values for the touch display
const int TS_LEFT=915,TS_RT=183,TS_TOP=953,TS_BOT=206;

TouchScreen ts = TouchScreen(XP, YP, XM, YM, 300);
// if the touchscreen is currently pressed
bool pressing = false;

// minimal pressure for registering a touch event
#define MINPRESSURE 200
// maximal pressure for registering a touch event
#define MAXPRESSURE 1000



// Time ------------------------------------------------------------------------

// factor by which the arduino internal time deviates from the real time
// makes the arduino deviate only by a few seconds over a day 
// (instead of approx. 2 minutes)
#define TIME_DEVIATION 1.00140625

// amount of milliseconds to display the welcome screen
// uses arduino internal time (not corrected time)
#define WELCOME_SCREEN_TIME 10000UL // 10 seconds

// how many milliseconds between two polls of the clock module
// uses arduino internal time (not corrected time)
#define POLLING_INTERVAL 86400000UL // 1 day

// value of the arduino internal clock at the time of the last poll
unsigned long lastPoll = 0;

// utility constant for the amount of milliseconds in a day
#define MILLIS_PER_DAY 86400000UL

// reference to the DS3231 real-time module
// connected to the arduino via the I2C interface
DS3231 rtc;

// the time currently displayed on the arduino clock
// each entry is the column of the selected node in the respective row
// e.g. 14:05:23 corresponds to {1, 4, 0, 5, 2, 3}
int displayedTime[] = {0, 0, 0, 0, 0, 0};

// the value of the arduino internal clock when the graphics were last updated
unsigned long lastMillis = 0;

// the current time of day in milliseconds
// not corrected (according to TIME_DEVIATION)
unsigned long timeOfDay = 0;

// time when the UI was last updated (not corrected)
unsigned long lastUpdateTime = 0;

// Converts a millisecond time of day to their graphical representation
// and stores that within the given array.
// e.g. 50723000 milliseconds, a.k.a 14:05:23 corresponds to {1, 4, 0, 5, 2, 3}
void millisToArray(unsigned long ms, int* arr) {
    ms /= 1000;
    for (int i = ROWS - 1; i >= 0; i--) {
        arr[i] = ms % ROW_SIZE[i];
        ms /= ROW_SIZE[i];
    }
}

// Converts the array representing the time graphics to
// the time of day (in milliseconds).
// e.g. {1, 4, 0, 5, 2, 3}, a.k.a 14:05:23 corresponds to 50723000 milliseconds
unsigned long arrayToMillis(int* arr) {
    unsigned long seconds = 0;
    for (int i = 0; i < ROWS; i++) {
        seconds *= ROW_SIZE[i];
        seconds += arr[i];
    }
    return seconds * 1000;
}

// Corrects the values from the arduino's internal clock.
unsigned long corrected(unsigned long time) {
    return time / TIME_DEVIATION;
}

// Fetches the time from the RTC and updates the time of day
void fetchRtcTime() {
    bool _;
    // ignore the 12/24 and the am/pm flags, 
    // since we should always be in 24-hour mode
    timeOfDay = rtc.getHour(_, _) * 3600UL;
    timeOfDay += rtc.getMinute() * 60UL;
    timeOfDay += rtc.getSecond();
    timeOfDay *= 1000;
    timeOfDay *= TIME_DEVIATION;
    lastMillis = millis();
    lastPoll = lastMillis;
}

// Updates the RTC with the current time of day
void changeRtcTime() {
    unsigned long corrected_time = corrected(timeOfDay);
    byte hour = corrected_time / 3600000UL;
    byte minute = (corrected_time % 3600000UL) / 60000UL;
    byte second = (corrected_time % 60000UL) / 1000UL;
    rtc.setHour(hour);
    rtc.setMinute(minute);
    rtc.setSecond(second);
}



// Graphics --------------------------------------------------------------------

// structure representing a node in the graphics
struct node {
    int x; 
    int y;
};

// reference to the display module
MCUFRIEND_kbv tft;

// the amount of rows with nodes
// note that a row is more of a structural than a graphical concept,
// i.e. nodes in the same row do not have to be on the exact same y-level.
// (but rows should still be recognizable)
const int ROWS = 6;

// the amount of nodes in each row
const int ROW_SIZE[ROWS] = {3, 10, 6, 10, 6, 10};

// radius of the nodes in pixels
const int NODE_RADIUS = 12;

// stores position of all nodes
// actual dimensions of the array are given by "ROW_SIZE"
// since rows do not all contain 10 nodes each
// i.e. only use NODES[i][j] with i < ROWS and j < ROW_SIZE[i]
const node NODES[][10] = {
    {
        {100, 100},
        {160, 100},
        {220, 100},
    },
    {
        { 25, 160},
        { 55, 160},
        { 85, 160},
        {115, 160},
        {145, 160},
        {175, 160},
        {205, 160},
        {235, 160},
        {265, 160},
        {295, 160},
    },
    {
        { 85, 220},
        {115, 220},
        {145, 220},
        {175, 220},
        {205, 220},
        {235, 220},
    },
    {
        { 25, 280},
        { 55, 280},
        { 85, 280},
        {115, 280},
        {145, 280},
        {175, 280},
        {205, 280},
        {235, 280},
        {265, 280},
        {295, 280},
    },
    {
        { 85, 340},
        {115, 340},
        {145, 340},
        {175, 340},
        {205, 340},
        {235, 340},
    },
    {
        { 25, 400},
        { 55, 400},
        { 85, 400},
        {115, 400},
        {145, 400},
        {175, 400},
        {205, 400},
        {235, 400},
        {265, 400},
        {295, 400},
    }
};

// for each node, stores whether the node is stale, i.e. has to be redrawn
// only valid for actually existing nodes
// i.e. only use stale[i][j] for i < ROWS and j < ROW_SIZE[i]
bool stale[ROWS][10];

// Calculates euclidian distance of two points (x1, y1) and (x2, y2).
double distance(int x1, int y1, int x2, int y2) {
    return sqrt((x1-x2)*(x1-x2) + (y1-y2)*(y1-y2));
}

// Computes distance of point p at (x, y) to 
// the segment from p1 at (x1, y1) to p2 at (x2, y2).
double distancePointToSegment(int x, int y, int x1, int y1, int x2, int y2) {
    // vector s from p1 to p2
    double sx = x2 - x1;
    double sy = y2 - y1;
    // vector v1 from p1 to p
    double v1x = x - x1;
    double v1y = y - y1;
    // vector v2 from p2 to p
    double v2x = x - x2;
    double v2y = y - y2;

    // use dot product to determine the position of p (relative to s)
    if (sx*v1x + sy*v1y <= 0) {
        // p is closest to p1
        return sqrt(v1x*v1x + v1y*v1y);
    }
    if (sx*v2x + sy*v2y >= 0) {
        // p is closest to p2
        return sqrt(v2x*v2x + v2y*v2y);
    }
    // here p actually lies "above" the segment,
    // i.e. there is a point q on the segment such that pq is perpendicular to s
    // calculate the area of the parallelogram spanned by s and v1 
    // and divide by the length of s to get the height of the parallelogram
    // (which is the height of p over the segment)
    return abs(v1x*sy - v1y*sx) / sqrt(sx*sx + sy*sy);
}

// Draws the welcome screen (a heart and some text).
void drawWelcomeScreen() {
    tft.fillScreen(WELCOME_BG_COLOR);

    // draw a heart consisting of two circles and some triangles
    // left side of the heart
    tft.fillCircle(100, 200, 80, WELCOME_HEART_COLOR);
    // right side of the heart
    tft.fillCircle(220, 200, 80, WELCOME_HEART_COLOR);
    // fill the middle of the heart
    tft.fillTriangle(43, 257, 277, 257, 160, 240, WELCOME_HEART_COLOR);
    // the bottom portion of the heart
    tft.fillTriangle(43, 257, 277, 257, 160, 359, WELCOME_HEART_COLOR);

    // write a message into the heart
    tft.setTextSize(3);
    tft.setTextColor(WELCOME_TEXT_COLOR);
    // cursor positions are set such that text will be centered on the heart
    tft.setCursor(115, 180);
    tft.print("FROHE");
    tft.setCursor(61, 220);
    tft.print("WEIHNACHTEN");
}

// Draws background, christmas tree and trunk.
void drawBackground() {
    tft.fillScreen(BACKGROUND_COLOR);
    // top triangle of tree
    tft.fillTriangle(160, 0, 0, 175, 320, 175, TREE_COLOR);
    // middle triangle of tree
    tft.fillTriangle(160, 100, 0, 295, 320, 295, TREE_COLOR);
    // bottom triangle of tree
    tft.fillTriangle(160, 220, 0, 415, 320, 415, TREE_COLOR);
    // trunk
    tft.fillRect(130, 415, 60, 65, TRUNK_COLOR);
}

// Draws the node in row r and column c.
void drawNode(int r, int c) {
    node v = NODES[r][c];
    // a node is active 
    // iff its column matches the value in displayedTime for its row
    bool active = displayedTime[r] == c;
    uint16_t color = active ? ACTIVE_NODE_COLOR : INACTIVE_NODE_COLOR;
    tft.fillCircle(v.x, v.y, NODE_RADIUS, color);
    tft.drawCircle(v.x, v.y, NODE_RADIUS, NODE_BORDER_COLOR);
    // value of the node is just the column
    // use digit to char conversion
    char val = c + '0';
    // node content is not perfectly centered
    // since chars have even dimensions and circles have odd dimensions
    // (but close enough)
    tft.drawChar(v.x-5, v.y-7, val, NODE_CONTENT_COLOR, NODE_CONTENT_COLOR, 2);
}

// Draws the edge between the nodes in (r, c1) and (r+1, c2)
// and marks all nodes which are covered by the edge as stale.
// Does not overdraw the source and destination node 
// and does not mark these nodes as stale either.
void drawEdge(int r, int c1, int c2, bool active) {
    node v1 = NODES[r][c1];
    node v2 = NODES[r+1][c2];

    // extract coordinates of the nodes
    int x1 = v1.x;
    int y1 = v1.y;
    int x2 = v2.x;
    int y2 = v2.y;

    double dx = x2 - x1;
    double dy = y2 - y1;
    // calculate the ratio of the nodes' distance to the node radius
    double d = sqrt(dx*dx + dy*dy) / NODE_RADIUS;
    dx /= d;
    dy /= d;
    // offset the node coordinates by dx and dy
    // use ceiling function to avoid overdrawing the nodes
    x1 += ceil(dx);
    y1 += ceil(dy);
    x2 -= ceil(dx);
    y2 -= ceil(dy);

    uint16_t color = active ? ACTIVE_EDGE_COLOR : INACTIVE_EDGE_COLOR;
    // only draw the line from the edges of the nodes
    tft.drawLine(x1, y1, x2, y2, color);

    // check if the edge draws over any nodes
    for (int nr = 0; nr < ROWS; nr++) {
        for (int nc = 0; nc < ROW_SIZE[nr]; nc++) {
            node n = NODES[nr][nc];
            // calculate distance of the node's center to the edge
            double distance = distancePointToSegment(n.x, n.y, x1, y1, x2, y2);
            // set the node stale if it is too close to the edge
            stale[nr][nc] = stale[nr][nc] || distance < NODE_RADIUS;
        }
    }
}

// Displays the current (corrected) timeOfDay on the screen.
// In edit mode it uses the edit time instead.
void updateTimeDisplay() {
    // determine the source of the time (editTime or timeOfDay)
    int* time;
    if (inEditMode)
        time = editTime;
    else {
        // convert timeOfDay to the displayable array format
        int tod[ROWS];
        millisToArray(corrected(timeOfDay), tod);
        time = tod;
    }

    // draw the time onto the screen
    for (int r = 0; r < ROWS; r++) {
        if (r + 1 < ROWS) {
            // check if edge has changed
            if (time[r] != displayedTime[r] || time[r+1] != displayedTime[r+1]) {
                // erase old edge (draw as inactive)
                drawEdge(r, displayedTime[r], displayedTime[r+1], false);
                // draw new edge as active
                drawEdge(r, time[r], time[r+1], true);
            }
        }
        // if active node has changed
        if (time[r] != displayedTime[r]) {
            // both nodes (old and new) need to be redrawn
            stale[r][displayedTime[r]] = true;
            stale[r][time[r]] = true;
        }
        // update the display time
        displayedTime[r] = time[r];
    }
    // redraw the stale nodes
    for (int r = 0; r < ROWS; r++) {
        for (int c = 0; c < ROW_SIZE[r]; c++) {
            if (stale[r][c]) {
                stale[r][c] = false;
                drawNode(r, c);
            }
        }
    }
}



// Edit mode -------------------------------------------------------------------
// Edit mode is activated by pressing on the trunk of the tree
// for at least 3 seconds.
// Then time can be changed by selecting the respective nodes
// Edit mode is exited by using the buttons at the bottom of the screen
// (only visible in edit mode)

// whether edit mode is currently active
bool inEditMode = false;

// whether trunk is currently pressed
bool pressingTrunk = false;

// the clock of the arduino internal time when the trunk press started, i.e. was
// first registered
// if pressingTrunk is false, the value has no meaning
unsigned long trunkPressStart = 0;

// how long the trunk needs to be pressed to enable edit mode
#define LONG_PRESS_DURATION 3000

// the time currently set in the edit mode
int editTime[ROWS];

// Enters the edit mode.
void enterEditMode() {
    inEditMode = true;

    tft.setTextSize(3);
    tft.setTextColor(BUTTON_CONTENT_COLOR);

    // draw back button
    tft.fillRoundRect(20, 425, 90, 45, 5, CANCEL_BUTTON_COLOR);
    tft.setCursor(29, 435);
    tft.print("BACK");

    // draw confirm button
    tft.fillRoundRect(210, 425, 90, 45, 5, CONFIRM_BUTTON_COLOR);
    tft.setCursor(237, 435);
    tft.print("OK");

    // set the edit time as the currently displayed time
    for (int r = 0; r < ROWS; r++) {
        editTime[r] = displayedTime[r];
    }
}

// Exits the edit mode and
// optionally persists the saved time to the RTC module.
void exitEditMode(bool persist) {
    inEditMode = false;

    if (persist) {
        // converts the edit time to milliseconds and un-corrects it
        timeOfDay = arrayToMillis(editTime) * TIME_DEVIATION;
        // persist the updated time of day to the RTC
        changeRtcTime();
    }

    // erase buttons
    tft.fillRect(20, 425, 90, 45, BACKGROUND_COLOR);
    tft.fillRect(210, 425, 90, 45, BACKGROUND_COLOR);

    updateTimeDisplay();
}



// Main section ----------------------------------------------------------------

// Executed once at the start of the program.
void setup() {
    // initialize I2C
    Wire.begin();
    // initialize screen
    unsigned int ID = tft.readID();
    tft.begin(ID);

    // initialize serial interface
    Serial.begin(9600);
    Serial.println(String(ID));

    // set 24 hour mode
    rtc.setClockMode(false);

    // draw a welcome screen and wait until continuing
    drawWelcomeScreen();
    delay(WELCOME_SCREEN_TIME);

    // get the time from the RTC module
    fetchRtcTime();
    millisToArray(corrected(timeOfDay), displayedTime);

    // draw background
    drawBackground();

    // draw the initial time (including inactive nodes/edges)
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
            // reset the stale flag for each node
            stale[r][c] = false;
            drawNode(r, c);
        }
    }
}

// Executed regularly after the setup method has finished.
void loop() {

    // Time logic
    unsigned long currentMillis = millis();
    // this will still work, even if currentMillis overflows
    timeOfDay += currentMillis - lastMillis;
    timeOfDay %= (unsigned long)(MILLIS_PER_DAY * TIME_DEVIATION);
    lastMillis = currentMillis;

    unsigned long currentSecond = corrected(timeOfDay) / 1000;
    unsigned long lastSecond = corrected(lastUpdateTime) / 1000;
    // update graphics whenever the second changes
    if (!inEditMode && currentSecond != lastSecond) {
        updateTimeDisplay();
        lastUpdateTime = timeOfDay;
    }

    // regularly fetch time from the RTC
    if (currentMillis - lastPoll > POLLING_INTERVAL)
        fetchRtcTime();

    // Touch logic
    TSPoint tp = ts.getPoint();
    // fix direction of shared pins
    pinMode(XM, OUTPUT);
    pinMode(YP, OUTPUT);

    // check if the user touches the screen
    if (tp.z > MINPRESSURE && tp.z < MAXPRESSURE) {
        // map the readings to coordinates on the screen
        unsigned int xPos = map(tp.x, TS_LEFT, TS_RT, 0, tft.width());
        unsigned int yPos = map(tp.y, TS_TOP, TS_BOT, 0, tft.height());

        if (!pressing) {
            // press has just started
            pressing = true;

            if (inEditMode) {
                // check if nodes where pressed
                for (int r = 0; r < ROWS; r++) {
                    for (int c = 0; c < ROW_SIZE[r]; c++) {
                        node n = NODES[r][c];
                        if (distance(n.x, n.y, xPos, yPos) <= NODE_RADIUS) {
                            int old = editTime[r];
                            editTime[r] = c;
                            unsigned long ms = arrayToMillis(editTime);
                            // only update time if input was valid
                            if (ms < MILLIS_PER_DAY)
                                updateTimeDisplay();
                            else
                                editTime[r] = old;
                            // do not look for other nodes, just stop here
                            return;
                        }
                    }
                }

                // check if buttons where pressed   
                if (20 <= xPos && xPos <= 110 && 425 <= yPos && yPos <= 470)
                    // cancel button
                    exitEditMode(false);
                else if (210 <= xPos && xPos <= 300 && 425 <= yPos && yPos <= 470)
                    // confirm button
                    exitEditMode(true);
            } else if (130 <= xPos && xPos <= 190 && 415 <= yPos) {
                // trunk pressed
                pressingTrunk = true;
                trunkPressStart = currentMillis;
            }

        } else {
            // check if trunk is still pressed
            if (!(130 <= xPos && xPos <= 190 && 415 <= yPos))
                pressingTrunk = false;

            unsigned long trunkPressTime = currentMillis - trunkPressStart;
            bool longTrunkPress = trunkPressTime >= LONG_PRESS_DURATION;
            if (!inEditMode && pressingTrunk && longTrunkPress)
                enterEditMode();
        }  
    } else {
        pressing = false;
        pressingTrunk = false;
    }
}
