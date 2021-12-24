#include <Adafruit_GFX.h>
#include <MCUFRIEND_kbv.h>
#include <TouchScreen.h>

// Colors
#define BACKGROUND      0x0033
#define TREE            0x00A0
#define TRUNK           0x3100
#define NODE_BORDER     0x0000
#define NODE_CONTENT    0x0000
#define ACTIVE_NODE     0xF800
#define INACTIVE_NODE   0xAA00
#define ACTIVE_EDGE     0xF800
#define INACTIVE_EDGE   0x00E0

// Screen
MCUFRIEND_kbv tft;

// Touch
const int XP=6,XM=A2,YP=A1,YM=7; //ID=0x9341
const int TS_LEFT=915,TS_RT=183,TS_TOP=953,TS_BOT=206;
TouchScreen ts = TouchScreen(XP, YP, XM, YM, 300);
bool pressed = false;
#define MINPRESSURE 200
#define MAXPRESSURE 1000

// Time
#define MILLIS_PER_DAY 86400000UL
int displayedTime[] = {0, 0, 0, 0, 0, 0};
unsigned long lastMillis = 0;
unsigned long timeOfDay = 0;

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

// calculates euclidian distance of two points
double distance(int x1, int y1, int x2, int y2) {
    return sqrt((x1-x2)*(x1-x2) + (y1-y2)*(y1-y2));
}

// draws background, christmas tree and trunk
void drawBackground() {
    tft.fillScreen(BACKGROUND);
    tft.fillTriangle(160, 0, 0, 175, 320, 175, TREE);
    tft.fillTriangle(160, 100, 0, 295, 320, 295, TREE);
    tft.fillTriangle(160, 220, 0, 415, 320, 415, TREE);
    tft.fillRect(130, 415, 60, 65, TRUNK);
}

// draws the node in (r, c), i.e. row r and column c
void drawNode(int r, int c, bool active) {
    node v = NODES[r][c];
    tft.fillCircle(v.x, v.y, CIRCLE_RADIUS, active ? ACTIVE_NODE : INACTIVE_NODE);
    tft.drawCircle(v.x, v.y, CIRCLE_RADIUS, NODE_BORDER);
    tft.drawChar(v.x-5, v.y-7, v.value + '0', NODE_CONTENT, NODE_CONTENT, 2);
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

    tft.drawLine(x1, y1, x2, y2, active ? ACTIVE_EDGE : INACTIVE_EDGE);

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
void updateTimeDisplay() {
    int tod[ROWS];
    millisToArray(timeOfDay, tod);
    for (int r = 0; r < ROWS; r++) {
        if (r + 1 < ROWS) {
            // if edge has changed
            if (tod[r] != displayedTime[r] || tod[r+1] != displayedTime[r+1]) {
                // erase old edge (draw as inactive) and draw new edge as active
                drawEdge(r, displayedTime[r], displayedTime[r+1], false);
                drawEdge(r, tod[r], tod[r+1], true);
            }
        }
        // if node has changed
        if (tod[r] != displayedTime[r]) {
            // mark both (old and new) nodes as stale
            stale[r][displayedTime[r]] = true;
            stale[r][tod[r]] = true;
        }
        displayedTime[r] = tod[r];
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

void setup() {
    // initialize screen
    unsigned int ID = tft.readID();
    Serial.begin(9600);
    Serial.println(String(ID)); 
    tft.begin(ID);

    // draw background and initial state
    drawBackground();
    for (int r = 0; r < ROWS; r++) {
        if (r + 1 < ROWS) {
            for (int c1 = 0; c1 < ROW_SIZE[r]; c1++) {
                for (int c2 = 0; c2 < ROW_SIZE[r+1]; c2++) {
                    drawEdge(r, c1, c2, displayedTime[r] == c1 && displayedTime[r+1] == c2);
                }
            }
        }
        for (int c = 0; c < ROW_SIZE[r]; c++) {
            // reset stale flag for each node
            stale[r][c] = false;
            drawNode(r, c, displayedTime[r] == c);
        }
    }
}

void loop() {
    // Time logic
    unsigned long currentMillis = millis();
    // do updates at start of second but not multiple times a second
    if (currentMillis / 1000 != lastMillis / 1000) {
        // do update
        timeOfDay += currentMillis - lastMillis;
        timeOfDay %= MILLIS_PER_DAY;
        updateTimeDisplay();
        lastMillis = currentMillis;
    }

    // Touch logic
    TSPoint tp = ts.getPoint();
    // fix direction of shared pins
    pinMode(XM, OUTPUT);
    pinMode(YP, OUTPUT);

    // check if the user touches the screen
    if (tp.z > MINPRESSURE && tp.z < MAXPRESSURE) {
        if (!pressed) {
            pressed = true;
            // map the readings to coordinates on the screen
            unsigned int xpos = map(tp.x, TS_LEFT, TS_RT, 0, tft.width());
            unsigned int ypos = map(tp.y, TS_TOP, TS_BOT, 0, tft.height());

            // check every node if it contains the given coordinates
            for (int r = 0; r < ROWS; r++) {
                for (int c = 0; c < ROW_SIZE[r]; c++) {
                    if (distance(NODES[r][c].x, NODES[r][c].y, xpos, ypos) <= CIRCLE_RADIUS) {
                        int old = displayedTime[r];
                        displayedTime[r] = c;
                        unsigned long ms = arrayToMillis(displayedTime);
                        // restore old value for proper visual updating
                        displayedTime[r] = old;

                        // only update time if input was valid
                        if (ms < MILLIS_PER_DAY) {
                            timeOfDay = ms;
                            updateTimeDisplay();
                        }
                    }
                }
            }   
        }     
    } else
        pressed = false;
}
