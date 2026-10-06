#include <windows.h>
#include <GL/glut.h>
#include <cmath>
#include <cstdlib>
#include <algorithm>
#include <cstdio>

// ================================================================
//  GRANDMASTER ARENA - 3D CHESS TOURNAMENT
//  OpenGL + GLUT / FreeGLUT (single-file Code::Blocks project)
// ================================================================

const float PI = 3.14159265358979323846f;

// ---------------- Camera / viewing transformation ----------------
float camX = 0.0f, camY = 5.0f, camZ = 18.0f;
float camYaw = -90.0f;
float camPitch = -10.0f;

bool mouseDragging = false;
int lastMouseX = 0, lastMouseY = 0;

// ---------------- Individual chess-piece control ----------------
//
// All 32 pieces are stored independently.  The board itself stays fixed.
// This is a graphics-project movement system, not a full chess-rules engine.
//
struct ChessPieceState
{
    int type;       // 1 pawn, 2 rook, 3 knight, 4 bishop, 5 queen, 6 king
    bool white;
    int row, col;   // current square: 0..7
    int homeRow, homeCol;
    bool captured;
    float angle;
    float scale;
};

ChessPieceState chessPieces[32];
int selectedPiece = 3; // white queen initially

const char* pieceTypeName(int type)
{
    switch(type)
    {
        case 1: return "PAWN";
        case 2: return "ROOK";
        case 3: return "KNIGHT";
        case 4: return "BISHOP";
        case 5: return "QUEEN";
        case 6: return "KING";
        default: return "PIECE";
    }
}

void initChessPieces()
{
    int back[8] = {2,3,4,5,6,4,3,2};
    int index = 0;

    // White back row
    for (int c=0;c<8;++c)
    {
        chessPieces[index].type = back[c];
        chessPieces[index].white = true;
        chessPieces[index].row = 0;
        chessPieces[index].col = c;
        chessPieces[index].homeRow = 0;
        chessPieces[index].homeCol = c;
        chessPieces[index].captured = false;
        chessPieces[index].angle = 0.0f;
        chessPieces[index].scale = 1.0f;
        ++index;
    }

    // White pawns
    for (int c=0;c<8;++c)
    {
        chessPieces[index].type = 1;
        chessPieces[index].white = true;
        chessPieces[index].row = 1;
        chessPieces[index].col = c;
        chessPieces[index].homeRow = 1;
        chessPieces[index].homeCol = c;
        chessPieces[index].captured = false;
        chessPieces[index].angle = 0.0f;
        chessPieces[index].scale = 1.0f;
        ++index;
    }

    // Black pawns
    for (int c=0;c<8;++c)
    {
        chessPieces[index].type = 1;
        chessPieces[index].white = false;
        chessPieces[index].row = 6;
        chessPieces[index].col = c;
        chessPieces[index].homeRow = 6;
        chessPieces[index].homeCol = c;
        chessPieces[index].captured = false;
        chessPieces[index].angle = 0.0f;
        chessPieces[index].scale = 1.0f;
        ++index;
    }

    // Black back row
    for (int c=0;c<8;++c)
    {
        chessPieces[index].type = back[c];
        chessPieces[index].white = false;
        chessPieces[index].row = 7;
        chessPieces[index].col = c;
        chessPieces[index].homeRow = 7;
        chessPieces[index].homeCol = c;
        chessPieces[index].captured = false;
        chessPieces[index].angle = 0.0f;
        chessPieces[index].scale = 1.0f;
        ++index;
    }

    selectedPiece = 3;
}

int chessPieceAt(int row, int col, int ignoreIndex=-1)
{
    for (int i=0;i<32;++i)
    {
        if (i==ignoreIndex || chessPieces[i].captured)
            continue;

        if (chessPieces[i].row==row && chessPieces[i].col==col)
            return i;
    }

    return -1;
}

void selectNextPieceOfColor(bool white)
{
    int start = selectedPiece;

    for (int step=1; step<=32; ++step)
    {
        int i = (start + step) % 32;
        if (!chessPieces[i].captured && chessPieces[i].white==white)
        {
            selectedPiece = i;
            return;
        }
    }
}

void selectAdjacentPiece(int direction)
{
    int start = selectedPiece;

    for (int step=1; step<=32; ++step)
    {
        int i = (start + direction*step + 3200) % 32;
        if (!chessPieces[i].captured)
        {
            selectedPiece = i;
            return;
        }
    }
}

void moveSelectedPiece(int dRow, int dCol)
{
    ChessPieceState &p = chessPieces[selectedPiece];

    if (p.captured) return;

    int oldRow = p.row;
    int oldCol = p.col;
    int newRow = p.row + dRow;
    int newCol = p.col + dCol;

    if (newRow<0 || newRow>7 || newCol<0 || newCol>7)
        return;

    int target = chessPieceAt(newRow,newCol,selectedPiece);

    if (target>=0)
    {
        if (chessPieces[target].white == p.white)
        {
            // For this graphics demo, swap friendly pieces so a requested
            // transformation always remains visible.
            chessPieces[target].row = oldRow;
            chessPieces[target].col = oldCol;
        }
        else
        {
            // Opponent on the destination square is captured.
            chessPieces[target].captured = true;
        }
    }

    p.row = newRow;
    p.col = newCol;
}

void moveSelectedPieceForward(int direction)
{
    // White advances toward larger row numbers; black toward smaller rows.
    int dRow = chessPieces[selectedPiece].white ? direction : -direction;
    moveSelectedPiece(dRow,0);
}

void resetSelectedChessPiece()
{
    ChessPieceState &p = chessPieces[selectedPiece];

    int occupant = chessPieceAt(p.homeRow,p.homeCol,selectedPiece);
    if (occupant<0)
    {
        p.row = p.homeRow;
        p.col = p.homeCol;
    }

    p.angle = 0.0f;
    p.scale = 1.0f;
}

void resetAllChessPieces()
{
    initChessPieces();
}

// ---------------- Animation / lighting / environment ---------------
float fanAngle = 0.0f;
bool fanRunning = true;

// Indoor lighting has THREE stages:
// 0 = normal, 1 = low light, 2 = dark (live display still glows)
int lightStage = 0;

// Outdoor scene: false = morning, true = night
bool isNight = false;

// Door animation
bool doorOpen = false;
float doorAngle = 0.0f;
float doorTargetAngle = 0.0f;

// Moving cars in the outdoor scene
float car1X = -11.0f;
float car2X =  12.0f;
float car3X = -1.0f;

// Small animation used by the active-piece selection ring.
float selectionPulse = 0.0f;

int windowWidth = 1280;
int windowHeight = 720;

// ---------------- Textures ----------------
GLuint woodTex = 0;
GLuint wallTex = 0;
GLuint floorTex = 0;
GLuint brickTex = 0;

GLUquadric* quadric = NULL;

// ================================================================
// MATERIALS
// ================================================================
void setMaterial(float ar, float ag, float ab,
                 float dr, float dg, float db,
                 float sr, float sg, float sb,
                 float shininess)
{
    GLfloat ambient[]  = { ar, ag, ab, 1.0f };
    GLfloat diffuse[]  = { dr, dg, db, 1.0f };
    GLfloat specular[] = { sr, sg, sb, 1.0f };

    glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, ambient);
    glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, diffuse);
    glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, specular);
    glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, shininess);
}

void matWhiteChess()
{
    setMaterial(0.26f,0.25f,0.21f, 0.93f,0.89f,0.74f, 0.98f,0.98f,0.92f, 92.0f);
}
void matBlackChess()
{
    setMaterial(0.03f,0.03f,0.03f, 0.10f,0.10f,0.12f, 0.85f,0.85f,0.90f, 118.0f);
}
void matWood()
{
    setMaterial(0.16f,0.07f,0.02f, 0.55f,0.24f,0.07f, 0.35f,0.22f,0.10f, 32.0f);
}
void matMetal()
{
    setMaterial(0.15f,0.15f,0.17f, 0.45f,0.47f,0.52f, 0.95f,0.95f,0.95f, 100.0f);
}
void matWall()
{
    setMaterial(0.25f,0.23f,0.20f, 0.78f,0.72f,0.64f, 0.12f,0.12f,0.12f, 12.0f);
}
void matGreen()
{
    setMaterial(0.05f,0.12f,0.04f, 0.12f,0.45f,0.12f, 0.12f,0.18f,0.10f, 18.0f);
}
void matBlue()
{
    setMaterial(0.03f,0.07f,0.15f, 0.12f,0.32f,0.70f, 0.40f,0.50f,0.80f, 45.0f);
}
void matRed()
{
    setMaterial(0.14f,0.03f,0.03f, 0.65f,0.10f,0.08f, 0.45f,0.25f,0.20f, 40.0f);
}
void matGold()
{
    setMaterial(0.22f,0.16f,0.03f, 0.78f,0.56f,0.10f, 0.95f,0.80f,0.35f, 95.0f);
}
void matDarkScreen()
{
    setMaterial(0.01f,0.01f,0.015f, 0.025f,0.035f,0.05f, 0.25f,0.35f,0.45f, 70.0f);
}
void matSkin()
{
    // Slightly lower specular than before so skin reads matte rather than
    // plastic/glossy, with a warm undertone.
    setMaterial(0.22f,0.13f,0.08f, 0.72f,0.47f,0.30f, 0.18f,0.14f,0.10f, 16.0f);
}
void matHair()
{
    setMaterial(0.02f,0.015f,0.01f, 0.05f,0.04f,0.03f, 0.14f,0.14f,0.14f, 18.0f);
}
void matFloorPolished()
{
    // Noticeably glossier and tighter-specular than matWood(), so the
    // floor reads as a lacquered/varnished hardwood surface with real
    // highlights instead of a flat matte texture.
    setMaterial(0.14f,0.09f,0.04f, 0.68f,0.46f,0.24f, 0.60f,0.58f,0.52f, 96.0f);
}

// ================================================================
// FORWARD DECLARATIONS
// (These two are DEFINED later, in the ROOM DECOR section, but are
//  USED earlier by drawSignboard()/drawEidDecor() and drawHouseExterior()'s
//  neighbours. Without these declarations the file fails to compile with
//  "was not declared in this scope" errors.)
// ================================================================
void draw3DText(float x, float y, float z, float scale, float rotY, const char* text);
void drawPoster(float x, float y, float z, float rotY,
                float panelW, float panelH,
                const char* line1, const char* line2);
void drawTrophy(float x, float y, float z);

// ================================================================
// PROCEDURAL TEXTURES - no external image files needed
// ================================================================
GLuint makeTexture(unsigned char* data, int w, int h)
{
    GLuint tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, w, h, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
    return tex;
}

void createTextures()
{
    const int W = 64, H = 64;
    static unsigned char wood[W*H*3];
    static unsigned char wall[W*H*3];
    static unsigned char floorData[W*H*3];
    static unsigned char brickData[W*H*3];

    for (int y=0; y<H; ++y)
    {
        for (int x=0; x<W; ++x)
        {
            int i = (y*W+x)*3;
            int stripe = ((x/4) + (int)(3.0f*std::sin(y*0.35f))) % 8;
            unsigned char base = (unsigned char)(95 + stripe*7);
            wood[i+0] = (unsigned char)std::min(180, (int)base+35);
            wood[i+1] = (unsigned char)std::min(110, (int)base/2+18);
            wood[i+2] = (unsigned char)std::min(70,  (int)base/3+8);

            bool mortar = (y%16 < 2) || (x%32 < 2);
            if (((y/16)%2)==1) mortar = mortar || ((x+16)%32 < 2);
            if (mortar) { wall[i+0] = 185; wall[i+1] = 180; wall[i+2] = 170; }
            else        { wall[i+0] = 218; wall[i+1] = 205; wall[i+2] = 188; }

            bool bmortar = (y%16 < 2) || (x%32 < 2);
            if (((y/16)%2)==1) bmortar = bmortar || ((x+16)%32 < 2);
            int noise = ((x*7 + y*13) % 18) - 9;
            if (bmortar)
            {
                // Light blue-gray mortar between the bricks
                brickData[i+0] = 165;
                brickData[i+1] = 180;
                brickData[i+2] = 195;
            }
            else
            {
                // Medium architectural blue bricks.
                // The small noise variation keeps the brick texture visible.
                int br = 85  + noise;
                int bg = 145 + noise/2;
                int bb = 205 + noise/3;

                brickData[i+0] = (unsigned char)std::max(0,std::min(255,br));
                brickData[i+1] = (unsigned char)std::max(0,std::min(255,bg));
                brickData[i+2] = (unsigned char)std::max(0,std::min(255,bb));
            }

            int plankX = x/8, plankY = y/8;
            bool seam = (x%8==0) || (y%8==0);
            bool darkPlank = ((plankX+plankY)%2==0);
            int grain = (int)(5.0f*std::sin((x+plankY*3)*0.9f) + 5.0f*std::sin(y*0.5f));
            int fr = (darkPlank?118:150) + grain;
            int fg = (darkPlank?78 :100) + (int)(grain*0.6f);
            int fb = (darkPlank?42 :55 ) + (int)(grain*0.4f);
            if (seam) { fr = (int)(fr*0.45f); fg = (int)(fg*0.45f); fb = (int)(fb*0.45f); }
            floorData[i+0] = (unsigned char)std::max(0,std::min(235,fr));
            floorData[i+1] = (unsigned char)std::max(0,std::min(235,fg));
            floorData[i+2] = (unsigned char)std::max(0,std::min(235,fb));
        }
    }

    woodTex  = makeTexture(wood, W, H);
    wallTex  = makeTexture(wall, W, H);
    floorTex = makeTexture(floorData, W, H);
    brickTex = makeTexture(brickData, W, H);
}

// ================================================================
// BASIC GEOMETRY
// ================================================================
void drawBox(float w, float h, float d)
{
    glPushMatrix();
    glScalef(w,h,d);
    glutSolidCube(1.0);
    glPopMatrix();
}

void drawTexturedBox(float w, float h, float d, GLuint tex,
                     float repeatX=1.0f, float repeatY=1.0f)
{
    const float x=w*0.5f, y=h*0.5f, z=d*0.5f;

    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

    glBegin(GL_QUADS);
    // Front (+Z)
    glNormal3f(0,0,1);
    glTexCoord2f(0,0);               glVertex3f(-x,-y, z);
    glTexCoord2f(repeatX,0);         glVertex3f( x,-y, z);
    glTexCoord2f(repeatX,repeatY);   glVertex3f( x, y, z);
    glTexCoord2f(0,repeatY);         glVertex3f(-x, y, z);

    // Back (-Z)
    glNormal3f(0,0,-1);
    glTexCoord2f(0,0);               glVertex3f( x,-y,-z);
    glTexCoord2f(repeatX,0);         glVertex3f(-x,-y,-z);
    glTexCoord2f(repeatX,repeatY);   glVertex3f(-x, y,-z);
    glTexCoord2f(0,repeatY);         glVertex3f( x, y,-z);

    // Right (+X)
    glNormal3f(1,0,0);
    glTexCoord2f(0,0);               glVertex3f( x,-y, z);
    glTexCoord2f(repeatX,0);         glVertex3f( x,-y,-z);
    glTexCoord2f(repeatX,repeatY);   glVertex3f( x, y,-z);
    glTexCoord2f(0,repeatY);         glVertex3f( x, y, z);

    // Left (-X)
    glNormal3f(-1,0,0);
    glTexCoord2f(0,0);               glVertex3f(-x,-y,-z);
    glTexCoord2f(repeatX,0);         glVertex3f(-x,-y, z);
    glTexCoord2f(repeatX,repeatY);   glVertex3f(-x, y, z);
    glTexCoord2f(0,repeatY);         glVertex3f(-x, y,-z);

    // Top (+Y)
    glNormal3f(0,1,0);
    glTexCoord2f(0,0);               glVertex3f(-x, y, z);
    glTexCoord2f(repeatX,0);         glVertex3f( x, y, z);
    glTexCoord2f(repeatX,repeatY);   glVertex3f( x, y,-z);
    glTexCoord2f(0,repeatY);         glVertex3f(-x, y,-z);

    // Bottom (-Y)
    glNormal3f(0,-1,0);
    glTexCoord2f(0,0);               glVertex3f(-x,-y,-z);
    glTexCoord2f(repeatX,0);         glVertex3f( x,-y,-z);
    glTexCoord2f(repeatX,repeatY);   glVertex3f( x,-y, z);
    glTexCoord2f(0,repeatY);         glVertex3f(-x,-y, z);
    glEnd();

    glDisable(GL_TEXTURE_2D);
}

void drawCylinderY(float radius, float height, int slices=24)
{
    if (!quadric) return;
    glPushMatrix();
    glRotatef(-90.0f,1,0,0); // GLU cylinder's Z axis -> +Y
    gluCylinder(quadric, radius, radius, height, slices, 1);
    gluDisk(quadric, 0.0, radius, slices, 1);
    glTranslatef(0,0,height);
    gluDisk(quadric, 0.0, radius, slices, 1);
    glPopMatrix();
}

void drawCylinderZ(float radius, float length, int slices=24)
{
    if (!quadric) return;
    gluCylinder(quadric, radius, radius, length, slices, 1);
}

// A tapered cylinder along +Z (GLU's native cylinder axis, so no extra
// rotation is needed here), with caps at both ends. Used for limbs
// (arms/forearms) that extend forward rather than stand upright.
void drawTaperedCylinderZ(float baseRadius, float topRadius, float length, int slices=20)
{
    if (!quadric) return;
    gluCylinder(quadric, baseRadius, topRadius, length, slices, 2);
    if (baseRadius > 0.001f) gluDisk(quadric, 0.0, baseRadius, slices, 1);
    glPushMatrix();
    glTranslatef(0,0,length);
    if (topRadius > 0.001f) gluDisk(quadric, 0.0, topRadius, slices, 1);
    glPopMatrix();
}

// A tapered cylinder (different base/top radius) standing along +Y, with
// caps at both ends. This is what gives the chess pieces their turned,
// lathe-like profile (necks, collars, tapered stems) instead of looking
// like stacked straight drums.
void drawTaperedCylinderY(float baseRadius, float topRadius, float height, int slices=28)
{
    if (!quadric) return;
    glPushMatrix();
    glRotatef(-90.0f,1,0,0);
    gluCylinder(quadric, baseRadius, topRadius, height, slices, 2);
    if (baseRadius > 0.001f) gluDisk(quadric, 0.0, baseRadius, slices, 1);
    glTranslatef(0,0,height);
    if (topRadius > 0.001f) gluDisk(quadric, 0.0, topRadius, slices, 1);
    glPopMatrix();
}

// ================================================================
// ROOM + WINDOW + OUTDOOR ENVIRONMENT
// ================================================================
void drawTree(float x, float z, float s)
{
    glPushMatrix();
    glTranslatef(x,0,z);
    glScalef(s,s,s);

    // trunk
    setMaterial(0.12f,0.05f,0.01f, 0.35f,0.14f,0.04f, 0.12f,0.08f,0.03f, 10.0f);
    glPushMatrix();
    glTranslatef(0,0.0f,0);
    drawCylinderY(0.20f,2.3f,20);
    glPopMatrix();

    // foliage
    matGreen();
    glPushMatrix(); glTranslatef(0,2.5f,0); glutSolidSphere(0.9,20,16); glPopMatrix();
    glPushMatrix(); glTranslatef(-0.45f,2.15f,0.15f); glutSolidSphere(0.65,18,14); glPopMatrix();
    glPushMatrix(); glTranslatef(0.5f,2.15f,-0.1f); glutSolidSphere(0.7,18,14); glPopMatrix();

    glPopMatrix();
}

void drawRoadLamp(float x, float z)
{
    // pole
    matMetal();
    glPushMatrix();
    glTranslatef(x,0.0f,z);
    drawCylinderY(0.08f,3.2f,18);
    glPopMatrix();

    // small arm
    glPushMatrix();
    glTranslatef(x,3.13f,z-0.28f);
    drawBox(0.10f,0.10f,0.60f);
    glPopMatrix();

    // lamp housing
    glPushMatrix();
    glTranslatef(x,3.05f,z-0.58f);
    drawBox(0.38f,0.18f,0.28f);
    glPopMatrix();

    // bulb is deliberately unlit so it visibly glows at night
    GLboolean lightingWasOn = glIsEnabled(GL_LIGHTING);
    if (lightingWasOn) glDisable(GL_LIGHTING);
    if (isNight) glColor3f(1.0f,0.82f,0.35f);
    else         glColor3f(0.86f,0.84f,0.72f);
    glPushMatrix();
    glTranslatef(x,2.93f,z-0.60f);
    glutSolidSphere(0.12f,16,12);
    glPopMatrix();
    if (lightingWasOn) glEnable(GL_LIGHTING);
}

// A simple lit, low-poly car: body, cabin, four wheels and headlight/taillight
// dots. rotY=0 points the car toward +X; use 180 for the opposite direction.
void drawCar(float x, float z, float rotY, float bodyR, float bodyG, float bodyB)
{
    glPushMatrix();
    glTranslatef(x,0,z);
    glRotatef(rotY,0,1,0);

    setMaterial(bodyR*0.25f,bodyG*0.25f,bodyB*0.25f, bodyR,bodyG,bodyB, 0.55f,0.55f,0.55f, 70.0f);
    glPushMatrix(); glTranslatef(0,0.34f,0); drawBox(1.75f,0.38f,0.82f); glPopMatrix();
    glPushMatrix(); glTranslatef(-0.05f,0.62f,0); drawBox(0.95f,0.30f,0.74f); glPopMatrix();

    setMaterial(0.02f,0.02f,0.03f, 0.06f,0.06f,0.07f, 0.30f,0.30f,0.30f, 40.0f);
    for (int sx=-1; sx<=1; sx+=2)
    for (int sz=-1; sz<=1; sz+=2)
    {
        glPushMatrix();
        glTranslatef(sx*0.58f,0.15f,sz*0.42f);
        glRotatef(90,1,0,0);
        drawCylinderZ(0.16f,0.10f,14);
        glPopMatrix();
    }

    GLboolean lightingWasOn2 = glIsEnabled(GL_LIGHTING);
    if (lightingWasOn2) glDisable(GL_LIGHTING);
    glColor3f(1.0f,0.95f,0.75f);
    glPushMatrix(); glTranslatef(0.86f,0.34f,0.26f); glutSolidSphere(0.06f,10,8); glPopMatrix();
    glPushMatrix(); glTranslatef(0.86f,0.34f,-0.26f); glutSolidSphere(0.06f,10,8); glPopMatrix();
    glColor3f(0.75f,0.05f,0.05f);
    glPushMatrix(); glTranslatef(-0.86f,0.34f,0.26f); glutSolidSphere(0.055f,10,8); glPopMatrix();
    glPushMatrix(); glTranslatef(-0.86f,0.34f,-0.26f); glutSolidSphere(0.055f,10,8); glPopMatrix();
    if (lightingWasOn2) glEnable(GL_LIGHTING);

    glPopMatrix();
}

void drawCitySkyline()
{
    // Distant building silhouettes seen through the window, unlit so their
    // flat sunset/night colour reads correctly regardless of room light.
    // Two depth layers (a hazy far row and a richer near row) give a simple
    // sense of atmospheric depth without needing any textures.
    GLboolean lightingWasOn = glIsEnabled(GL_LIGHTING);
    if (lightingWasOn) glDisable(GL_LIGHTING);

    struct Bldg { float x, w, h, depth, z; bool antenna; bool tank; };

    static const Bldg bldgsFar[] = {
        {-13.5f,1.6f,4.5f,0.30f,-25.2f,false,false}, {-11.2f,1.1f,6.5f,0.30f,-25.2f,true, false},
        {-9.4f, 1.8f,3.6f,0.30f,-25.2f,false,true},  {-6.8f, 1.3f,7.5f,0.30f,-25.2f,true, false},
        {-4.5f, 1.6f,5.0f,0.30f,-25.2f,false,false}, {-1.8f, 1.0f,8.2f,0.30f,-25.2f,true, false},
        { 0.6f, 1.7f,4.8f,0.30f,-25.2f,false,true},  { 3.0f, 1.2f,6.8f,0.30f,-25.2f,true, false},
        { 5.5f, 1.9f,3.8f,0.30f,-25.2f,false,false}, { 8.0f, 1.3f,7.0f,0.30f,-25.2f,true, false},
        {10.4f, 1.6f,5.2f,0.30f,-25.2f,false,false}, {12.8f, 1.1f,6.0f,0.30f,-25.2f,false,true}
    };
    static const Bldg bldgsNear[] = {
        {-14.6f,2.0f,3.2f,0.35f,-23.6f,false,false}, {-8.2f, 2.4f,2.4f,0.35f,-23.6f,false,false},
        {-2.6f, 2.1f,3.6f,0.35f,-23.6f,true, false}, { 2.2f, 2.3f,2.8f,0.35f,-23.6f,false,true},
        { 7.4f, 2.0f,3.4f,0.35f,-23.6f,false,false}, {13.6f, 2.2f,2.6f,0.35f,-23.6f,false,false}
    };

    for (int pass=0; pass<2; ++pass)
    {
        const Bldg* layer = (pass==0) ? bldgsFar : bldgsNear;
        int cnt = (pass==0) ? (int)(sizeof(bldgsFar)/sizeof(bldgsFar[0]))
                            : (int)(sizeof(bldgsNear)/sizeof(bldgsNear[0]));
        float haze = (pass==0) ? 0.55f : 0.0f; // far row blends toward the sky for atmospheric depth

        float skyR = isNight ? 0.012f : 0.55f;
        float skyG = isNight ? 0.022f : 0.74f;
        float skyB = isNight ? 0.070f : 0.92f;

        for (int i=0;i<cnt;++i)
        {
            const Bldg& b = layer[i];
            bool alt = (i%2==0);

            float baseR,baseG,baseB, topR,topG,topB;
            if (isNight)
            {
                baseR=0.015f; baseG=0.015f; baseB=0.035f;
                topR =0.030f; topG =0.030f; topB =0.060f;
            }
            else
            {
                // Cool gray-blue city tones suit the clear daytime sky.
                baseR = alt?0.30f:0.24f; baseG = alt?0.34f:0.28f; baseB = alt?0.42f:0.36f;
                topR  = baseR+0.12f;     topG  = baseG+0.12f;     topB  = baseB+0.12f;
            }
            baseR = baseR*(1.0f-haze)+skyR*haze; baseG = baseG*(1.0f-haze)+skyG*haze; baseB = baseB*(1.0f-haze)+skyB*haze;
            topR  = topR *(1.0f-haze)+skyR*haze; topG  = topG *(1.0f-haze)+skyG*haze; topB  = topB *(1.0f-haze)+skyB*haze;

            // Lower body and a slightly lighter top third catching the sunset.
            glColor3f(baseR,baseG,baseB);
            glPushMatrix(); glTranslatef(b.x,b.h*0.33f-0.05f,b.z); drawBox(b.w,b.h*0.66f,b.depth); glPopMatrix();
            glColor3f(topR,topG,topB);
            glPushMatrix(); glTranslatef(b.x,b.h*0.83f-0.05f,b.z); drawBox(b.w,b.h*0.34f,b.depth); glPopMatrix();

            // Rooftop details for a bit of skyline variety.
            if (b.antenna)
            {
                glColor3f(baseR*0.7f,baseG*0.7f,baseB*0.7f);
                glPushMatrix(); glTranslatef(b.x,b.h+0.35f-0.05f,b.z); drawBox(0.04f,0.70f,0.04f); glPopMatrix();
            }
            if (b.tank)
            {
                glColor3f(baseR*0.8f,baseG*0.8f,baseB*0.8f);
                glPushMatrix(); glTranslatef(b.x-b.w*0.2f,b.h+0.14f-0.05f,b.z); drawBox(0.30f,0.28f,0.30f); glPopMatrix();
            }

            // Lit windows, denser and closer-spaced on the near row.
            if (isNight) glColor3f(1.0f,0.85f,0.45f);
            else         glColor3f(0.95f*(1.0f-haze)+skyR*haze, 0.55f*(1.0f-haze)+skyG*haze, 0.28f*(1.0f-haze)+skyB*haze);

            int rows = (pass==0) ? 3 : 4;
            for (int wcol=-1; wcol<=1; ++wcol)
            for (int wrow=0; wrow<rows; ++wrow)
            {
                if ((wcol+wrow+i)%3!=0) continue;
                glPushMatrix();
                glTranslatef(b.x+wcol*b.w*0.28f,
                            0.8f+wrow*(b.h/(float)(rows+1)),
                            b.z + b.depth*0.5f + 0.01f);
                drawBox(0.13f,0.18f,0.02f);
                glPopMatrix();
            }
        }
    }

    if (lightingWasOn) glEnable(GL_LIGHTING);
}

// Extra night-sky detail: a moon and fixed stars make the outdoor view
// feel intentional rather than just a dark background.
void drawNightSkyDetails()
{
    if (!isNight) return;

    GLboolean lightingWasOn = glIsEnabled(GL_LIGHTING);
    if (lightingWasOn) glDisable(GL_LIGHTING);

    // Moon
    glColor3f(0.95f,0.95f,0.78f);
    glPushMatrix();
    glTranslatef(-8.0f,9.0f,-25.2f);
    glScalef(1.0f,1.0f,0.25f);
    glutSolidSphere(0.72f,24,18);
    glPopMatrix();

    // Stars
    static const float stars[][2] = {
        {-13.0f,10.8f},{-10.5f,8.8f},{-7.0f,11.5f},{-4.5f,9.6f},
        {-1.5f,10.9f},{ 1.6f,8.9f},{ 4.2f,11.7f},{ 6.8f,9.7f},
        { 9.2f,11.1f},{12.0f,8.7f},{-12.0f,6.9f},{-5.8f,7.6f},
        { 0.2f,7.4f},{ 5.3f,6.8f},{10.6f,7.5f}
    };

    glColor3f(0.95f,0.97f,1.0f);
    for (int i=0; i<(int)(sizeof(stars)/sizeof(stars[0])); ++i)
    {
        float r = (i%3==0) ? 0.055f : 0.035f;
        glPushMatrix();
        glTranslatef(stars[i][0],stars[i][1],-25.0f);
        glutSolidSphere(r,8,6);
        glPopMatrix();
    }

    if (lightingWasOn) glEnable(GL_LIGHTING);
}

void drawOutsideEnvironment()
{
    // Large outside ground so the camera can actually leave the room.
    if (isNight)
        setMaterial(0.01f,0.025f,0.01f, 0.035f,0.075f,0.04f, 0.03f,0.03f,0.03f, 4.0f);
    else
        setMaterial(0.08f,0.06f,0.04f, 0.24f,0.16f,0.12f, 0.08f,0.06f,0.05f, 8.0f);

    glPushMatrix();
    glTranslatef(0.0f,-0.08f,-18.0f);
    drawBox(28.0f,0.15f,16.0f);
    glPopMatrix();

    // Sky: drawn without lighting, layered so daytime reads as a sunset
    // (deep violet up high, warm orange near the horizon).
    GLboolean lightingWasOn = glIsEnabled(GL_LIGHTING);
    if (lightingWasOn) glDisable(GL_LIGHTING);
    if (isNight)
    {
        glColor3f(0.012f,0.022f,0.070f);
        glPushMatrix();
        glTranslatef(0.0f,7.0f,-26.0f);
        drawBox(34.0f,14.0f,0.20f);
        glPopMatrix();
    }
    else
    {
        // Clear daytime gradient: deep blue above, lighter blue near horizon.
        glColor3f(0.18f,0.45f,0.78f);
        glPushMatrix(); glTranslatef(0.0f,10.5f,-26.0f); drawBox(34.0f,7.0f,0.20f); glPopMatrix();

        glColor3f(0.36f,0.64f,0.88f);
        glPushMatrix(); glTranslatef(0.0f,5.7f,-26.0f); drawBox(34.0f,3.0f,0.20f); glPopMatrix();

        glColor3f(0.72f,0.84f,0.95f);
        glPushMatrix(); glTranslatef(0.0f,2.9f,-26.0f); drawBox(34.0f,2.8f,0.20f); glPopMatrix();
    }
    if (lightingWasOn) glEnable(GL_LIGHTING);

    drawNightSkyDetails();
    drawCitySkyline();

    // Road across the outdoor area.
    setMaterial(0.025f,0.025f,0.028f, 0.12f,0.12f,0.14f, 0.08f,0.08f,0.08f, 10.0f);
    glPushMatrix();
    glTranslatef(0.0f,0.02f,-15.2f);
    drawBox(28.0f,0.07f,3.0f);
    glPopMatrix();

    // Road centre markings.
    setMaterial(0.18f,0.14f,0.02f, 0.92f,0.72f,0.10f, 0.15f,0.12f,0.03f, 10.0f);
    for (int i=-6; i<=6; ++i)
    {
        glPushMatrix();
        glTranslatef(i*2.0f,0.075f,-15.2f);
        drawBox(1.0f,0.02f,0.08f);
        glPopMatrix();
    }

    // Sidewalk / path leading from the door toward the road.
    setMaterial(0.12f,0.12f,0.12f, 0.42f,0.42f,0.44f, 0.10f,0.10f,0.10f, 12.0f);
    glPushMatrix();
    glTranslatef(-7.10f,0.01f,-12.0f);
    drawBox(3.3f,0.06f,4.0f);
    glPopMatrix();

    // Low sunset sun near the horizon, with a soft warm halo. Only shown
    // in daytime mode.
    if (!isNight)
    {
        GLboolean oldLight = glIsEnabled(GL_LIGHTING);
        if (oldLight) glDisable(GL_LIGHTING);
        glColor3f(1.0f,0.88f,0.48f);
        glPushMatrix();
        glTranslatef(-8.0f,9.0f,-24.6f);
        glutSolidSphere(1.00f,10,8);
        glPopMatrix();
        glColor3f(1.0f,0.95f,0.68f);
        glPushMatrix();
        glTranslatef(-8.0f,9.0f,-24.5f);
        glutSolidSphere(0.58f,28,20);
        glPopMatrix();
        if (oldLight) glEnable(GL_LIGHTING);
    }

    // Trees around the road and window view.
    drawTree(-11.0f,-18.5f,1.15f);
    drawTree(-3.0f,-20.5f,0.95f);
    drawTree( 4.8f,-18.8f,1.05f);
    drawTree( 8.0f,-20.0f,0.90f);
    drawTree(11.0f,-17.4f,0.80f);

    // Road lamps are visible all the time; at night they are the outdoor light sources.
    drawRoadLamp(-4.5f,-14.0f);
    drawRoadLamp( 5.0f,-14.0f);

    // Continuously moving cars along the road.
    drawCar(car1X,-15.6f,   0.0f, 0.60f,0.08f,0.08f);
    drawCar(car2X,-14.8f, 180.0f, 0.10f,0.20f,0.55f);
    drawCar(car3X,-15.6f,   0.0f, 0.85f,0.82f,0.78f);
}


// ================================================================
// HOUSE EXTERIOR - gives the building an actual house-like silhouette
// (gabled roof, eaves, chimney, painted facade, porch) when seen from
// outside instead of a bare flat wall floating in the scene.
// ================================================================
void drawHouseExterior()
{
    const float wallTop  = 8.0f;
    const float roofRise = 3.3f;
    const float halfW    = 10.5f;
    const float backZ    = -10.0f;
    const float frontZ   = 10.6f;
    const float roofLen  = frontZ - backZ;
    const float midZ     = (backZ+frontZ)*0.5f;

    const float slopeLen   = std::sqrt(halfW*halfW + roofRise*roofRise);
    const float slopeAngle = std::atan2(roofRise,halfW) * 180.0f / PI;

    // -------- Brick-textured exterior walls (back/outside face) --------
    // Blue brick exterior.  The fairly bright diffuse values stop the
    // texture from becoming almost black under the daytime/night lighting.
    setMaterial(
        0.18f, 0.25f, 0.36f,    // ambient
        0.72f, 0.84f, 1.00f,    // diffuse
        0.18f, 0.24f, 0.32f,    // specular
        22.0f                    // shininess
    );
    glPushMatrix(); glTranslatef(-9.325f,4.0f,backZ-0.20f); drawTexturedBox(1.35f,8.0f,0.18f,brickTex,1,3); glPopMatrix();
    glPushMatrix(); glTranslatef(-0.675f,4.0f,backZ-0.20f); drawTexturedBox(9.75f,8.0f,0.18f,brickTex,4,3); glPopMatrix();
    glPushMatrix(); glTranslatef( 9.20f,4.0f,backZ-0.20f); drawTexturedBox(1.60f,8.0f,0.18f,brickTex,1,3); glPopMatrix();
    glPushMatrix(); glTranslatef(-7.10f,6.05f,backZ-0.20f); drawTexturedBox(3.10f,3.90f,0.18f,brickTex,2,2); glPopMatrix();
    // Floor-to-ceiling window: only wall above it remains.
    glPushMatrix(); glTranslatef(6.30f,7.30f,backZ-0.20f); drawTexturedBox(4.20f,1.40f,0.18f,brickTex,2,1); glPopMatrix();

    // Brick gable fill under the roof peak.
    glPushMatrix(); glTranslatef(0,wallTop+roofRise*0.5f,backZ-0.22f); drawTexturedBox(halfW*1.55f,roofRise,0.18f,brickTex,5,2); glPopMatrix();

    // Slim wooden trims so the facade still feels finished.
    matWood();
    glPushMatrix(); glTranslatef(0,0.14f,backZ-0.31f); drawBox(halfW*2.0f,0.28f,0.08f); glPopMatrix();
    glPushMatrix(); glTranslatef(0,wallTop-0.10f,backZ-0.31f); drawBox(halfW*2.0f,0.22f,0.08f); glPopMatrix();
    glPushMatrix(); glTranslatef(-halfW+0.15f,wallTop*0.5f,backZ-0.31f); drawBox(0.22f,wallTop,0.08f); glPopMatrix();
    glPushMatrix(); glTranslatef( halfW-0.15f,wallTop*0.5f,backZ-0.31f); drawBox(0.22f,wallTop,0.08f); glPopMatrix();

    // Roof panels remain dark, so the brick house has contrast.
    setMaterial(0.03f,0.03f,0.03f, 0.14f,0.14f,0.15f, 0.14f,0.14f,0.15f, 20.0f);
    glPushMatrix();
    glTranslatef(-halfW*0.5f, wallTop+roofRise*0.5f, midZ);
    glRotatef(slopeAngle,0,0,1);
    drawBox(slopeLen,0.18f,roofLen);
    glPopMatrix();

    glPushMatrix();
    glTranslatef( halfW*0.5f, wallTop+roofRise*0.5f, midZ);
    glRotatef(-slopeAngle,0,0,1);
    drawBox(slopeLen,0.18f,roofLen);
    glPopMatrix();

    setMaterial(0.01f,0.01f,0.01f, 0.06f,0.06f,0.06f, 0.10f,0.10f,0.10f, 14.0f);
    glPushMatrix(); glTranslatef(0,wallTop+roofRise+0.06f,midZ); drawBox(0.32f,0.20f,roofLen); glPopMatrix();

    // Chimney
    setMaterial(0.18f,0.15f,0.14f, 0.56f,0.50f,0.46f, 0.08f,0.08f,0.08f, 14.0f);
    glPushMatrix(); glTranslatef(6.0f,wallTop+roofRise*0.9f,-4.0f); drawTexturedBox(0.60f,2.4f,0.60f,brickTex,1,2); glPopMatrix();
    setMaterial(0.02f,0.02f,0.02f, 0.06f,0.06f,0.06f, 0.05f,0.05f,0.05f, 6.0f);
    glPushMatrix(); glTranslatef(6.0f,wallTop+roofRise*0.9f+1.25f,-4.0f); drawBox(0.70f,0.10f,0.70f); glPopMatrix();

    // Small porch over the door.
    setMaterial(0.03f,0.03f,0.03f, 0.14f,0.14f,0.15f, 0.14f,0.14f,0.15f, 20.0f);
    glPushMatrix(); glTranslatef(-7.10f,4.55f,backZ-1.35f); drawBox(3.7f,0.16f,1.7f); glPopMatrix();
    matWood();
    glPushMatrix(); glTranslatef(-8.55f,2.25f,backZ-1.95f); drawBox(0.14f,4.5f,0.14f); glPopMatrix();
    glPushMatrix(); glTranslatef(-5.65f,2.25f,backZ-1.95f); drawBox(0.14f,4.5f,0.14f); glPopMatrix();

    // Two shallow entrance steps make the doorway connect more naturally
    // to the outdoor path.
    setMaterial(0.18f,0.19f,0.21f, 0.58f,0.60f,0.64f, 0.12f,0.12f,0.12f, 18.0f);
    glPushMatrix(); glTranslatef(-7.10f,0.08f,backZ-0.65f); drawBox(3.50f,0.16f,0.65f); glPopMatrix();
    glPushMatrix(); glTranslatef(-7.10f,0.04f,backZ-1.05f); drawBox(3.85f,0.08f,0.55f); glPopMatrix();

    // Small side planter beside the door instead of under the full-height window.
    setMaterial(0.18f,0.18f,0.18f, 0.42f,0.42f,0.44f, 0.10f,0.10f,0.10f, 10.0f);
    glPushMatrix(); glTranslatef(2.60f,0.55f,backZ-0.40f); drawBox(2.40f,0.35f,0.42f); glPopMatrix();
    matGreen();
    for (int i=-2;i<=2;++i)
    {
        glPushMatrix(); glTranslatef(2.60f+i*0.42f,0.85f,backZ-0.40f); glutSolidSphere(0.17f,10,8); glPopMatrix();
    }
}


// ================================================================
// TOURNAMENT DECOR + OUTDOOR SIGNBOARD + MARKET STALL
// Colourful string lights, a golden trophy roof emblem, bunting flags,
// hanging lanterns, a roadside signboard and a market stall, all mounted
// on the house exterior so the building reads as festive and colourful
// from outside and clearly announces the chess tournament (not Eid).
// ================================================================

// A festive strand of small coloured bulbs strung in a gentle sag between
// two points - unlit so the colours stay bright regardless of room light.
void drawStringLights(float x1, float y1, float z1, float x2, float y2, float z2, int count)
{
    static const float bulbColors[6][3] = {
        {1.0f,0.15f,0.15f}, {1.0f,0.75f,0.05f}, {0.15f,0.85f,0.25f},
        {0.15f,0.55f,1.0f}, {0.95f,0.15f,0.75f}, {1.0f,1.0f,1.0f}
    };
    GLboolean lightingWasOn = glIsEnabled(GL_LIGHTING);
    if (lightingWasOn) glDisable(GL_LIGHTING);

    glColor3f(0.05f,0.05f,0.05f);
    for (int i=0;i<=count;++i)
    {
        float t = (float)i/count;
        float sag = 0.16f*std::sin(t*PI);
        float bx = x1+(x2-x1)*t;
        float by = y1+(y2-y1)*t - sag;
        float bz = z1+(z2-z1)*t;
        const float* c = bulbColors[i % 6];
        glColor3f(c[0],c[1],c[2]);
        glPushMatrix(); glTranslatef(bx,by,bz); glutSolidSphere(0.06f,10,8); glPopMatrix();
    }

    if (lightingWasOn) glEnable(GL_LIGHTING);
}

// A crescent moon with a small star beside it, built from unlit primitives:
// a bright flattened sphere for the moon, a sky-coloured sphere offset to
// bite a crescent shape out of it, and a handful of dots for the star.
void drawCrescentStar(float x, float y, float z)
{
    GLboolean lightingWasOn = glIsEnabled(GL_LIGHTING);
    if (lightingWasOn) glDisable(GL_LIGHTING);

    glColor3f(1.0f,0.85f,0.25f);
    glPushMatrix(); glTranslatef(x,y,z); glScalef(1.0f,1.0f,0.30f); glutSolidSphere(0.42f,20,16); glPopMatrix();

    float skyR = isNight?0.012f:0.62f, skyG = isNight?0.022f:0.22f, skyB = isNight?0.070f:0.28f;
    glColor3f(skyR,skyG,skyB);
    glPushMatrix(); glTranslatef(x+0.18f,y+0.06f,z-0.05f); glScalef(1.0f,1.0f,0.30f); glutSolidSphere(0.38f,20,16); glPopMatrix();

    glColor3f(1.0f,0.85f,0.25f);
    static const float sx[5] = {0.0f, 0.09f, 0.055f, -0.055f, -0.09f};
    static const float sy[5] = {0.11f, 0.03f, -0.09f, -0.09f, 0.03f};
    for (int i=0;i<5;++i)
    {
        glPushMatrix(); glTranslatef(x+0.62f+sx[i],y+sy[i],z); glutSolidSphere(0.035f,8,6); glPopMatrix();
    }

    if (lightingWasOn) glEnable(GL_LIGHTING);
}

// A large golden trophy emblem mounted at the roof peak, with a few
// twinkling star accents around it - the tournament's own "crest" instead
// of the crescent-moon-and-star (which read as an Eid ornament, not a
// chess-tournament one).
void drawRoofEmblem(float x, float y, float z)
{
    matGold();
    glPushMatrix();
    glTranslatef(x,y,z);
    glScalef(2.4f,2.4f,2.4f);
    drawTrophy(0,0,0);
    glPopMatrix();

    GLboolean lw = glIsEnabled(GL_LIGHTING);
    if (lw) glDisable(GL_LIGHTING);
    glColor3f(1.0f,0.88f,0.30f);
    static const float sx[6] = {1.0f,-1.0f,0.65f,-0.65f,0.0f,0.0f};
    static const float sy[6] = {0.35f,0.35f,-0.45f,-0.45f,0.75f,-0.85f};
    for (int i=0;i<6;++i)
    {
        glPushMatrix(); glTranslatef(x+sx[i],y+sy[i],z); glutSolidSphere(0.05f,8,6); glPopMatrix();
    }
    if (lw) glEnable(GL_LIGHTING);
}

// A string of small triangular flags between two points, Eid-green/gold
// palette, hung along a thin wire.
void drawBunting(float x1, float y1, float z1, float x2, float y2, float z2, int count)
{
    static const float flagColors[4][3] = {
        {0.03f,0.35f,0.10f}, {0.75f,0.55f,0.05f}, {0.85f,0.85f,0.85f}, {0.55f,0.03f,0.03f}
    };
    GLboolean lightingWasOn = glIsEnabled(GL_LIGHTING);
    if (lightingWasOn) glDisable(GL_LIGHTING);

    glColor3f(0.85f,0.85f,0.85f);
    glPushMatrix();
    glTranslatef((x1+x2)*0.5f,(y1+y2)*0.5f+0.04f,(z1+z2)*0.5f);
    drawBox(std::fabs(x2-x1)+0.05f,0.02f,0.02f);
    glPopMatrix();

    for (int i=0;i<count;++i)
    {
        float t = (i+0.5f)/count;
        float fx = x1+(x2-x1)*t;
        float fy = y1+(y2-y1)*t - 0.10f;
        float fz = z1+(z2-z1)*t;
        const float* c = flagColors[i%4];
        glColor3f(c[0],c[1],c[2]);
        glPushMatrix();
        glTranslatef(fx,fy,fz);
        glBegin(GL_TRIANGLES);
            glVertex3f(-0.09f, 0.10f, 0.0f);
            glVertex3f( 0.09f, 0.10f, 0.0f);
            glVertex3f( 0.0f, -0.07f, 0.0f);
        glEnd();
        glPopMatrix();
    }

    if (lightingWasOn) glEnable(GL_LIGHTING);
}

// A small hanging paper lantern with a warm glowing bulb.
void drawHangingLantern(float x, float y, float z)
{
    matGold();
    glPushMatrix(); glTranslatef(x,y+0.30f,z); drawBox(0.025f,0.28f,0.025f); glPopMatrix();
    glPushMatrix(); glTranslatef(x,y-0.19f,z); drawCylinderY(0.03f,0.07f,10); glPopMatrix();

    GLboolean lightingWasOn = glIsEnabled(GL_LIGHTING);
    if (lightingWasOn) glDisable(GL_LIGHTING);
    glColor3f(1.0f,0.55f,0.15f);
    glPushMatrix(); glTranslatef(x,y,z); glutSolidSphere(0.16f,14,10); glPopMatrix();
    if (lightingWasOn) glEnable(GL_LIGHTING);
}

// A roadside wooden signboard on two posts, reusing the same plaque text
// logic as the indoor posters, telling passers-by what's happening inside.
void drawSignboard(float x, float z, float rotY, const char* line1, const char* line2)
{
    matWood();
    glPushMatrix(); glTranslatef(x-3.35f,0.95f,z); glRotatef(rotY,0,1,0); drawBox(0.18f,1.9f,0.18f); glPopMatrix();
    glPushMatrix(); glTranslatef(x+3.35f,0.95f,z); glRotatef(rotY,0,1,0); drawBox(0.18f,1.9f,0.18f); glPopMatrix();

    drawPoster(x,2.55f,z,rotY,6.2f,3.0f,line1,line2);
}

// A small, colourful roadside market/refreshment stall - striped canopy,
// a wooden counter, and a row of brightly coloured jars/goods - placed
// beside the tournament signboard so passers-by see life around the venue.
void drawMarketStall(float x, float z, float rotY)
{
    glPushMatrix();
    glTranslatef(x,0,z);
    glRotatef(rotY,0,1,0);

    // corner posts
    matWood();
    glPushMatrix(); glTranslatef(-1.1f,1.1f,-0.5f); drawBox(0.12f,2.2f,0.12f); glPopMatrix();
    glPushMatrix(); glTranslatef( 1.1f,1.1f,-0.5f); drawBox(0.12f,2.2f,0.12f); glPopMatrix();
    glPushMatrix(); glTranslatef(-1.1f,1.1f, 0.5f); drawBox(0.12f,2.2f,0.12f); glPopMatrix();
    glPushMatrix(); glTranslatef( 1.1f,1.1f, 0.5f); drawBox(0.12f,2.2f,0.12f); glPopMatrix();

    // striped awning roof, bright alternating colours
    static const float stripeColors[4][3] = {
        {0.88f,0.15f,0.15f}, {0.96f,0.80f,0.10f}, {0.15f,0.55f,0.88f}, {0.95f,0.95f,0.95f}
    };
    const int stripes = 8;
    const float canopyW = 2.6f, canopyD = 1.3f;
    for (int i=0;i<stripes;++i)
    {
        const float* c = stripeColors[i%4];
        setMaterial(c[0]*0.3f,c[1]*0.3f,c[2]*0.3f, c[0],c[1],c[2], 0.12f,0.12f,0.12f, 15.0f);
        float sx0 = -canopyW*0.5f + canopyW*((float)i+0.5f)/stripes;
        glPushMatrix(); glTranslatef(sx0,2.35f,0.0f); drawBox(canopyW/stripes+0.02f,0.10f,canopyD); glPopMatrix();
    }
    // triangular valance hanging off the front edge of the awning
    for (int i=0;i<stripes;++i)
    {
        const float* c = stripeColors[i%4];
        setMaterial(c[0]*0.3f,c[1]*0.3f,c[2]*0.3f, c[0],c[1],c[2], 0.10f,0.10f,0.10f, 12.0f);
        float sx0 = -canopyW*0.5f + canopyW*((float)i+0.5f)/stripes;
        glPushMatrix();
        glTranslatef(sx0,2.20f,canopyD*0.5f);
        glBegin(GL_TRIANGLES);
            glVertex3f(-canopyW/(2*stripes),0.10f,0.0f);
            glVertex3f( canopyW/(2*stripes),0.10f,0.0f);
            glVertex3f(0.0f,-0.14f,0.0f);
        glEnd();
        glPopMatrix();
    }

    // wooden counter
    matWood();
    glPushMatrix(); glTranslatef(0,0.95f,0.35f); drawBox(2.4f,0.10f,0.55f); glPopMatrix();
    glPushMatrix(); glTranslatef(-1.05f,0.48f,0.35f); drawBox(0.10f,0.95f,0.10f); glPopMatrix();
    glPushMatrix(); glTranslatef( 1.05f,0.48f,0.35f); drawBox(0.10f,0.95f,0.10f); glPopMatrix();

    // a row of colourful jars/goods on the counter
    static const float itemColors[5][3] = {
        {0.85f,0.15f,0.15f}, {0.15f,0.65f,0.25f}, {0.90f,0.70f,0.10f}, {0.20f,0.40f,0.85f}, {0.75f,0.20f,0.65f}
    };
    for (int i=0;i<5;++i)
    {
        const float* c = itemColors[i];
        setMaterial(c[0]*0.3f,c[1]*0.3f,c[2]*0.3f, c[0],c[1],c[2], 0.30f,0.30f,0.30f, 40.0f);
        glPushMatrix(); glTranslatef(-0.9f+i*0.45f,1.12f,0.30f); drawCylinderY(0.10f,0.22f,12); glPopMatrix();
    }

    // small pennants hanging under the canopy edge
    drawBunting(-canopyW*0.5f,2.05f,canopyD*0.5f+0.02f, canopyW*0.5f,2.05f,canopyD*0.5f+0.02f, 6);

    glPopMatrix();
}

void drawTournamentDecor()
{
    const float wallTop  = 8.0f;
    const float roofRise = 3.3f;
    const float halfW    = 10.5f;
    const float backZ    = -10.0f;
    const float frontZ   = 10.6f;

    // Golden trophy emblem mounted at the roof peak (the tournament's own
    // crest, replacing the earlier Eid crescent-moon-and-star ornament).
    drawRoofEmblem(0.0f, wallTop+roofRise+0.85f, backZ-0.15f);

    // Bright rainbow-striped banner band across the gable.
    static const float bannerColors[6][3] = {
        {0.85f,0.12f,0.12f}, {0.92f,0.55f,0.08f}, {0.90f,0.82f,0.10f},
        {0.12f,0.65f,0.25f}, {0.12f,0.40f,0.85f}, {0.55f,0.15f,0.70f}
    };
    const int bandStripes = 12;
    for (int i=0;i<bandStripes;++i)
    {
        const float* c = bannerColors[i%6];
        setMaterial(c[0]*0.3f,c[1]*0.3f,c[2]*0.3f, c[0],c[1],c[2], 0.20f,0.20f,0.20f, 25.0f);
        float sx0 = -halfW*1.5f*0.5f + halfW*1.5f*((float)i+0.5f)/bandStripes;
        glPushMatrix(); glTranslatef(sx0,wallTop+0.35f,backZ-0.155f); drawBox(halfW*1.5f/bandStripes+0.02f,0.22f,0.02f); glPopMatrix();
    }
    setMaterial(0.20f,0.15f,0.02f, 0.62f,0.46f,0.08f, 0.20f,0.16f,0.05f, 30.0f);
    glPushMatrix(); glTranslatef(0,wallTop+0.12f,backZ-0.155f); drawBox(halfW*1.5f,0.10f,0.02f); glPopMatrix();

    // Tournament name across the gable, facing outward.
    GLboolean lw = glIsEnabled(GL_LIGHTING);
    if (lw) glDisable(GL_LIGHTING);
    glColor3f(1.0f,0.85f,0.25f);
    if (lw) glEnable(GL_LIGHTING);
    {
        const char* bannerText = "GRANDMASTER ARENA";
        const float bannerScale = 0.0040f;
        // rotY=180 -> local +x (the direction the stroke font advances)
        // maps to world -x, so starting at +halfWidth centers it on x=0.
        float halfWidth = (float)glutStrokeLength(GLUT_STROKE_ROMAN, (const unsigned char*)bannerText) * bannerScale * 0.5f;
        draw3DText(halfWidth, wallTop+1.85f, backZ-0.16f, bannerScale, 180.0f, bannerText);
    }

    // Colourful string lights along both roof eaves, front to back.
    drawStringLights(-halfW, wallTop+0.05f, backZ,  -halfW, wallTop+0.05f, frontZ, 12);
    drawStringLights( halfW, wallTop+0.05f, backZ,   halfW, wallTop+0.05f, frontZ, 12);

    // Bunting flags strung between the porch posts.
    drawBunting(-8.55f,4.30f,backZ-1.95f, -5.65f,4.30f,backZ-1.95f, 6);

    // Lanterns flanking the front door.
    drawHangingLantern(-9.05f,3.55f,backZ-0.35f);
    drawHangingLantern(-5.15f,3.55f,backZ-0.35f);

    // Roadside signboard announcing the tournament, facing the road/camera.
    drawSignboard(-4.0f, -13.0f, 180.0f, "CHESS", "TOURNAMENT");

    // A colourful market/refreshment stall set up beside the signboard.
    drawMarketStall(2.0f, -13.0f, 180.0f);
}


void drawWindowFrame()
{
    matWood();
    // Floor-to-ceiling window opening: x 4.2..8.4, y 0..6.6 on back wall z=-10
    float z = -9.83f;
    glPushMatrix(); glTranslatef(4.25f,3.30f,z); drawBox(0.16f,6.60f,0.18f); glPopMatrix();
    glPushMatrix(); glTranslatef(8.35f,3.30f,z); drawBox(0.16f,6.60f,0.18f); glPopMatrix();
    glPushMatrix(); glTranslatef(6.30f,6.55f,z); drawBox(4.25f,0.16f,0.18f); glPopMatrix();
    glPushMatrix(); glTranslatef(6.30f,3.30f,z); drawBox(0.12f,6.40f,0.15f); glPopMatrix();
    // Slim floor rail for realism without blocking the glass.
    glPushMatrix(); glTranslatef(6.30f,0.08f,z); drawBox(4.25f,0.12f,0.18f); glPopMatrix();
}



// Transparent glass pane inside the floor-to-ceiling window.
// The outside remains visible, but the cyan tint/reflection makes it
// unmistakably read as a real window rather than an empty hole.
void drawWindowGlass()
{
    GLboolean lightingWasOn = glIsEnabled(GL_LIGHTING);
    if (lightingWasOn) glDisable(GL_LIGHTING);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);

    glColor4f(0.42f,0.72f,0.95f,0.13f);
    glPushMatrix();
    glTranslatef(6.30f,3.30f,-9.90f);
    drawBox(3.95f,6.28f,0.025f);
    glPopMatrix();

    // Thin highlight strips on the pane.
    glColor4f(0.90f,0.97f,1.0f,0.18f);
    glPushMatrix(); glTranslatef(5.15f,3.80f,-9.88f); drawBox(0.035f,4.5f,0.012f); glPopMatrix();
    glPushMatrix(); glTranslatef(7.35f,2.60f,-9.88f); drawBox(0.025f,3.2f,0.012f); glPopMatrix();

    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);

    if (lightingWasOn) glEnable(GL_LIGHTING);
}

void drawDoor()
{
    const float hingeX = -8.65f;
    const float doorW  = 3.10f;
    const float doorH  = 4.10f;
    const float z      = -9.82f;

    // Static door frame around the actual opening.
    matWood();
    glPushMatrix(); glTranslatef(hingeX-0.10f,doorH*0.5f,z); drawBox(0.20f,doorH+0.25f,0.28f); glPopMatrix();
    glPushMatrix(); glTranslatef(hingeX+doorW+0.10f,doorH*0.5f,z); drawBox(0.20f,doorH+0.25f,0.28f); glPopMatrix();
    glPushMatrix(); glTranslatef(hingeX+doorW*0.5f,doorH+0.10f,z); drawBox(doorW+0.35f,0.20f,0.28f); glPopMatrix();

    // Single hinged door. Rotation is around its left edge.
    glPushMatrix();
    glTranslatef(hingeX,0.0f,z);
    glRotatef(doorAngle,0,1,0);

    matWood();
    glPushMatrix();
    glTranslatef(doorW*0.5f,doorH*0.5f,0.0f);
    drawTexturedBox(doorW,doorH,0.18f,woodTex,2.0f,2.4f);
    glPopMatrix();

    // Decorative strips.
    matMetal();
    glPushMatrix(); glTranslatef(doorW*0.50f,doorH*0.52f,0.11f); drawBox(0.05f,doorH*0.82f,0.04f); glPopMatrix();

    // Handle follows the door during rotation.
    matGold();
    glPushMatrix();
    glTranslatef(doorW-0.34f,2.0f,0.16f);
    glutSolidSphere(0.10f,16,12);
    glPopMatrix();

    glPopMatrix();
}


// ================================================================
// ROOM DECOR (wall posters, framed picture, lamps, rug, plants, trophy)
// ================================================================

// Draws stroke-font text flat against a wall. rotY selects which wall the
// text faces: 0 = back wall (+Z facing), 90 = left wall (+X facing),
// -90 = right wall (-X facing). Text grows away from (x,y,z) in the
// direction that reads correctly for someone standing in the room.
void draw3DText(float x, float y, float z, float scale, float rotY, const char* text)
{
    GLboolean lightingWasOn = glIsEnabled(GL_LIGHTING);
    if (lightingWasOn) glDisable(GL_LIGHTING);
    glPushMatrix();
    glTranslatef(x,y,z);
    glRotatef(rotY,0,1,0);
    glScalef(scale,scale,scale);
    for (const char* p = text; *p; ++p)
        glutStrokeCharacter(GLUT_STROKE_ROMAN, *p);
    glPopMatrix();
    if (lightingWasOn) glEnable(GL_LIGHTING);
}

// Places one line of stroke-font text so it is properly centered on a
// panel at (x,y,z) rotated by rotY, regardless of which way rotY points.
// glutStrokeLength() gives the text's real width (previous code only
// guessed an offset from the panel width, which happened to work for
// rotY=90/-90 but pushed text for rotY=0/180 off past the panel edge -
// that's what was clipping "CHESS"/"TOURNAMENT" behind the signboard post).
void drawPosterTextLine(float x, float y, float z, float rotY, float textScale, const char* text)
{
    float thetaRad = rotY * PI / 180.0f;
    // Local +x (the direction glutStrokeCharacter advances in) maps to
    // this world direction once rotated by rotY:
    float dirX = std::cos(thetaRad);
    float dirZ = -std::sin(thetaRad);
    // Local +z (out of the panel face) maps to this world direction -
    // used only for a tiny nudge so the text doesn't z-fight the panel:
    float depthX = std::sin(thetaRad);
    float depthZ = std::cos(thetaRad);
    const float depthNudge = 0.10f;

    float w = (float)glutStrokeLength(GLUT_STROKE_ROMAN, (const unsigned char*)text) * textScale;
    float startX = x - dirX*(w*0.5f) + depthX*depthNudge;
    float startZ = z - dirZ*(w*0.5f) + depthZ*depthNudge;
    draw3DText(startX, y, startZ, textScale, rotY, text);
}

float getFittedPosterTextScale(float panelW, const char* text)
{
    const float baseScale = 0.0055f;
    const float maxWidth = panelW * 0.76f;
    float rawWidth = (float)glutStrokeLength(GLUT_STROKE_ROMAN, (const unsigned char*)text);
    if (rawWidth <= 1.0f) return baseScale;
    float fitted = maxWidth / rawWidth;
    if (fitted > baseScale) fitted = baseScale;
    return fitted;
}

// A black plaque with two lines of cream text, mounted flush on a wall.
// rotY: 90 for the left wall, -90 for the right wall, 0 for the back wall
// (also used at 180 for the freestanding outdoor signboard).
void drawPoster(float x, float y, float z, float rotY,
                float panelW, float panelH,
                const char* line1, const char* line2)
{
    glPushMatrix();
    glTranslatef(x,y,z);
    glRotatef(rotY,0,1,0);

    setMaterial(0.01f,0.01f,0.01f, 0.03f,0.03f,0.03f, 0.10f,0.10f,0.10f, 10.0f);
    glPushMatrix(); glTranslatef(0,0,0.06f); drawBox(panelW,panelH,0.06f); glPopMatrix();

    matWood();
    glPushMatrix(); glTranslatef(0,0,0.02f); drawBox(panelW+0.14f,panelH+0.14f,0.05f); glPopMatrix();
    glPopMatrix();

    GLboolean lightingWasOn = glIsEnabled(GL_LIGHTING);
    if (lightingWasOn) glDisable(GL_LIGHTING);
    glColor3f(0.95f,0.88f,0.62f);
    if (lightingWasOn) glEnable(GL_LIGHTING);

    float scale1 = getFittedPosterTextScale(panelW, line1);
    drawPosterTextLine(x, y+panelH*0.18f, z, rotY, scale1, line1);

    if (line2)
    {
        float scale2 = getFittedPosterTextScale(panelW, line2);
        drawPosterTextLine(x, y-panelH*0.18f, z, rotY, scale2, line2);
    }
}

// Small decorative framed chessboard picture, mounted on the back wall.
void drawFramedChessPicture(float cx, float cy, float z)
{
    matWood();
    glPushMatrix(); glTranslatef(cx,cy,z+0.02f); drawBox(2.6f,3.0f,0.06f); glPopMatrix();

    setMaterial(0.02f,0.02f,0.02f, 0.05f,0.05f,0.05f, 0.10f,0.10f,0.10f, 10.0f);
    glPushMatrix(); glTranslatef(cx,cy,z+0.06f); drawBox(2.3f,2.7f,0.04f); glPopMatrix();

    const float sq = 0.26f;
    for (int r=0; r<8; ++r)
    for (int c=0; c<8; ++c)
    {
        if ((r+c)%2==0) setMaterial(0.30f,0.28f,0.24f, 0.88f,0.84f,0.72f, 0.10f,0.10f,0.10f, 14.0f);
        else            setMaterial(0.02f,0.02f,0.02f, 0.08f,0.08f,0.08f, 0.08f,0.08f,0.08f, 14.0f);

        float px = cx + (c-3.5f)*sq;
        float py = cy + (3.5f-r)*sq;
        glPushMatrix(); glTranslatef(px,py,z+0.10f); drawBox(sq*0.94f,sq*0.94f,0.03f); glPopMatrix();
    }
}

// Warm glowing wall sconce. rotY orients the backing plate against its wall.
void drawWallLamp(float x, float y, float z, float rotY)
{
    glPushMatrix();
    glTranslatef(x,y,z);
    glRotatef(rotY,0,1,0);

    matMetal();
    glPushMatrix(); glTranslatef(0,0,0.05f); drawBox(0.30f,0.42f,0.08f); glPopMatrix();
    glPushMatrix(); glTranslatef(0,-0.16f,0.14f); drawBox(0.10f,0.10f,0.16f); glPopMatrix();

    GLboolean lightingWasOn = glIsEnabled(GL_LIGHTING);
    if (lightingWasOn) glDisable(GL_LIGHTING);
    glColor3f(1.0f,0.78f,0.32f);
    glPushMatrix(); glTranslatef(0,-0.16f,0.24f); glutSolidSphere(0.11f,14,10); glPopMatrix();
    if (lightingWasOn) glEnable(GL_LIGHTING);

    glPopMatrix();
}

// A small potted plant: terracotta pot with a cluster of green foliage.
void drawPottedPlant(float x, float z, float scale=1.0f)
{
    glPushMatrix();
    glTranslatef(x,0,z);
    glScalef(scale,scale,scale);

    setMaterial(0.16f,0.06f,0.02f, 0.55f,0.24f,0.10f, 0.12f,0.08f,0.05f, 12.0f);
    glPushMatrix(); glTranslatef(0,0.22f,0); drawBox(0.55f,0.44f,0.55f); glPopMatrix();

    matGreen();
    glPushMatrix(); glTranslatef(0,0.62f,0); glutSolidSphere(0.42f,16,12); glPopMatrix();
    glPushMatrix(); glTranslatef(-0.22f,0.50f,0.10f); glutSolidSphere(0.28f,14,10); glPopMatrix();
    glPushMatrix(); glTranslatef(0.24f,0.48f,-0.08f); glutSolidSphere(0.30f,14,10); glPopMatrix();
    glPushMatrix(); glTranslatef(0.02f,0.90f,0.05f); glutSolidSphere(0.24f,14,10); glPopMatrix();

    glPopMatrix();
}

// A small gold trophy cup on a stand.
void drawTrophy(float x, float y, float z)
{
    glPushMatrix();
    glTranslatef(x,y,z);

    matGold();
    drawCylinderY(0.16f,0.06f,20);
    glPushMatrix(); glTranslatef(0,0.06f,0); drawCylinderY(0.05f,0.20f,14); glPopMatrix();
    glPushMatrix(); glTranslatef(0,0.26f,0); drawCylinderY(0.13f,0.05f,20); glPopMatrix();
    glPushMatrix(); glTranslatef(0,0.31f,0); glutSolidSphere(0.16f,18,14); glPopMatrix();
    glPushMatrix(); glTranslatef(0,0.44f,0); drawCylinderY(0.045f,0.10f,12); glPopMatrix();

    glPopMatrix();
}

// A low cabinet against a wall, with a trophy displayed on top.
void drawCabinet(float cx, float cz)
{
    setMaterial(0.30f,0.06f,0.05f, 0.62f,0.12f,0.10f, 0.20f,0.10f,0.10f, 30.0f);
    glPushMatrix(); glTranslatef(cx,1.02f,cz); drawBox(2.0f,0.10f,1.0f); glPopMatrix();

    setMaterial(0.18f,0.18f,0.18f, 0.85f,0.85f,0.85f, 0.15f,0.15f,0.15f, 20.0f);
    glPushMatrix(); glTranslatef(cx,0.48f,cz); drawBox(1.9f,0.95f,0.95f); glPopMatrix();

    drawTrophy(cx,1.07f,cz);
}

// A wall-mounted bookshelf with a few shelves of colourful book spines.
// Uses the same wall convention as drawPoster/drawWallLamp: rotY=90 for the
// left wall, rotY=-90 for the right wall (local +Z then points into the room).
void drawBookshelf(float x, float y, float z, float rotY,
                   float shelfW, float shelfH, float shelfD)
{
    glPushMatrix();
    glTranslatef(x,y,z);
    glRotatef(rotY,0,1,0);

    matWood();
    glPushMatrix(); glTranslatef(0,shelfH*0.5f,shelfD-0.03f); drawBox(shelfW,shelfH,0.06f); glPopMatrix();
    glPushMatrix(); glTranslatef(0,shelfH,shelfD*0.5f); drawBox(shelfW,0.08f,shelfD); glPopMatrix();
    glPushMatrix(); glTranslatef(0,0,shelfD*0.5f); drawBox(shelfW,0.08f,shelfD); glPopMatrix();
    glPushMatrix(); glTranslatef(-shelfW*0.5f+0.04f,shelfH*0.5f,shelfD*0.5f); drawBox(0.08f,shelfH,shelfD); glPopMatrix();
    glPushMatrix(); glTranslatef( shelfW*0.5f-0.04f,shelfH*0.5f,shelfD*0.5f); drawBox(0.08f,shelfH,shelfD); glPopMatrix();

    const int numShelves = 4;
    for (int i=1; i<numShelves; ++i)
    {
        float sy = shelfH*i/(float)numShelves;
        glPushMatrix(); glTranslatef(0,sy,shelfD*0.5f); drawBox(shelfW-0.10f,0.05f,shelfD-0.05f); glPopMatrix();
    }

    static const float bookColors[6][3] = {
        {0.55f,0.10f,0.10f}, {0.10f,0.30f,0.55f}, {0.15f,0.45f,0.20f},
        {0.60f,0.45f,0.08f}, {0.35f,0.15f,0.45f}, {0.55f,0.55f,0.55f}
    };
    int colorIdx = 0;
    for (int level=0; level<numShelves; ++level)
    {
        float baseY = shelfH*level/(float)numShelves + 0.10f;
        float xCur  = -shelfW*0.5f + 0.16f;
        while (xCur < shelfW*0.5f - 0.14f)
        {
            float bw = 0.10f + 0.03f*(colorIdx%3);
            float bh = 0.55f + 0.10f*((colorIdx+level)%3);
            const float* c = bookColors[colorIdx % 6];
            setMaterial(c[0]*0.3f,c[1]*0.3f,c[2]*0.3f, c[0],c[1],c[2], 0.10f,0.10f,0.10f, 12.0f);
            glPushMatrix();
            glTranslatef(xCur+bw*0.5f, baseY+bh*0.5f, shelfD*0.5f);
            drawBox(bw,bh,shelfD-0.10f);
            glPopMatrix();
            xCur += bw+0.02f;
            ++colorIdx;
        }
    }

    glPopMatrix();
}

// A small hanging vine cascading down a wall, made of shrinking green blobs.
void drawHangingVine(float x, float topY, float z, float length)
{
    matGreen();
    int segs = (int)(length/0.32f);
    if (segs < 2) segs = 2;
    for (int i=0; i<segs; ++i)
    {
        float t = i/(float)segs;
        float y = topY - t*length;
        float sway = 0.10f*std::sin(t*6.0f + x*0.7f);
        float s = 0.15f - 0.09f*t;
        if (s < 0.05f) s = 0.05f;
        glPushMatrix();
        glTranslatef(x+sway,y,z);
        glutSolidSphere(s,10,8);
        glPopMatrix();
    }
}

// A small round side table with a couple of drink cans, for the spectators.
void drawSideTable(float x, float z)
{
    matWood();
    glPushMatrix(); glTranslatef(x,0.95f,z); drawCylinderY(0.55f,0.06f,20); glPopMatrix();
    glPushMatrix(); glTranslatef(x,0.0f,z); drawCylinderY(0.08f,0.95f,12); glPopMatrix();

    static const float canColors[3][3] = { {0.65f,0.08f,0.08f}, {0.08f,0.35f,0.10f}, {0.75f,0.75f,0.78f} };
    for (int i=0;i<2;++i)
    {
        const float* c = canColors[(int)(x*3+i+z) % 3 < 0 ? 0 : (int)(x*3+i+z) % 3];
        setMaterial(c[0]*0.3f,c[1]*0.3f,c[2]*0.3f, c[0],c[1],c[2], 0.35f,0.35f,0.35f, 40.0f);
        float ox = (i==0) ? -0.18f : 0.18f;
        float oz = (i==0) ? 0.10f : -0.12f;
        glPushMatrix(); glTranslatef(x+ox,1.13f,z+oz); drawCylinderY(0.075f,0.20f,14); glPopMatrix();
    }
}

// A low writing desk with a laptop, a labelled stack of books and a pen cup,
// meant to sit near the front of the room as if the viewer's own desk.
void drawDeskScene(float cx, float cz)
{
    matWood();
    glPushMatrix(); glTranslatef(cx,0.90f,cz); drawBox(3.2f,0.10f,1.3f); glPopMatrix();
    for (int sx=-1; sx<=1; sx+=2)
    for (int sz=-1; sz<=1; sz+=2)
    {
        glPushMatrix(); glTranslatef(cx+sx*1.45f,0.45f,cz+sz*0.55f); drawBox(0.10f,0.90f,0.10f); glPopMatrix();
    }

    // Laptop: base + upright screen, angled slightly open.
    float lx = cx+0.85f;
    setMaterial(0.05f,0.05f,0.06f, 0.16f,0.16f,0.18f, 0.40f,0.40f,0.42f, 60.0f);
    glPushMatrix(); glTranslatef(lx,0.97f,cz); drawBox(0.85f,0.04f,0.60f); glPopMatrix();
    glPushMatrix();
    glTranslatef(lx,0.97f,cz-0.29f);
    glRotatef(-12.0f,1,0,0);
    glTranslatef(0,0.30f,0);
    drawBox(0.85f,0.60f,0.03f);
    glPopMatrix();

    // Stack of three labelled books.
    float bx = cx-0.95f;
    static const float sc[3][3] = { {0.10f,0.10f,0.10f}, {0.35f,0.02f,0.05f}, {0.05f,0.20f,0.10f} };
    static const char* labels[3] = { "VICTORY", "MINDSET", "CHESS STRATEGY" };
    float by = 0.95f;
    for (int i=0;i<3;++i)
    {
        const float* c = sc[i];
        setMaterial(c[0]*0.4f,c[1]*0.4f,c[2]*0.4f, c[0],c[1],c[2], 0.12f,0.12f,0.12f, 14.0f);
        glPushMatrix(); glTranslatef(bx,by+0.09f,cz); drawBox(1.05f,0.16f,0.75f); glPopMatrix();

        GLboolean lightingWasOn = glIsEnabled(GL_LIGHTING);
        if (lightingWasOn) glDisable(GL_LIGHTING);
        glColor3f(0.96f,0.88f,0.64f);
        if (lightingWasOn) glEnable(GL_LIGHTING);

        float labelScale = 0.00120f;
        float maxBookWidth = 0.82f;
        float rawWidth = (float)glutStrokeLength(GLUT_STROKE_ROMAN, (const unsigned char*)labels[i]) * labelScale;
        if (rawWidth > maxBookWidth)
            labelScale *= (maxBookWidth / rawWidth);

        float finalWidth = (float)glutStrokeLength(GLUT_STROKE_ROMAN, (const unsigned char*)labels[i]) * labelScale;
        float textX = bx - finalWidth * 0.5f;
        draw3DText(textX, by+0.040f, cz+0.385f, labelScale, 0.0f, labels[i]);

        by += 0.18f;
    }

    // Pen cup with a couple of pens.
    matDarkScreen();
    glPushMatrix(); glTranslatef(cx+0.05f,1.10f,cz+0.55f); drawCylinderY(0.10f,0.20f,14); glPopMatrix();
    matGold();
    glPushMatrix(); glTranslatef(cx+0.02f,1.28f,cz+0.55f); glRotatef(12,0,0,1); drawCylinderY(0.012f,0.26f,8); glPopMatrix();
    matRed();
    glPushMatrix(); glTranslatef(cx+0.09f,1.28f,cz+0.52f); glRotatef(-8,0,0,1); drawCylinderY(0.012f,0.24f,8); glPopMatrix();
}

// Layered rug under the main table, sitting just above the floor.
void drawCarpet(float cx, float cz, float w, float d)
{
    setMaterial(0.20f,0.02f,0.02f, 0.55f,0.08f,0.06f, 0.08f,0.04f,0.04f, 10.0f);
    glPushMatrix(); glTranslatef(cx,0.005f,cz); drawBox(w,0.02f,d); glPopMatrix();

    setMaterial(0.20f,0.13f,0.02f, 0.72f,0.48f,0.10f, 0.10f,0.08f,0.03f, 10.0f);
    glPushMatrix(); glTranslatef(cx,0.012f,cz); drawBox(w*0.82f,0.02f,d*0.82f); glPopMatrix();
}

// Coffered, coloured ceiling for the tournament hall - a grid of alternating
// dark-wood and maroon recessed panels plus a couple of round light
// fixtures, replacing the old flat grey slab.
void drawCeiling()
{
    // Base ceiling slab, warm cream instead of flat grey.
    setMaterial(0.20f,0.17f,0.12f, 0.82f,0.76f,0.62f, 0.15f,0.15f,0.12f, 16.0f);
    glPushMatrix(); glTranslatef(0,8.0f,0); drawBox(20.0f,0.15f,20.0f); glPopMatrix();

    // Coffered grid of colour panels recessed just below the slab.
    const int gridN = 6;
    const float cellSize = 18.0f/gridN;
    for (int r=0;r<gridN;++r)
    for (int c=0;c<gridN;++c)
    {
        bool alt = ((r+c)%2==0);
        if (alt)
            setMaterial(0.16f,0.07f,0.02f, 0.42f,0.20f,0.08f, 0.15f,0.10f,0.05f, 20.0f); // dark wood
        else
            setMaterial(0.22f,0.05f,0.05f, 0.55f,0.10f,0.09f, 0.10f,0.06f,0.06f, 16.0f); // maroon accent

        float px = -9.0f + cellSize*(c+0.5f);
        float pz = -9.0f + cellSize*(r+0.5f);
        glPushMatrix(); glTranslatef(px,7.90f,pz); drawBox(cellSize*0.86f,0.06f,cellSize*0.86f); glPopMatrix();
    }

    // A gold border strip framing the whole coffered grid.
    setMaterial(0.22f,0.16f,0.03f, 0.68f,0.50f,0.10f, 0.30f,0.25f,0.10f, 40.0f);
    glPushMatrix(); glTranslatef(0,7.905f,-9.05f); drawBox(18.2f,0.05f,0.18f); glPopMatrix();
    glPushMatrix(); glTranslatef(0,7.905f, 9.05f); drawBox(18.2f,0.05f,0.18f); glPopMatrix();
    glPushMatrix(); glTranslatef(-9.05f,7.905f,0); drawBox(0.18f,0.05f,18.2f); glPopMatrix();
    glPushMatrix(); glTranslatef( 9.05f,7.905f,0); drawBox(0.18f,0.05f,18.2f); glPopMatrix();

    // Recessed round light fixtures over each table.
    GLboolean lightingWasOn = glIsEnabled(GL_LIGHTING);
    if (lightingWasOn) glDisable(GL_LIGHTING);
    glColor3f(1.0f,0.93f,0.75f);
    glPushMatrix(); glTranslatef(0.0f,7.86f,2.0f); drawCylinderY(0.38f,0.03f,20); glPopMatrix();
    glPushMatrix(); glTranslatef(0.0f,7.86f,-5.6f); drawCylinderY(0.30f,0.03f,20); glPopMatrix();
    if (lightingWasOn) glEnable(GL_LIGHTING);
}

// A three-seat sofa (base cushion, backrest, two armrests and short legs)
// for a lounge nook where spectators can sit rather than stand.
void drawSofa(float x, float z, float angle, float r, float g, float b)
{
    glPushMatrix();
    glTranslatef(x,0,z);
    glRotatef(angle,0,1,0);

    // Short wooden legs.
    matWood();
    for (int sx=-1; sx<=1; sx+=2)
    for (int sz=-1; sz<=1; sz+=2)
    {
        glPushMatrix(); glTranslatef(sx*1.05f,0.175f,sz*0.36f); drawBox(0.10f,0.35f,0.10f); glPopMatrix();
    }

    // Seat cushion (three segments, lightly separated so it doesn't read as
    // one solid slab).
    setMaterial(r*0.22f,g*0.22f,b*0.22f, r,g,b, 0.10f,0.10f,0.10f, 14.0f);
    for (int i=-1; i<=1; ++i)
    {
        glPushMatrix(); glTranslatef(i*0.72f,0.50f,0.0f); drawBox(0.66f,0.30f,0.85f); glPopMatrix();
    }

    // Backrest.
    glPushMatrix(); glTranslatef(0.0f,1.00f,0.36f); drawBox(2.30f,0.75f,0.20f); glPopMatrix();
    for (int i=-1; i<=1; ++i)
    {
        glPushMatrix(); glTranslatef(i*0.72f,1.02f,0.30f); glutSolidSphere(0.32,14,10); glPopMatrix(); // back cushions
    }

    // Rolled armrests on both ends.
    setMaterial(r*0.20f,g*0.20f,b*0.20f, r*0.88f,g*0.88f,b*0.88f, 0.10f,0.10f,0.10f, 14.0f);
    for (int s=-1; s<=1; s+=2)
    {
        glPushMatrix(); glTranslatef(s*1.15f,0.72f,0.0f); drawBox(0.24f,0.62f,0.95f); glPopMatrix();
        glPushMatrix(); glTranslatef(s*1.15f,1.02f,-0.475f); glRotatef(90,1,0,0); drawCylinderY(0.12f,0.95f,14); glPopMatrix();
    }

    glPopMatrix();
}

// A seated audience figure, built like drawSpectator() but with bent knees
// so it can sit on a sofa/chair whose seat is at y = seatY.
void drawSeatedSpectator(float x, float z, float angle, float shirtR, float shirtG, float shirtB, float seatY=0.65f)
{
    glPushMatrix();
    glTranslatef(x,seatY,z);
    glRotatef(angle,0,1,0);

    // ---- legs: thigh runs forward (local -Z) from the hip, calf drops to the floor ----
    for (int s=-1; s<=1; s+=2)
    {
        float lx = s*0.22f;
        setMaterial(0.04f,0.04f,0.05f, 0.14f,0.14f,0.16f, 0.08f,0.08f,0.09f, 12.0f);

        glPushMatrix();
        glTranslatef(lx,0.02f,-0.20f);
        glRotatef(88.0f,1,0,0);
        drawTaperedCylinderY(0.135f,0.115f,0.40f,14);                                     // thigh
        glPopMatrix();

        glPushMatrix(); glTranslatef(lx,0.0f,-0.40f); glutSolidSphere(0.115,12,10); glPopMatrix(); // knee

        glPushMatrix(); glTranslatef(lx,0.0f,-0.40f); drawTaperedCylinderY(0.115f,0.10f,seatY,14); glPopMatrix(); // calf to floor

        setMaterial(0.02f,0.02f,0.02f, 0.07f,0.07f,0.08f, 0.10f,0.10f,0.10f, 20.0f);
        glPushMatrix(); glTranslatef(lx,-seatY+0.04f,-0.30f); drawBox(0.17f,0.08f,0.30f); glPopMatrix();          // shoe
    }

    // ---- pelvis / torso / head / arms, same proportions as drawSpectator()
    // but shifted down by the standing pelvis height (0.92) since the hip
    // already sits at the seat. ----
    setMaterial(shirtR*0.20f,shirtG*0.20f,shirtB*0.20f, shirtR*0.80f,shirtG*0.80f,shirtB*0.80f, 0.12f,0.12f,0.12f, 14.0f);
    glPushMatrix(); glTranslatef(0,0.0f,0); drawBox(0.70f,0.20f,0.38f); glPopMatrix();

    setMaterial(shirtR*0.24f,shirtG*0.24f,shirtB*0.24f, shirtR,shirtG,shirtB, 0.14f,0.14f,0.14f, 16.0f);
    glPushMatrix(); glTranslatef(0,0.36f,0); drawBox(0.74f,0.50f,0.44f); glPopMatrix();
    glPushMatrix(); glTranslatef(0,0.80f,0); drawBox(0.88f,0.42f,0.46f); glPopMatrix();
    for (int s=-1;s<=1;s+=2)
    {
        glPushMatrix(); glTranslatef(s*0.43f,0.96f,0); glutSolidSphere(0.14,14,10); glPopMatrix();
    }

    matSkin();
    glPushMatrix(); glTranslatef(0,1.30f,0); drawTaperedCylinderY(0.135f,0.125f,0.20f,16); glPopMatrix();

    glPushMatrix(); glTranslatef(0,1.63f,0); glutSolidSphere(0.35,24,18); glPopMatrix();

    for (int s=-1;s<=1;s+=2)
    {
        glPushMatrix(); glTranslatef(s*0.35f,1.61f,-0.02f); glScalef(0.5f,1.0f,0.7f); glutSolidSphere(0.085,12,8); glPopMatrix();
    }

    matHair();
    glPushMatrix(); glTranslatef(0,1.84f,0.02f); glScalef(1.0f,0.40f,1.0f); glutSolidSphere(0.36,22,16); glPopMatrix();

    matHair();
    for (int s=-1;s<=1;s+=2)
    {
        glPushMatrix(); glTranslatef(s*0.12f,1.735f,-0.315f); drawBox(0.09f,0.018f,0.02f); glPopMatrix();
    }

    setMaterial(0.01f,0.01f,0.01f, 0.03f,0.03f,0.03f, 0.10f,0.10f,0.10f, 10.0f);
    glPushMatrix(); glTranslatef(-0.12f,1.69f,-0.315f); glutSolidSphere(0.032,10,8); glPopMatrix();
    glPushMatrix(); glTranslatef( 0.12f,1.69f,-0.315f); glutSolidSphere(0.032,10,8); glPopMatrix();

    // Arms bent forward, resting on the knees rather than hanging down.
    for (int s=-1;s<=1;s+=2)
    {
        setMaterial(shirtR*0.24f,shirtG*0.24f,shirtB*0.24f, shirtR,shirtG,shirtB, 0.14f,0.14f,0.14f, 16.0f);
        glPushMatrix();
        glTranslatef(s*0.51f,0.98f,0.0f);
        glRotatef(-70.0f,1,0,0);
        glRotatef(s*4.0f,0,0,1);
        drawTaperedCylinderY(0.100f,0.085f,0.36f,14);                       // upper arm
        glTranslatef(0,0.36f,0);
        glutSolidSphere(0.082,12,10);                                       // elbow
        glRotatef(35.0f,1,0,0);
        drawTaperedCylinderY(0.085f,0.070f,0.32f,14);                       // forearm down to the knee
        glTranslatef(0,0.32f,0);

        setMaterial(0.16f,0.10f,0.06f, 0.68f,0.45f,0.30f, 0.09f,0.09f,0.08f, 10.0f);
        glPushMatrix(); glScalef(1.0f,0.85f,1.2f); glutSolidSphere(0.10,14,10); glPopMatrix();
        glPopMatrix();
    }

    glPopMatrix();
}

// Low wooden coffee table with a snack bowl, set in front of a sofa.
void drawCoffeeTable(float x, float z)
{
    matWood();
    glPushMatrix(); glTranslatef(x,0.42f,z); drawTexturedBox(1.3f,0.10f,0.75f,woodTex,1,1); glPopMatrix();
    for (int sx=-1; sx<=1; sx+=2)
    for (int sz=-1; sz<=1; sz+=2)
    {
        glPushMatrix(); glTranslatef(x+sx*0.55f,0.20f,z+sz*0.30f); drawBox(0.08f,0.40f,0.08f); glPopMatrix();
    }
    matGold();
    glPushMatrix(); glTranslatef(x,0.50f,z); drawCylinderY(0.18f,0.07f,16); glPopMatrix();
}

// A standing floor lamp with a glowing shade near a sofa nook.
void drawFloorLamp(float x, float z)
{
    matMetal();
    glPushMatrix(); glTranslatef(x,0.04f,z); drawCylinderY(0.22f,0.06f,18); glPopMatrix();
    glPushMatrix(); glTranslatef(x,1.25f,z); drawCylinderY(0.035f,2.35f,10); glPopMatrix();

    GLboolean lightingWasOn = glIsEnabled(GL_LIGHTING);
    if (lightingWasOn) glDisable(GL_LIGHTING);
    glColor3f(1.0f,0.92f,0.72f);
    glPushMatrix(); glTranslatef(x,2.65f,z); drawTaperedCylinderY(0.42f,0.30f,0.55f,16); glPopMatrix();
    if (lightingWasOn) glEnable(GL_LIGHTING);
}

// Draped curtain panel with a simple rod, flanking the window.
void drawCurtain(float x, float z, float topY, float botY, float r, float g, float b)
{
    matMetal();
    glPushMatrix(); glTranslatef(x,topY+0.08f,z-0.06f); glRotatef(90,1,0,0); drawCylinderY(0.03f,0.75f,10); glPopMatrix();

    setMaterial(r*0.25f,g*0.22f,b*0.20f, r,g,b, 0.08f,0.08f,0.08f, 10.0f);
    float h = topY-botY;
    for (int i=-1; i<=1; ++i)
    {
        glPushMatrix(); glTranslatef(x+i*0.16f,botY+h*0.5f,z-0.03f); drawBox(0.20f,h,0.07f); glPopMatrix();
    }
}

// A wall-mounted flat-screen TV on a wood accent panel with a thin blue
// LED trim, showing a muted "broadcast" (header bar + colour blocks)
// rather than the live match itself.
void drawWallTV(float x, float y, float z, float rotY, float w=2.4f, float h=1.4f)
{
    glPushMatrix();
    glTranslatef(x,y,z);
    glRotatef(rotY,0,1,0);

    // Wood accent panel behind the screen, wider than the TV itself.
    matWood();
    glPushMatrix(); glTranslatef(0,0,0.02f); drawTexturedBox(w+1.0f,h+1.0f,0.05f,woodTex,1,1); glPopMatrix();

    // Slim wall mount bracket.
    matMetal();
    glPushMatrix(); glTranslatef(0,0,0.07f); drawBox(0.30f,0.30f,0.10f); glPopMatrix();

    // Black bezel.
    setMaterial(0.01f,0.01f,0.01f, 0.03f,0.03f,0.04f, 0.20f,0.20f,0.22f, 60.0f);
    glPushMatrix(); glTranslatef(0,0,0.12f); drawBox(w,h,0.08f); glPopMatrix();

    // Screen + LED trim are unlit so the wall nook still glows softly in
    // the dimmer indoor lighting stages.
    GLboolean lightingWasOn = glIsEnabled(GL_LIGHTING);
    if (lightingWasOn) glDisable(GL_LIGHTING);

    glColor3f(0.05f,0.08f,0.14f);
    glPushMatrix(); glTranslatef(0,0,0.165f); drawBox(w-0.16f,h-0.16f,0.03f); glPopMatrix();

    glColor3f(0.85f,0.15f,0.15f);
    glPushMatrix(); glTranslatef(0,h*0.30f,0.185f); drawBox(w-0.24f,0.16f,0.02f); glPopMatrix();

    static const float bar[4][3] = { {0.95f,0.75f,0.15f},{0.20f,0.55f,0.85f},{0.30f,0.75f,0.35f},{0.80f,0.35f,0.75f} };
    for (int i=0;i<4;++i)
    {
        glColor3f(bar[i][0],bar[i][1],bar[i][2]);
        float bx = -w*0.5f + (w/4.0f)*(i+0.5f) + 0.06f;
        glPushMatrix(); glTranslatef(bx,-h*0.12f,0.185f); drawBox(w/4.0f-0.12f,h*0.34f,0.02f); glPopMatrix();
    }

    glColor3f(0.25f,0.65f,0.95f);
    glPushMatrix(); glTranslatef(0,(h+1.0f)*0.5f-0.03f,0.045f); drawBox(w+0.9f,0.03f,0.02f); glPopMatrix();
    glPushMatrix(); glTranslatef(0,-(h+1.0f)*0.5f+0.03f,0.045f); drawBox(w+0.9f,0.03f,0.02f); glPopMatrix();

    if (lightingWasOn) glEnable(GL_LIGHTING);

    glPopMatrix();
}

// A small floating wall shelf holding a trophy and a mini potted plant,
// meant to sit just under a wall TV.
void drawWallShelf(float x, float y, float z, float rotY)
{
    glPushMatrix();
    glTranslatef(x,y,z);
    glRotatef(rotY,0,1,0);

    matWood();
    glPushMatrix(); glTranslatef(0,0,0.20f); drawBox(1.6f,0.08f,0.34f); glPopMatrix();
    for (int s=-1; s<=1; s+=2)
    {
        glPushMatrix(); glTranslatef(s*0.65f,-0.10f,0.10f); drawBox(0.06f,0.20f,0.06f); glPopMatrix();
    }

    drawTrophy(-0.45f,0.06f,0.24f);
    glPushMatrix(); glTranslatef(0,0.04f,0); drawPottedPlant(0.45f,0.24f,0.45f); glPopMatrix();

    glPopMatrix();
}

// Two lounge nooks (sofa + seated audience + coffee table + floor lamp)
// tucked into the open front corners of the hall.
void drawLoungeArea()
{
    drawCarpet(-6.5f,6.0f,2.7f,2.1f);
    drawSofa(-6.5f,6.3f,0.0f, 0.32f,0.28f,0.55f);
    drawSeatedSpectator(-7.15f,6.05f,0.0f, 0.55f,0.15f,0.15f);
    drawSeatedSpectator(-6.50f,6.05f,0.0f, 0.20f,0.50f,0.30f);
    drawSeatedSpectator(-5.85f,6.05f,0.0f, 0.60f,0.55f,0.10f);
    drawCoffeeTable(-6.5f,5.05f);
    drawFloorLamp(-8.05f,6.7f);

    drawCarpet(6.5f,6.0f,2.7f,2.1f);
    drawSofa(6.5f,6.3f,0.0f, 0.55f,0.30f,0.18f);
    drawSeatedSpectator(5.85f,6.05f,0.0f, 0.15f,0.35f,0.55f);
    drawSeatedSpectator(6.50f,6.05f,0.0f, 0.55f,0.45f,0.45f);
    drawSeatedSpectator(7.15f,6.05f,0.0f, 0.20f,0.55f,0.55f);
    drawCoffeeTable(6.5f,5.05f);
    drawFloorLamp(8.05f,6.7f);
}

void drawRoomDecor()
{
    drawCarpet(0.0f,2.0f,5.6f,7.0f);

    drawPoster(-9.85f,5.2f,-3.0f, 90.0f, 2.6f,3.0f, "CHESS", "TOURNAMENT");
    drawPoster( 9.85f,5.2f,-3.0f,-90.0f, 2.6f,3.0f, "Good Moves", "Better Minds");
    drawFramedChessPicture(-2.6f,5.6f,-10.0f);

    drawWallLamp(-9.85f,4.2f, 1.0f, 90.0f);
    drawWallLamp( 9.85f,4.2f, 1.0f,-90.0f);
    drawWallLamp(-9.85f,4.2f,-6.5f, 90.0f);
    drawWallLamp( 9.85f,4.2f,-6.5f,-90.0f);

    drawPottedPlant(-9.0f,-8.2f,1.0f);
    drawPottedPlant( 9.0f,-8.2f,0.9f);
    drawPottedPlant(-9.0f, 8.5f,1.1f);
    drawPottedPlant( 9.0f, 8.5f,1.0f);

    drawCabinet(9.0f,-7.5f);

    // Bookshelves flanking the room, each topped with a small trophy.
    drawBookshelf(-9.55f,0.0f,4.5f, 90.0f, 1.6f,5.0f,0.45f);
    drawBookshelf( 9.55f,0.0f,4.5f,-90.0f, 1.6f,5.0f,0.45f);
    drawTrophy(-9.05f,5.06f,4.5f);
    drawTrophy( 9.05f,5.06f,4.5f);

    // Ivy hanging near the ceiling in a few spots.
    drawHangingVine(-6.5f,7.9f,-9.6f,2.6f);
    drawHangingVine( 2.6f,7.9f,-9.6f,2.2f);
    drawHangingVine(-9.9f,7.9f, 3.5f,2.4f);
    drawHangingVine( 9.9f,7.9f, 3.5f,2.4f);
    drawHangingVine(-9.9f,7.9f,-6.0f,2.0f);
    drawHangingVine( 9.9f,7.9f,-6.0f,2.0f);

    // Side tables with drinks near the spectators.
    drawSideTable(-6.6f,2.6f);
    drawSideTable( 6.6f,2.6f);
    drawSideTable(-3.0f,-6.0f);
    drawSideTable( 3.0f,-6.0f);

    // The viewer's own foreground desk, near the front of the room.
    drawDeskScene(0.0f,8.6f);

    // Curtains flanking the window.
    drawCurtain(3.55f,-9.75f,7.05f,1.55f, 0.45f,0.08f,0.08f);
    drawCurtain(8.65f,-9.75f,7.05f,1.55f, 0.45f,0.08f,0.08f);

    // Two sofa lounge nooks with seated spectators, so not everyone in the
    // crowd is left standing.
    drawLoungeArea();

    // Wall-mounted flat-screen TVs on a wood accent panel with a small
    // trophy shelf underneath, dressing up the side walls above each sofa.
    drawWallTV(-9.85f,4.35f,6.3f, 90.0f);
    drawWallTV( 9.85f,4.35f,6.3f,-90.0f);
    drawWallShelf(-9.75f,3.15f,6.3f, 90.0f);
    drawWallShelf( 9.75f,3.15f,6.3f,-90.0f);
}


void drawRoom()
{
    // Floor
    matFloorPolished();
    glPushMatrix();
    glTranslatef(0,-0.10f,0);
    drawTexturedBox(20.0f,0.20f,20.0f,floorTex,8.0f,8.0f);
    glPopMatrix();

    // Left / right walls
    matWall();
    glPushMatrix(); glTranslatef(-10.0f,4.0f,0); drawTexturedBox(0.20f,8.0f,20.0f,wallTex,6.0f,3.0f); glPopMatrix();
    glPushMatrix(); glTranslatef( 10.0f,4.0f,0); drawTexturedBox(0.20f,8.0f,20.0f,wallTex,6.0f,3.0f); glPopMatrix();

    // Back wall has TWO real openings:
    // door:   x = -8.65 .. -5.55, y = 0 .. 4.10
    // window: x =  4.20 ..  8.40, y = 0 .. 6.60  (floor-to-ceiling)
    const float z=-10.0f;

    // Full-height pieces left of door, between door/window, and right of window.
    glPushMatrix(); glTranslatef(-9.325f,4.0f,z); drawTexturedBox(1.35f,8.0f,0.20f,wallTex,1,3); glPopMatrix();
    glPushMatrix(); glTranslatef(-0.675f,4.0f,z); drawTexturedBox(9.75f,8.0f,0.20f,wallTex,4,3); glPopMatrix();
    glPushMatrix(); glTranslatef( 9.20f, 4.0f,z); drawTexturedBox(1.60f,8.0f,0.20f,wallTex,1,3); glPopMatrix();

    // Wall above the door opening.
    glPushMatrix(); glTranslatef(-7.10f,6.05f,z); drawTexturedBox(3.10f,3.90f,0.20f,wallTex,2,2); glPopMatrix();

    // Floor-to-ceiling window: only the upper wall remains.
    glPushMatrix(); glTranslatef(6.30f,7.30f,z); drawTexturedBox(4.20f,1.40f,0.20f,wallTex,2,1); glPopMatrix();

    // Ceiling - coloured coffered ceiling instead of a flat grey slab.
    drawCeiling();

    drawWindowFrame();
    drawWindowGlass();
    drawDoor();
    drawRoomDecor();
}


// ================================================================
// FURNITURE
// ================================================================
void drawTable(float cx, float cz, float w=5.0f, float d=5.0f)
{
    matWood();
    glPushMatrix();
    glTranslatef(cx,2.25f,cz);
    drawTexturedBox(w,0.28f,d,woodTex,3.0f,3.0f);
    glPopMatrix();

    float lx=w*0.42f, lz=d*0.42f;
    for (int sx=-1; sx<=1; sx+=2)
    for (int sz=-1; sz<=1; sz+=2)
    {
        glPushMatrix();
        glTranslatef(cx+sx*lx,1.10f,cz+sz*lz);
        drawTexturedBox(0.28f,2.2f,0.28f,woodTex,1.0f,2.0f);
        glPopMatrix();
    }
}

void drawChair(float x, float z, float angle)
{
    glPushMatrix();
    glTranslatef(x,0,z);
    glRotatef(angle,0,1,0);

    matWood();
    // seat
    glPushMatrix(); glTranslatef(0,0.95f,0); drawTexturedBox(1.25f,0.18f,1.25f,woodTex,1,1); glPopMatrix();
    // back
    glPushMatrix(); glTranslatef(0,1.85f,0.52f); drawTexturedBox(1.25f,1.7f,0.16f,woodTex,1,1); glPopMatrix();
    // legs
    for (int sx=-1; sx<=1; sx+=2)
    for (int sz=-1; sz<=1; sz+=2)
    {
        glPushMatrix(); glTranslatef(0.45f*sx,0.45f,0.45f*sz); drawBox(0.14f,0.9f,0.14f); glPopMatrix();
    }
    glPopMatrix();
}

void drawChessTimer(float x, float y, float z)
{
    matMetal();
    glPushMatrix(); glTranslatef(x,y,z); drawBox(1.15f,0.55f,0.45f); glPopMatrix();

    matDarkScreen();
    glPushMatrix(); glTranslatef(x-0.28f,y,z+0.24f); drawBox(0.40f,0.25f,0.035f); glPopMatrix();
    glPushMatrix(); glTranslatef(x+0.28f,y,z+0.24f); drawBox(0.40f,0.25f,0.035f); glPopMatrix();

    matRed();
    glPushMatrix(); glTranslatef(x-0.28f,y+0.34f,z); drawCylinderY(0.09f,0.10f,18); glPopMatrix();
    glPushMatrix(); glTranslatef(x+0.28f,y+0.34f,z); drawCylinderY(0.09f,0.10f,18); glPopMatrix();
}

// ================================================================
// CHESS PIECES
// ================================================================
void drawPawn()
{
    drawTaperedCylinderY(0.30f,0.24f,0.07f);                                       // flared foot
    glPushMatrix(); glTranslatef(0,0.07f,0); drawCylinderY(0.22f,0.03f,28); glPopMatrix();       // base ring
    glPushMatrix(); glTranslatef(0,0.10f,0); drawTaperedCylinderY(0.19f,0.14f,0.32f); glPopMatrix(); // tapered stem
    glPushMatrix(); glTranslatef(0,0.42f,0); drawCylinderY(0.15f,0.03f,28); glPopMatrix();       // neck collar
    glPushMatrix(); glTranslatef(0,0.45f+0.19f,0); glutSolidSphere(0.19,28,22); glPopMatrix();   // head
}

void drawRook()
{
    drawTaperedCylinderY(0.32f,0.27f,0.06f);                                       // flared base
    glPushMatrix(); glTranslatef(0,0.06f,0); drawCylinderY(0.24f,0.03f,28); glPopMatrix();       // base ring
    glPushMatrix(); glTranslatef(0,0.09f,0); drawTaperedCylinderY(0.21f,0.24f,0.50f); glPopMatrix(); // slightly flaring tower body
    glPushMatrix(); glTranslatef(0,0.59f,0); drawCylinderY(0.29f,0.08f,28); glPopMatrix();       // rim below crenellations
    for (int i=0;i<4;++i)
    {
        float a=i*90.0f*PI/180.0f;
        glPushMatrix();
        glTranslatef(0.19f*std::cos(a),0.75f,0.19f*std::sin(a));
        drawBox(0.18f,0.16f,0.18f);
        glPopMatrix();
    }
}

void drawBishop()
{
    drawTaperedCylinderY(0.30f,0.24f,0.06f);                                       // flared base
    glPushMatrix(); glTranslatef(0,0.06f,0); drawCylinderY(0.22f,0.03f,28); glPopMatrix();       // base ring
    glPushMatrix(); glTranslatef(0,0.09f,0); drawTaperedCylinderY(0.18f,0.13f,0.42f); glPopMatrix(); // tapered body
    glPushMatrix(); glTranslatef(0,0.51f,0); drawCylinderY(0.15f,0.025f,28); glPopMatrix();      // collar
    glPushMatrix(); glTranslatef(0,0.535f+0.18f,0); glutSolidSphere(0.18,26,20); glPopMatrix();  // ball
    glPushMatrix(); glTranslatef(0,0.535f+0.36f,0); glRotatef(-90,1,0,0); glutSolidCone(0.095,0.24,22,14); glPopMatrix(); // mitre
    glPushMatrix(); glTranslatef(0,0.535f+0.36f+0.24f,0); glutSolidSphere(0.045,14,10); glPopMatrix(); // finial
}

void drawKnight()
{
    drawTaperedCylinderY(0.30f,0.25f,0.06f);                                       // flared base
    glPushMatrix(); glTranslatef(0,0.06f,0); drawCylinderY(0.22f,0.03f,28); glPopMatrix();       // base ring
    glPushMatrix(); glTranslatef(0,0.09f,0); drawTaperedCylinderY(0.19f,0.16f,0.29f); glPopMatrix(); // tapered body

    // A clearer horse head: a tapered neck built from two chained,
    // increasingly forward-tilted segments, topped with a distinct
    // long/narrow head - brow ridge, tapering muzzle down to a nostril
    // flare, an underslung jaw taper, sharp forward-leaning ears and
    // dark (unlit) eyes so it reads as a horse rather than a box.
    glPushMatrix();
    glTranslatef(0,0.38f,0.02f);
    glRotatef(-20.0f,1,0,0);
    drawTaperedCylinderY(0.155f,0.125f,0.24f,24);                                   // lower neck

    for (int i=0;i<3;++i)                                                           // mane ridge, lower neck
    {
        float t = i/2.0f;
        glPushMatrix();
        glTranslatef(0.0f, 0.03f+t*0.17f, 0.10f);
        drawBox(0.035f,0.10f-0.035f*t,0.05f);
        glPopMatrix();
    }

    glTranslatef(0,0.24f,0);
    glRotatef(-28.0f,1,0,0);
    drawTaperedCylinderY(0.125f,0.10f,0.19f,24);                                    // upper neck, curving forward

    for (int i=0;i<2;++i)                                                           // mane ridge, upper neck
    {
        float t = (float)i;
        glPushMatrix();
        glTranslatef(0.0f, 0.03f+t*0.08f, 0.08f);
        drawBox(0.032f,0.07f,0.045f);
        glPopMatrix();
    }

    glTranslatef(0,0.19f,0);
    glRotatef(-6.0f,1,0,0);

    // Head, built long and narrow (horse heads are elongated, not square).
    glPushMatrix(); glTranslatef(0,0.075f,-0.03f); drawBox(0.145f,0.15f,0.24f); glPopMatrix();     // main head block
    glPushMatrix(); glTranslatef(0,0.105f,-0.06f); drawBox(0.125f,0.07f,0.14f); glPopMatrix();     // brow / forehead ridge
    glPushMatrix(); glTranslatef(0,0.045f,-0.19f); drawBox(0.115f,0.10f,0.16f); glPopMatrix();     // upper muzzle
    glPushMatrix(); glTranslatef(0,0.020f,-0.30f); drawBox(0.085f,0.075f,0.09f); glPopMatrix();    // lower muzzle
    glPushMatrix(); glTranslatef(0,0.010f,-0.36f); drawBox(0.075f,0.05f,0.045f); glPopMatrix();    // nose / nostril flare
    glPushMatrix();
    glTranslatef(0,-0.020f,-0.15f);
    glRotatef(18.0f,1,0,0);
    drawBox(0.10f,0.06f,0.18f);                                                     // underslung jaw taper
    glPopMatrix();

    for (int s=-1;s<=1;s+=2)                                                        // ears, leaning forward/inward
    {
        glPushMatrix();
        glTranslatef(s*0.06f,0.19f,0.01f);
        glRotatef((float)s*10.0f,0,0,1);
        glRotatef(-6.0f,0,1,0);
        glRotatef(-90.0f,1,0,0);
        glutSolidCone(0.026,0.135,10,5);
        glPopMatrix();
    }

    GLboolean lw = glIsEnabled(GL_LIGHTING);                                        // eyes, dark and unlit so they
    if (lw) glDisable(GL_LIGHTING);                                                 // stay visible on both colours
    glColor3f(0.02f,0.02f,0.02f);
    for (int s=-1;s<=1;s+=2)
    {
        glPushMatrix();
        glTranslatef(s*0.085f,0.095f,-0.10f);
        glutSolidSphere(0.016,10,8);
        glPopMatrix();
    }
    if (lw) glEnable(GL_LIGHTING);

    glPopMatrix();
}

void drawQueen()
{
    drawTaperedCylinderY(0.34f,0.28f,0.07f);                                       // flared base
    glPushMatrix(); glTranslatef(0,0.07f,0); drawCylinderY(0.25f,0.03f,28); glPopMatrix();       // base ring
    glPushMatrix(); glTranslatef(0,0.10f,0); drawTaperedCylinderY(0.20f,0.15f,0.52f); glPopMatrix(); // tapered stem
    glPushMatrix(); glTranslatef(0,0.62f,0); drawTaperedCylinderY(0.15f,0.27f,0.10f); glPopMatrix(); // flared cup
    glPushMatrix(); glTranslatef(0,0.72f,0); glutSolidSphere(0.16,26,20); glPopMatrix();         // crown base
    for(int i=0;i<6;++i)
    {
        float a=2*PI*i/6.0f;
        glPushMatrix();
        glTranslatef(0.21f*std::cos(a),0.86f,0.21f*std::sin(a));
        glutSolidSphere(0.075,14,10);
        glPopMatrix();
    }
    glPushMatrix(); glTranslatef(0,0.95f,0); glutSolidSphere(0.09,16,12); glPopMatrix();         // top finial
}

void drawKing()
{
    drawTaperedCylinderY(0.35f,0.29f,0.07f);                                       // flared base
    glPushMatrix(); glTranslatef(0,0.07f,0); drawCylinderY(0.26f,0.03f,28); glPopMatrix();       // base ring
    glPushMatrix(); glTranslatef(0,0.10f,0); drawTaperedCylinderY(0.21f,0.16f,0.55f); glPopMatrix(); // tapered stem
    glPushMatrix(); glTranslatef(0,0.65f,0); drawTaperedCylinderY(0.16f,0.26f,0.10f); glPopMatrix(); // flared collar
    glPushMatrix(); glTranslatef(0,0.75f,0); drawCylinderY(0.25f,0.10f,28); glPopMatrix();       // crown band
    // cross
    glPushMatrix(); glTranslatef(0,1.02f,0); drawBox(0.10f,0.34f,0.10f); glPopMatrix();
    glPushMatrix(); glTranslatef(0,1.07f,0); drawBox(0.30f,0.09f,0.10f); glPopMatrix();
}

void drawPieceByType(int type)
{
    switch(type)
    {
        case 1: drawPawn(); break;
        case 2: drawRook(); break;
        case 3: drawKnight(); break;
        case 4: drawBishop(); break;
        case 5: drawQueen(); break;
        case 6: drawKing(); break;
    }
}

void drawPieceAt(int index, float boardZ)
{
    if (index<0 || index>=32 || chessPieces[index].captured)
        return;

    ChessPieceState &p = chessPieces[index];

    const float sq = 0.50f;
    float x = (p.col - 3.5f) * sq;
    float z = boardZ + (3.5f - p.row) * sq;

    glPushMatrix();
    glTranslatef(x,2.52f,z);

    // Rotation and scaling apply only to this individual piece.
    glRotatef(p.angle,0,1,0);
    glScalef(p.scale,p.scale,p.scale);

    if (index==selectedPiece)
        matGold();
    else if (p.white)
        matWhiteChess();
    else
        matBlackChess();

    glScalef(0.36f,0.36f,0.36f);
    drawPieceByType(p.type);

    glPopMatrix();
}


void drawChessPieces(float boardZ)
{
    for (int i=0;i<32;++i)
        drawPieceAt(i,boardZ);
}


// Small A-H / 1-8 labels around the main chessboard.
// These improve readability and make the board look more tournament-like.
void drawBoardFlatText(float x, float z, const char* text, float scale)
{
    GLboolean lightingWasOn = glIsEnabled(GL_LIGHTING);
    if (lightingWasOn) glDisable(GL_LIGHTING);

    glColor3f(0.92f,0.78f,0.34f);
    glPushMatrix();
    glTranslatef(x,2.455f,z);
    glRotatef(-90.0f,1,0,0);
    glScalef(scale,scale,scale);
    for (const char* p=text; *p; ++p)
        glutStrokeCharacter(GLUT_STROKE_ROMAN,*p);
    glPopMatrix();

    if (lightingWasOn) glEnable(GL_LIGHTING);
}

void drawBoardCoordinates(float cx, float cz)
{
    const float sq = 0.50f;
    const char* files[8] = {"A","B","C","D","E","F","G","H"};
    const char* ranks[8] = {"8","7","6","5","4","3","2","1"};

    for (int c=0;c<8;++c)
    {
        float x = cx + (c-3.5f)*sq - 0.045f;
        drawBoardFlatText(x,cz+2.20f,files[c],0.00145f);
    }

    for (int r=0;r<8;++r)
    {
        float z = cz + (3.5f-r)*sq + 0.035f;
        drawBoardFlatText(cx-2.22f,z,ranks[r],0.00135f);
    }
}

// Animated ring under the currently controlled queen.
void drawSelectedPieceIndicator(float boardZ)
{
    ChessPieceState &p = chessPieces[selectedPiece];

    if (p.captured) return;

    const float sq = 0.50f;
    float x = (p.col - 3.5f) * sq;
    float z = boardZ + (3.5f - p.row) * sq;

    float pulseScale = 1.0f + 0.08f*std::sin(selectionPulse);

    GLboolean lightingWasOn = glIsEnabled(GL_LIGHTING);
    if (lightingWasOn) glDisable(GL_LIGHTING);

    if (p.white) glColor3f(1.0f,0.78f,0.12f);
    else         glColor3f(0.20f,0.80f,1.0f);

    glPushMatrix();
    glTranslatef(x,2.49f,z);
    glRotatef(90.0f,1,0,0);
    glScalef(pulseScale,pulseScale,pulseScale);
    glutSolidTorus(0.025f,0.30f,10,28);
    glPopMatrix();

    if (lightingWasOn) glEnable(GL_LIGHTING);
}


void drawChessBoard(float cx, float cz)
{
    const float sq=0.50f;
    for(int r=0;r<8;++r)
    {
        for(int c=0;c<8;++c)
        {
            if ((r+c)%2==0)
                setMaterial(0.30f,0.28f,0.22f, 0.90f,0.86f,0.72f, 0.55f,0.52f,0.42f, 55.0f);
            else
                setMaterial(0.03f,0.03f,0.03f, 0.12f,0.10f,0.08f, 0.50f,0.46f,0.40f, 65.0f);

            float x = cx + (c-3.5f)*sq;
            float z = cz + (3.5f-r)*sq;
            glPushMatrix();
            glTranslatef(x,2.43f,z);
            drawBox(sq*0.98f,0.06f,sq*0.98f);
            glPopMatrix();
        }
    }

    matWood();
    glPushMatrix(); glTranslatef(cx,2.39f,cz+2.08f); drawBox(4.35f,0.08f,0.16f); glPopMatrix();
    glPushMatrix(); glTranslatef(cx,2.39f,cz-2.08f); drawBox(4.35f,0.08f,0.16f); glPopMatrix();
    glPushMatrix(); glTranslatef(cx+2.08f,2.39f,cz); drawBox(0.16f,0.08f,4.35f); glPopMatrix();
    glPushMatrix(); glTranslatef(cx-2.08f,2.39f,cz); drawBox(0.16f,0.08f,4.35f); glPopMatrix();

    drawBoardCoordinates(cx,cz);
}

// ================================================================
// HUMAN PLAYERS
// ================================================================
void drawPlayer(float x, float z, float angle, float shirtR, float shirtG, float shirtB)
{
    // The model's FRONT is local -Z.  The two tournament players are
    // rotated in drawMainTournamentArea() so they visibly face each other.
    glPushMatrix();
    glTranslatef(x,0,z);
    glRotatef(angle,0,1,0);

    // ---- legs: tapered calf + thigh with a knee joint, plus shoes ----
    for (int s=-1; s<=1; s+=2)
    {
        float lx = s*0.25f;
        setMaterial(0.03f,0.03f,0.05f, 0.10f,0.12f,0.18f, 0.09f,0.09f,0.10f, 12.0f);
        glPushMatrix(); glTranslatef(lx,0.06f,-0.05f); drawTaperedCylinderY(0.145f,0.115f,0.42f,16); glPopMatrix(); // calf
        glPushMatrix(); glTranslatef(lx,0.48f,-0.05f); glutSolidSphere(0.125f,14,10); glPopMatrix();               // knee
        glPushMatrix(); glTranslatef(lx,0.48f,-0.05f); drawTaperedCylinderY(0.125f,0.165f,0.46f,16); glPopMatrix(); // thigh

        setMaterial(0.02f,0.02f,0.02f, 0.07f,0.07f,0.08f, 0.12f,0.12f,0.12f, 24.0f);
        glPushMatrix(); glTranslatef(lx,0.045f,-0.02f); drawBox(0.19f,0.09f,0.36f); glPopMatrix();                  // shoe
    }

    // ---- pelvis / waist block connecting legs to torso ----
    setMaterial(shirtR*0.20f,shirtG*0.20f,shirtB*0.20f, shirtR*0.82f,shirtG*0.82f,shirtB*0.82f, 0.14f,0.14f,0.14f, 16.0f);
    glPushMatrix(); glTranslatef(0,0.97f,-0.02f); drawBox(0.78f,0.22f,0.42f); glPopMatrix();

    // ---- torso: narrower waist tapering up into broader shoulders ----
    setMaterial(shirtR*0.24f,shirtG*0.24f,shirtB*0.24f, shirtR,shirtG,shirtB, 0.16f,0.16f,0.16f, 18.0f);
    glPushMatrix(); glTranslatef(0,1.33f,0);   drawBox(0.82f,0.55f,0.48f); glPopMatrix();  // waist/chest
    glPushMatrix(); glTranslatef(0,1.82f,0);   drawBox(0.98f,0.46f,0.50f); glPopMatrix();  // upper chest/shoulders
    for (int s=-1;s<=1;s+=2)                                                               // rounded shoulder caps
    {
        glPushMatrix(); glTranslatef(s*0.47f,2.00f,0); glutSolidSphere(0.155f,14,10); glPopMatrix();
    }

    // Shirt collar / central placket to make the torso more defined.
    setMaterial(0.85f,0.85f,0.86f, 0.96f,0.96f,0.96f, 0.18f,0.18f,0.18f, 18.0f);
    glPushMatrix(); glTranslatef(-0.10f,2.04f,-0.16f); glRotatef(28.0f,0,0,1); drawBox(0.18f,0.12f,0.02f); glPopMatrix();
    glPushMatrix(); glTranslatef( 0.10f,2.04f,-0.16f); glRotatef(-28.0f,0,0,1); drawBox(0.18f,0.12f,0.02f); glPopMatrix();
    setMaterial(0.02f,0.02f,0.08f, 0.08f,0.10f,0.40f, 0.08f,0.08f,0.10f, 10.0f);
    glPushMatrix(); glTranslatef(0,1.72f,-0.17f); drawBox(0.08f,0.55f,0.02f); glPopMatrix();

    // ---- neck ----
    matSkin();
    glPushMatrix(); glTranslatef(0,2.28f,0); drawTaperedCylinderY(0.145f,0.135f,0.24f,16); glPopMatrix();

    // ---- head ----
    glPushMatrix(); glTranslatef(0,2.65f,0); glutSolidSphere(0.37,26,20); glPopMatrix();

    // ears
    for (int s=-1;s<=1;s+=2)
    {
        glPushMatrix(); glTranslatef(s*0.37f,2.63f,-0.02f); glScalef(0.5f,1.0f,0.7f); glutSolidSphere(0.09,12,8); glPopMatrix();
    }

    // hair
    matHair();
    glPushMatrix(); glTranslatef(0,2.87f,0.03f); glScalef(1.02f,0.46f,1.02f); glutSolidSphere(0.39,24,18); glPopMatrix();

    // eyebrows, sitting just above the eyes
    matHair();
    for (int s=-1;s<=1;s+=2)
    {
        glPushMatrix(); glTranslatef(s*0.13f,2.775f,-0.345f); drawBox(0.095f,0.020f,0.02f); glPopMatrix();
    }

    // Face details on local -Z side make the facing direction obvious.
    setMaterial(0.80f,0.80f,0.80f, 0.95f,0.95f,0.95f, 0.12f,0.12f,0.12f, 12.0f);
    glPushMatrix(); glTranslatef(-0.13f,2.72f,-0.340f); glutSolidSphere(0.048f,12,8); glPopMatrix();
    glPushMatrix(); glTranslatef( 0.13f,2.72f,-0.340f); glutSolidSphere(0.048f,12,8); glPopMatrix();

    setMaterial(0.01f,0.01f,0.01f, 0.03f,0.03f,0.03f, 0.12f,0.12f,0.12f, 10.0f);
    glPushMatrix(); glTranslatef(-0.13f,2.72f,-0.380f); glutSolidSphere(0.022f,12,8); glPopMatrix();
    glPushMatrix(); glTranslatef( 0.13f,2.72f,-0.380f); glutSolidSphere(0.022f,12,8); glPopMatrix();

    matSkin();
    glPushMatrix(); glTranslatef(0.0f,2.60f,-0.382f); glScalef(0.07f,0.10f,0.08f); glutSolidSphere(1.0,14,10); glPopMatrix();

    // mouth
    setMaterial(0.10f,0.01f,0.01f, 0.34f,0.06f,0.05f, 0.06f,0.04f,0.04f, 8.0f);
    glPushMatrix(); glTranslatef(0.0f,2.48f,-0.362f); drawBox(0.16f,0.025f,0.02f); glPopMatrix();

    // ---- arms: tapered upper arm + forearm with an elbow joint,
    // reaching forward-down toward the chess board (local -Z). ----
    for (int s=-1;s<=1;s+=2)
    {
        setMaterial(shirtR*0.24f,shirtG*0.24f,shirtB*0.24f, shirtR,shirtG,shirtB, 0.16f,0.16f,0.16f, 18.0f);
        glPushMatrix();
        glTranslatef(s*0.49f,2.00f,0.0f);
        glRotatef(-90.0f,1,0,0);   // point the limb chain's local +Y forward (-Z)
        glRotatef(-10.0f,1,0,0);   // slight downward droop toward the board
        drawTaperedCylinderY(0.115f,0.095f,0.40f,14);                       // upper arm
        glTranslatef(0,0.40f,0);
        glutSolidSphere(0.095,12,10);                                       // elbow
        drawTaperedCylinderY(0.095f,0.075f,0.36f,14);                       // forearm
        glTranslatef(0,0.36f,0);

        matSkin();
        glPushMatrix(); glScalef(1.0f,0.85f,1.25f); glutSolidSphere(0.115,16,12); glPopMatrix(); // hand (slightly oval, not a perfect ball)
        glPopMatrix();
    }

    glPopMatrix();
}

// ================================================================
// SPECTATORS (audience watching the match)
// ================================================================
void drawSpectator(float x, float z, float angle, float shirtR, float shirtG, float shirtB)
{
    // Simple standing figure with arms resting at the sides, used for the
    // crowd of people watching the tournament. Local front is -Z, same as
    // drawPlayer(), so 'angle' should point the spectator toward the board.
    glPushMatrix();
    glTranslatef(x,0,z);
    glRotatef(angle,0,1,0);

    // ---- legs: tapered calf + thigh with a knee joint, plus shoes ----
    for (int s=-1; s<=1; s+=2)
    {
        float lx = s*0.22f;
        setMaterial(0.04f,0.04f,0.05f, 0.14f,0.14f,0.16f, 0.08f,0.08f,0.09f, 12.0f);
        glPushMatrix(); glTranslatef(lx,0.05f,0); drawTaperedCylinderY(0.135f,0.105f,0.40f,16); glPopMatrix(); // calf
        glPushMatrix(); glTranslatef(lx,0.45f,0); glutSolidSphere(0.115,14,10); glPopMatrix();                 // knee
        glPushMatrix(); glTranslatef(lx,0.45f,0); drawTaperedCylinderY(0.115f,0.15f,0.42f,16); glPopMatrix();  // thigh

        setMaterial(0.02f,0.02f,0.02f, 0.07f,0.07f,0.08f, 0.10f,0.10f,0.10f, 20.0f);
        glPushMatrix(); glTranslatef(lx,0.04f,0.03f); drawBox(0.17f,0.08f,0.33f); glPopMatrix();               // shoe
    }

    // ---- pelvis / waist block connecting legs to torso ----
    setMaterial(shirtR*0.20f,shirtG*0.20f,shirtB*0.20f, shirtR*0.80f,shirtG*0.80f,shirtB*0.80f, 0.12f,0.12f,0.12f, 14.0f);
    glPushMatrix(); glTranslatef(0,0.92f,0); drawBox(0.70f,0.20f,0.38f); glPopMatrix();

    // ---- torso: narrower waist tapering up into shoulders ----
    setMaterial(shirtR*0.24f,shirtG*0.24f,shirtB*0.24f, shirtR,shirtG,shirtB, 0.14f,0.14f,0.14f, 16.0f);
    glPushMatrix(); glTranslatef(0,1.28f,0); drawBox(0.74f,0.50f,0.44f); glPopMatrix();   // waist/chest
    glPushMatrix(); glTranslatef(0,1.72f,0); drawBox(0.88f,0.42f,0.46f); glPopMatrix();   // upper chest/shoulders
    for (int s=-1;s<=1;s+=2)                                                              // rounded shoulder caps
    {
        glPushMatrix(); glTranslatef(s*0.43f,1.88f,0); glutSolidSphere(0.14,14,10); glPopMatrix();
    }

    // ---- neck ----
    matSkin();
    glPushMatrix(); glTranslatef(0,2.22f,0); drawTaperedCylinderY(0.135f,0.125f,0.20f,16); glPopMatrix();

    // ---- head ----
    glPushMatrix(); glTranslatef(0,2.55f,0); glutSolidSphere(0.35,24,18); glPopMatrix();

    // ears
    for (int s=-1;s<=1;s+=2)
    {
        glPushMatrix(); glTranslatef(s*0.35f,2.53f,-0.02f); glScalef(0.5f,1.0f,0.7f); glutSolidSphere(0.085,12,8); glPopMatrix();
    }

    // hair
    matHair();
    glPushMatrix(); glTranslatef(0,2.76f,0.02f); glScalef(1.0f,0.40f,1.0f); glutSolidSphere(0.36,22,16); glPopMatrix();

    // eyebrows, sitting just above the eyes
    matHair();
    for (int s=-1;s<=1;s+=2)
    {
        glPushMatrix(); glTranslatef(s*0.12f,2.655f,-0.315f); drawBox(0.09f,0.018f,0.02f); glPopMatrix();
    }

    // eyes, facing toward the board (local -Z)
    setMaterial(0.80f,0.80f,0.80f, 0.95f,0.95f,0.95f, 0.12f,0.12f,0.12f, 12.0f);
    glPushMatrix(); glTranslatef(-0.12f,2.61f,-0.310f); glutSolidSphere(0.044,10,8); glPopMatrix();
    glPushMatrix(); glTranslatef( 0.12f,2.61f,-0.310f); glutSolidSphere(0.044,10,8); glPopMatrix();
    setMaterial(0.01f,0.01f,0.01f, 0.03f,0.03f,0.03f, 0.10f,0.10f,0.10f, 10.0f);
    glPushMatrix(); glTranslatef(-0.12f,2.61f,-0.345f); glutSolidSphere(0.020,10,8); glPopMatrix();
    glPushMatrix(); glTranslatef( 0.12f,2.61f,-0.345f); glutSolidSphere(0.020,10,8); glPopMatrix();
    matSkin();
    glPushMatrix(); glTranslatef(0.0f,2.50f,-0.350f); glScalef(0.06f,0.09f,0.07f); glutSolidSphere(1.0,12,8); glPopMatrix();
    setMaterial(0.10f,0.01f,0.01f, 0.34f,0.06f,0.05f, 0.06f,0.04f,0.04f, 8.0f);
    glPushMatrix(); glTranslatef(0.0f,2.40f,-0.332f); drawBox(0.14f,0.020f,0.02f); glPopMatrix();

    // shirt front line for a slightly more defined outfit
    setMaterial(0.82f,0.82f,0.84f, 0.95f,0.95f,0.95f, 0.10f,0.10f,0.10f, 10.0f);
    glPushMatrix(); glTranslatef(0,1.62f,-0.16f); drawBox(0.05f,0.65f,0.02f); glPopMatrix();

    // ---- arms: tapered upper arm + forearm with an elbow joint,
    // hanging down at the sides. ----
    for (int s=-1;s<=1;s+=2)
    {
        setMaterial(shirtR*0.24f,shirtG*0.24f,shirtB*0.24f, shirtR,shirtG,shirtB, 0.14f,0.14f,0.14f, 16.0f);
        glPushMatrix();
        glTranslatef(s*0.51f,1.90f,0.0f);
        glRotatef(180.0f,1,0,0);   // point the limb chain's local +Y downward
        glRotatef(s*4.0f,0,0,1);   // slight outward rest lean
        drawTaperedCylinderY(0.100f,0.085f,0.40f,14);                       // upper arm
        glTranslatef(0,0.40f,0);
        glutSolidSphere(0.082,12,10);                                       // elbow
        drawTaperedCylinderY(0.085f,0.070f,0.34f,14);                       // forearm
        glTranslatef(0,0.34f,0);

        setMaterial(0.16f,0.10f,0.06f, 0.68f,0.45f,0.30f, 0.09f,0.09f,0.08f, 10.0f);
        glPushMatrix(); glScalef(1.0f,0.85f,1.2f); glutSolidSphere(0.10,14,10); glPopMatrix();   // hand (slightly oval)
        glPopMatrix();
    }

    glPopMatrix();
}

void drawSpectators()
{
    // A small crowd placed around the main tournament table and along the
    // walls, all roughly facing the game so it reads as an audience.
    // (glRotatef(angle,0,1,0) turns local -Z toward -X at +90 and toward
    // +X at -90, so the west row needs -90 to look east at the table and
    // the east row needs +90 to look west at it.)
    drawSpectator(-4.2f, 2.6f,-90.0f,  0.55f,0.15f,0.15f);
    drawSpectator(-4.2f, 1.2f,-90.0f,  0.20f,0.45f,0.20f);
    drawSpectator(-4.2f,-0.2f,-90.0f,  0.65f,0.55f,0.10f);

    drawSpectator( 4.2f, 2.6f, 90.0f,  0.15f,0.30f,0.55f);
    drawSpectator( 4.2f, 1.2f, 90.0f,  0.50f,0.20f,0.50f);
    drawSpectator( 4.2f,-0.2f, 90.0f,  0.20f,0.55f,0.55f);

    drawSpectator(-2.0f, 7.6f, 0.0f,   0.60f,0.35f,0.10f);
    drawSpectator( 2.0f, 7.6f, 0.0f,   0.10f,0.55f,0.35f);

    // A couple watching the second, smaller table as well (that table is
    // north of them, so they face +Z, angle 180).
    drawSpectator(-2.6f,-7.4f,180.0f,  0.45f,0.45f,0.45f);
    drawSpectator( 2.6f,-7.4f,-90.0f,  0.30f,0.20f,0.60f);

    // A few extra onlookers: a back row by the main table, and a couple
    // more filling out the crowd behind the second table.
    drawSpectator(-4.2f,-2.0f,-90.0f,  0.10f,0.40f,0.60f);
    drawSpectator( 4.2f,-2.0f, 90.0f,  0.45f,0.30f,0.15f);
    drawSpectator(-2.6f,-8.6f,180.0f,  0.55f,0.55f,0.20f);
    drawSpectator( 2.6f,-8.6f,-90.0f,  0.15f,0.45f,0.15f);
}


// ================================================================
// CONTINUOUS ROTATING FANS
// ================================================================
void drawFan(float x, float z, float scale=1.0f)
{
    glPushMatrix();
    glTranslatef(x,7.35f,z);
    glScalef(scale,scale,scale);

    matMetal();
    glPushMatrix(); glTranslatef(0,0.0f,0); drawCylinderY(0.06f,0.45f,18); glPopMatrix();

    glTranslatef(0,-0.10f,0);
    matMetal();
    glutSolidSphere(0.24,18,14);

    glRotatef(fanAngle,0,1,0);
    for(int i=0;i<4;++i)
    {
        glPushMatrix();
        glRotatef(i*90.0f,0,1,0);
        glTranslatef(1.00f,0,0);
        glRotatef(-6.0f,0,0,1);
        drawBox(1.65f,0.07f,0.34f);
        glPopMatrix();
    }

    glPopMatrix();
}

// ================================================================
// LIVE WALL DISPLAY
// ================================================================
void drawLiveDisplay()
{
    const float cx = 0.0f;
    const float cy = 5.55f;
    const float z  = -9.73f;
    const float s  = 0.34f;

    // Lit metallic frame. In dark mode it becomes subtle while the screen stays bright.
    matMetal();
    glPushMatrix(); glTranslatef(cx,cy,z+0.02f); drawBox(3.72f,3.72f,0.15f); glPopMatrix();

    // Everything inside the display is unlit. This is what makes the display
    // continue to glow when the room is in Stage 3 (dark).
    GLboolean lightingWasOn = glIsEnabled(GL_LIGHTING);
    if (lightingWasOn) glDisable(GL_LIGHTING);

    // cyan glow border behind the black screen
    glColor3f(0.02f,0.55f,0.82f);
    glPushMatrix(); glTranslatef(cx,cy,z+0.115f); drawBox(3.44f,3.44f,0.055f); glPopMatrix();

    glColor3f(0.008f,0.020f,0.035f);
    glPushMatrix(); glTranslatef(cx,cy,z+0.145f); drawBox(3.20f,3.20f,0.055f); glPopMatrix();

    // board squares
    for(int r=0;r<8;++r)
    for(int c=0;c<8;++c)
    {
        if ((r+c)%2==0) glColor3f(0.82f,0.78f,0.64f);
        else            glColor3f(0.08f,0.18f,0.22f);

        float x = cx + (c-3.5f)*s;
        float y = cy + (3.5f-r)*s;
        glPushMatrix(); glTranslatef(x,y,z+0.20f); drawBox(s*0.96f,s*0.96f,0.04f); glPopMatrix();
    }

    // LIVE PIECE POSITIONS:
    // every active main-board piece is drawn from its current row/column.
    for (int i=0;i<32;++i)
    {
        if (chessPieces[i].captured)
            continue;

        ChessPieceState &p = chessPieces[i];

        float x = cx + (p.col-3.5f)*s;
        float y = cy + (3.5f-p.row)*s;

        if (i==selectedPiece)
        {
            if (p.white) glColor3f(1.0f,0.72f,0.08f);
            else         glColor3f(0.15f,0.75f,1.0f);
        }
        else if (p.white)
            glColor3f(0.96f,0.92f,0.76f);
        else
            glColor3f(0.035f,0.035f,0.045f);

        float radius = (p.type==1) ? 0.070f : 0.092f;
        if (i==selectedPiece) radius += 0.025f;

        glPushMatrix();
        glTranslatef(x,y,z+0.30f);
        glutSolidSphere(radius,14,10);
        glPopMatrix();

        // A thin wall-screen selection ring around the current piece.
        if (i==selectedPiece)
        {
            if (p.white) glColor3f(1.0f,0.85f,0.20f);
            else         glColor3f(0.25f,0.85f,1.0f);

            glPushMatrix();
            glTranslatef(x,y,z+0.315f);
            glutWireTorus(0.012f,0.15f,8,22);
            glPopMatrix();
        }
    }

    if (lightingWasOn) glEnable(GL_LIGHTING);
}


// ================================================================
// TOURNAMENT SETUP
// ================================================================
void drawMainTournamentArea()
{
    const float boardZ = 2.0f;

    drawTable(0.0f,boardZ,5.4f,5.0f);
    drawChessBoard(0.0f,boardZ);
    drawChessPieces(boardZ);
    drawSelectedPieceIndicator(boardZ);
    drawChessTimer(2.30f,2.72f,boardZ);

    // The local face points toward -Z.
    // Player at +Z looks toward -Z (angle 0).
    // Player at -Z looks toward +Z (angle 180).
    // FIX: the chair's angle must match its player's angle so the
    // backrest sits BEHIND the player (away from the board). It had been
    // swapped (180/0 instead of 0/180), which put each backrest between
    // the player and the board - right in front of their face.
    drawChair(0.0f,5.30f,0.0f);
    drawChair(0.0f,-1.30f,180.0f);
    drawPlayer(0.0f,5.15f,0.0f,   0.15f,0.26f,0.65f);
    drawPlayer(0.0f,-1.15f,180.0f,0.62f,0.14f,0.12f);
}



void drawMiniChessPiece(float x, float z, int type, bool white)
{
    if (white) matWhiteChess(); else matBlackChess();

    glPushMatrix();
    glTranslatef(x,2.50f,z);
    glScalef(0.24f,0.24f,0.24f);
    drawPieceByType(type);
    glPopMatrix();
}

void drawSecondTournamentTable()
{
    float z=-5.6f;
    drawTable(0,z,4.2f,3.6f);

    // simplified board
    const float sq=0.34f;
    for(int r=0;r<8;++r)
    for(int c=0;c<8;++c)
    {
        if((r+c)%2==0)
            setMaterial(0.28f,0.27f,0.23f, 0.88f,0.84f,0.70f, 0.15f,0.15f,0.13f,18);
        else
            setMaterial(0.03f,0.03f,0.03f, 0.10f,0.09f,0.08f, 0.12f,0.12f,0.12f,18);
        glPushMatrix();
        glTranslatef((c-3.5f)*sq,2.43f,z+(3.5f-r)*sq);
        drawBox(sq*0.96f,0.05f,sq*0.96f);
        glPopMatrix();
    }

    // A compact mid-game arrangement so this reads as a real second match,
    // not just an empty decorative board.
    for (int c=0;c<8;++c)
    {
        if (c!=3 && c!=4)
        {
            drawMiniChessPiece((c-3.5f)*sq, z+(3.5f-1)*sq, 1, true);
            drawMiniChessPiece((c-3.5f)*sq, z+(3.5f-6)*sq, 1, false);
        }
    }

    drawMiniChessPiece((-0.5f)*sq,z+(3.5f-0)*sq,5,true);
    drawMiniChessPiece(( 0.5f)*sq,z+(3.5f-0)*sq,6,true);
    drawMiniChessPiece((-0.5f)*sq,z+(3.5f-7)*sq,5,false);
    drawMiniChessPiece(( 0.5f)*sq,z+(3.5f-7)*sq,6,false);

    drawChessTimer(0.0f,2.68f,z+1.57f);

    // Rotate both chairs so they face the table.
    drawChair(-2.6f,z,270);
    drawChair( 2.6f,z,90);
}

// ================================================================
// LIGHTING
// ================================================================
void setupOutdoorLights()
{
    glEnable(GL_LIGHTING);

    // Outdoor and indoor lights are handled separately so a dark tournament
    // room can coexist with a bright morning outside or illuminated night road.
    glDisable(GL_LIGHT0);
    glDisable(GL_LIGHT1);
    glDisable(GL_LIGHT2);
    glDisable(GL_LIGHT3);
    glDisable(GL_LIGHT4);

    if (!isNight)
    {
        GLfloat globalAmbient[] = {0.16f,0.17f,0.18f,1.0f};
        glLightModelfv(GL_LIGHT_MODEL_AMBIENT,globalAmbient);

        // Morning sunlight: directional light.
        GLfloat amb[]  = {0.20f,0.18f,0.12f,1.0f};
        GLfloat diff[] = {1.00f,0.88f,0.64f,1.0f};
        GLfloat spec[] = {0.85f,0.78f,0.62f,1.0f};
        GLfloat pos[]  = {-0.35f,0.90f,0.25f,0.0f};
        glLightfv(GL_LIGHT2,GL_AMBIENT,amb);
        glLightfv(GL_LIGHT2,GL_DIFFUSE,diff);
        glLightfv(GL_LIGHT2,GL_SPECULAR,spec);
        glLightfv(GL_LIGHT2,GL_POSITION,pos);
        glEnable(GL_LIGHT2);
    }
    else
    {
        GLfloat globalAmbient[] = {0.008f,0.010f,0.020f,1.0f};
        glLightModelfv(GL_LIGHT_MODEL_AMBIENT,globalAmbient);

        // Night road lamp 1.
        GLfloat amb[]  = {0.015f,0.010f,0.004f,1.0f};
        GLfloat diff[] = {1.00f,0.72f,0.28f,1.0f};
        GLfloat spec[] = {0.95f,0.70f,0.30f,1.0f};
        GLfloat p3[]   = {-4.5f,2.95f,-14.60f,1.0f};
        GLfloat p4[]   = { 5.0f,2.95f,-14.60f,1.0f};

        glLightfv(GL_LIGHT3,GL_AMBIENT,amb);
        glLightfv(GL_LIGHT3,GL_DIFFUSE,diff);
        glLightfv(GL_LIGHT3,GL_SPECULAR,spec);
        glLightfv(GL_LIGHT3,GL_POSITION,p3);
        glLightf(GL_LIGHT3,GL_CONSTANT_ATTENUATION,0.40f);
        glLightf(GL_LIGHT3,GL_LINEAR_ATTENUATION,0.08f);
        glLightf(GL_LIGHT3,GL_QUADRATIC_ATTENUATION,0.018f);

        glLightfv(GL_LIGHT4,GL_AMBIENT,amb);
        glLightfv(GL_LIGHT4,GL_DIFFUSE,diff);
        glLightfv(GL_LIGHT4,GL_SPECULAR,spec);
        glLightfv(GL_LIGHT4,GL_POSITION,p4);
        glLightf(GL_LIGHT4,GL_CONSTANT_ATTENUATION,0.40f);
        glLightf(GL_LIGHT4,GL_LINEAR_ATTENUATION,0.08f);
        glLightf(GL_LIGHT4,GL_QUADRATIC_ATTENUATION,0.018f);

        glEnable(GL_LIGHT3);
        glEnable(GL_LIGHT4);
    }
}

void setupIndoorLights()
{
    glEnable(GL_LIGHTING);

    // Outdoor lights must not illuminate the indoor scene in fixed-function OpenGL.
    glDisable(GL_LIGHT2);
    glDisable(GL_LIGHT3);
    glDisable(GL_LIGHT4);

    if (lightStage==2)
    {
        // Stage 3: room is intentionally almost completely dark.
        GLfloat globalAmbient[] = {0.004f,0.004f,0.006f,1.0f};
        glLightModelfv(GL_LIGHT_MODEL_AMBIENT,globalAmbient);
        glDisable(GL_LIGHT0);
        glDisable(GL_LIGHT1);
        return;
    }

    glEnable(GL_LIGHT0);
    glEnable(GL_LIGHT1);

    GLfloat globalAmbientNormal[] = {0.17f,0.165f,0.15f,1.0f};
    GLfloat globalAmbientLow[]    = {0.035f,0.035f,0.045f,1.0f};
    glLightModelfv(GL_LIGHT_MODEL_AMBIENT,
                    lightStage==0 ? globalAmbientNormal : globalAmbientLow);

    float factor = (lightStage==0) ? 1.0f : 0.30f;

    // Light 0 - warm ceiling light over the main chess table.
    GLfloat ambient0[]  = {0.08f*factor,0.075f*factor,0.065f*factor,1.0f};
    GLfloat diffuse0[]  = {1.00f*factor,0.93f*factor,0.80f*factor,1.0f};
    GLfloat specular0[] = {0.95f*factor,0.90f*factor,0.82f*factor,1.0f};
    GLfloat pos0[]      = {0.0f,7.35f,2.0f,1.0f};

    glLightfv(GL_LIGHT0,GL_AMBIENT,ambient0);
    glLightfv(GL_LIGHT0,GL_DIFFUSE,diffuse0);
    glLightfv(GL_LIGHT0,GL_SPECULAR,specular0);
    glLightfv(GL_LIGHT0,GL_POSITION,pos0);
    glLightf(GL_LIGHT0,GL_CONSTANT_ATTENUATION,0.65f);
    glLightf(GL_LIGHT0,GL_LINEAR_ATTENUATION,0.025f);
    glLightf(GL_LIGHT0,GL_QUADRATIC_ATTENUATION,0.002f);

    // Light 1 - softer fill light from the opposite side of the hall.
    GLfloat ambient1[]  = {0.05f*factor,0.055f*factor,0.065f*factor,1.0f};
    GLfloat diffuse1[]  = {0.64f*factor,0.71f*factor,0.86f*factor,1.0f};
    GLfloat specular1[] = {0.54f*factor,0.62f*factor,0.76f*factor,1.0f};
    GLfloat pos1[]      = {-5.0f,6.8f,-3.5f,1.0f};

    glLightfv(GL_LIGHT1,GL_AMBIENT,ambient1);
    glLightfv(GL_LIGHT1,GL_DIFFUSE,diffuse1);
    glLightfv(GL_LIGHT1,GL_SPECULAR,specular1);
    glLightfv(GL_LIGHT1,GL_POSITION,pos1);
    glLightf(GL_LIGHT1,GL_CONSTANT_ATTENUATION,0.75f);
    glLightf(GL_LIGHT1,GL_LINEAR_ATTENUATION,0.030f);
    glLightf(GL_LIGHT1,GL_QUADRATIC_ATTENUATION,0.002f);
}


// ================================================================
// CAMERA
// ================================================================
void getViewDirection(float &dx, float &dy, float &dz)
{
    float yaw = camYaw*PI/180.0f;
    float pitch = camPitch*PI/180.0f;
    dx = std::cos(pitch)*std::cos(yaw);
    dy = std::sin(pitch);
    dz = std::cos(pitch)*std::sin(yaw);
}

void setupCamera()
{
    float dx,dy,dz;
    getViewDirection(dx,dy,dz);
    gluLookAt(camX,camY,camZ,
              camX+dx,camY+dy,camZ+dz,
              0.0f,1.0f,0.0f);
}

void drawBitmapText(float x, float y, const char* text)
{
    glRasterPos2f(x,y);
    for (const char* p=text; *p; ++p)
        glutBitmapCharacter(GLUT_BITMAP_8_BY_13,*p);
}

void drawHUD()
{
    GLboolean lightWasOn = glIsEnabled(GL_LIGHTING);
    GLboolean depthWasOn = glIsEnabled(GL_DEPTH_TEST);
    if (lightWasOn) glDisable(GL_LIGHTING);
    if (depthWasOn) glDisable(GL_DEPTH_TEST);

    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    gluOrtho2D(0,windowWidth,0,windowHeight);

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    glColor3f(1.0f,1.0f,1.0f);
    const char* lightText = (lightStage==0) ? "Indoor Light: NORMAL" :
                            (lightStage==1) ? "Indoor Light: LOW" : "Indoor Light: DARK";
    const char* outsideText = isNight ? "Outside: NIGHT" : "Outside: MORNING";
    const char* doorText = doorOpen ? "Door: OPEN" : "Door: CLOSED";
    char pieceText[160];
    ChessPieceState &sp = chessPieces[selectedPiece];
    std::sprintf(pieceText,
                 "Selected: %s %s | square %c%d",
                 sp.white ? "WHITE" : "BLACK",
                 pieceTypeName(sp.type),
                 'A'+sp.col,
                 8-sp.row);

    drawBitmapText(12,windowHeight-22,lightText);
    drawBitmapText(12,windowHeight-40,outsideText);
    drawBitmapText(12,windowHeight-58,doorText);
    drawBitmapText(12,windowHeight-76,pieceText);

    glColor3f(0.90f,0.90f,0.70f);
    drawBitmapText(12,36,"5 cycle WHITE | 6 cycle BLACK | [/] or ,/. previous/next piece | I/K forward/back | J/L left/right");
    drawBitmapText(12,18,"R/Shift+R rotate | +/- scale | T reset selected | B reset all | O door | G light | N day/night | 7 outside");

    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);

    if (depthWasOn) glEnable(GL_DEPTH_TEST);
    if (lightWasOn) glEnable(GL_LIGHTING);
}

// ================================================================
// DISPLAY / CALLBACKS
// ================================================================
void display()
{
    // Background changes with the outdoor time-of-day mode.
    if (isNight) glClearColor(0.004f,0.006f,0.018f,1.0f);
    else         glClearColor(0.24f,0.47f,0.70f,1.0f);

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    setupCamera();

    // 1) Render outdoor world with sun OR road-lamp illumination.
    setupOutdoorLights();
    drawOutsideEnvironment();
    drawHouseExterior();
    drawTournamentDecor();   // colourful tournament decor + signboard + market stall

    // 2) Render the tournament hall with its independent 3-stage lighting.
    setupIndoorLights();
    drawRoom();

    drawFan(-4.3f,2.0f,1.0f);
    drawFan( 4.0f,-2.5f,0.9f);

    drawMainTournamentArea();
    drawSecondTournamentTable();
    drawSpectators();
    drawLiveDisplay();

    drawHUD();
    glutSwapBuffers();
}


void reshape(int w, int h)
{
    if (h==0) h=1;
    windowWidth=w;
    windowHeight=h;
    glViewport(0,0,w,h);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(60.0,(double)w/(double)h,0.1,120.0);
    glMatrixMode(GL_MODELVIEW);
}


void moveCamera(float forward, float side)
{
    float yaw = camYaw*PI/180.0f;
    float fx = std::cos(yaw);
    float fz = std::sin(yaw);
    float rx = std::cos(yaw + PI/2.0f);
    float rz = std::sin(yaw + PI/2.0f);

    camX += fx*forward + rx*side;
    camZ += fz*forward + rz*side;
}

void keyboard(unsigned char key, int, int)
{
    switch(key)
    {
        // Camera movement
        case 'w': case 'W': moveCamera( 0.45f,0); break;
        case 's': case 'S': moveCamera(-0.45f,0); break;
        case 'z': case 'Z': moveCamera(0,-0.45f); break;
        case 'd': case 'D': moveCamera(0, 0.45f); break;
        case 'q': case 'Q': camY += 0.35f; break;
        case 'e': case 'E':
            camY -= 0.35f;
            if(camY<0.5f) camY=0.5f;
            break;

        // --------------------------------------------------------
        // SELECT MULTIPLE INDIVIDUAL CHESS PIECES
        // --------------------------------------------------------
        case '5':
            selectNextPieceOfColor(true);
            break;

        case '6':
            selectNextPieceOfColor(false);
            break;

        case '[':
        case ',':
            selectAdjacentPiece(-1);
            break;

        case ']':
        case '.':
            selectAdjacentPiece(+1);
            break;

        // --------------------------------------------------------
        // MOVE ONLY THE CURRENTLY SELECTED PIECE
        // --------------------------------------------------------
        case 'i': case 'I':
            moveSelectedPieceForward(+1);
            break;

        case 'k': case 'K':
            moveSelectedPieceForward(-1);
            break;

        case 'j': case 'J':
            moveSelectedPiece(0,-1);
            break;

        case 'l': case 'L':
            moveSelectedPiece(0,+1);
            break;

        // Object-coordinate rotation and scaling of selected piece.
        case 'r':
            chessPieces[selectedPiece].angle += 8.0f;
            break;

        case 'R':
            chessPieces[selectedPiece].angle -= 8.0f;
            break;

        case '+': case '=':
            chessPieces[selectedPiece].scale += 0.08f;
            if (chessPieces[selectedPiece].scale>2.0f)
                chessPieces[selectedPiece].scale=2.0f;
            break;

        case '-': case '_':
            chessPieces[selectedPiece].scale -= 0.08f;
            if (chessPieces[selectedPiece].scale<0.45f)
                chessPieces[selectedPiece].scale=0.45f;
            break;

        // Reset selected piece / all pieces.
        case 't': case 'T':
            resetSelectedChessPiece();
            break;

        case 'b': case 'B':
            resetAllChessPieces();
            break;

        // Fan ON/OFF
        case 'f': case 'F':
            fanRunning = !fanRunning;
            break;

        // THREE indoor lighting stages: normal -> low -> dark -> normal.
        case 'g': case 'G':
            lightStage = (lightStage+1)%3;
            break;

        // Morning/night outdoor environment.
        case 'n': case 'N':
            isNight = !isNight;
            break;

        // Open/close the real hinged door.
        case 'o': case 'O':
            doorOpen = !doorOpen;
            doorTargetAngle = doorOpen ? 95.0f : 0.0f;
            break;

        // Camera presets
        case '1':
            camX=0; camY=5.0f; camZ=18.0f; camYaw=-90; camPitch=-10;
            break;
        case '2':
            camX=0; camY=16.0f; camZ=7.5f; camYaw=-90; camPitch=-68;
            break;
        case '3':
            camX=0; camY=3.8f; camZ=7.4f; camYaw=-90; camPitch=-12;
            break;
        case '4':
            camX=8.0f; camY=5.2f; camZ=9.0f; camYaw=-135; camPitch=-12;
            break;
        case '7':
            camX=-7.1f; camY=3.2f; camZ=-18.0f; camYaw=90.0f; camPitch=-3.0f;
            break;

        case 27:
            std::exit(0);
            break;
    }

    glutPostRedisplay();
}


void specialKeys(int key, int, int)
{
    if (key==GLUT_KEY_LEFT)  camYaw   -= 3.0f;
    if (key==GLUT_KEY_RIGHT) camYaw   += 3.0f;
    if (key==GLUT_KEY_UP)    camPitch += 3.0f;
    if (key==GLUT_KEY_DOWN)  camPitch -= 3.0f;

    if (camPitch>80.0f) camPitch=80.0f;
    if (camPitch<-80.0f) camPitch=-80.0f;
    glutPostRedisplay();
}

void mouse(int button, int state, int x, int y)
{
    if (button==GLUT_LEFT_BUTTON)
    {
        if (state==GLUT_DOWN)
        {
            mouseDragging=true;
            lastMouseX=x;
            lastMouseY=y;
        }
        else mouseDragging=false;
    }

    // Common GLUT mouse wheel convention
    if (state==GLUT_DOWN && button==3) moveCamera(0.6f,0);
    if (state==GLUT_DOWN && button==4) moveCamera(-0.6f,0);

    glutPostRedisplay();
}

void mouseMotion(int x, int y)
{
    if (!mouseDragging) return;

    int dx=x-lastMouseX;
    int dy=y-lastMouseY;
    lastMouseX=x;
    lastMouseY=y;

    camYaw   += dx*0.35f;
    camPitch -= dy*0.35f;

    if (camPitch>80.0f) camPitch=80.0f;
    if (camPitch<-80.0f) camPitch=-80.0f;

    glutPostRedisplay();
}

void update(int)
{
    if (fanRunning)
    {
        fanAngle += 4.0f;
        if (fanAngle>=360.0f) fanAngle-=360.0f;
    }

    selectionPulse += 0.10f;
    if (selectionPulse > 2.0f*PI) selectionPulse -= 2.0f*PI;

    // Smooth door animation.
    const float doorSpeed=2.5f;
    if (doorAngle < doorTargetAngle)
    {
        doorAngle += doorSpeed;
        if (doorAngle > doorTargetAngle) doorAngle=doorTargetAngle;
    }
    else if (doorAngle > doorTargetAngle)
    {
        doorAngle -= doorSpeed;
        if (doorAngle < doorTargetAngle) doorAngle=doorTargetAngle;
    }

    // Continuous car movement in the outdoor scene.
    car1X += 0.07f;
    if (car1X > 15.5f) car1X = -15.5f;

    car2X -= 0.09f;
    if (car2X < -15.5f) car2X = 15.5f;

    car3X += 0.05f;
    if (car3X > 15.5f) car3X = -15.5f;

    glutPostRedisplay();
    glutTimerFunc(16,update,0);
}


void init()
{
    glClearColor(0.24f,0.47f,0.70f,1.0f);

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glEnable(GL_NORMALIZE);
    glShadeModel(GL_SMOOTH);

    glEnable(GL_LIGHTING);
    glLightModeli(GL_LIGHT_MODEL_LOCAL_VIEWER,GL_TRUE);

    glHint(GL_PERSPECTIVE_CORRECTION_HINT,GL_NICEST);

    quadric = gluNewQuadric();
    gluQuadricNormals(quadric,GLU_SMOOTH);

    createTextures();
    initChessPieces();
}


int main(int argc, char** argv)
{
    glutInit(&argc,argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
    glutInitWindowSize(1280,720);
    glutInitWindowPosition(70,40);
    glutCreateWindow("Grandmaster Arena - 3D Chess Tournament");

    init();

    glutDisplayFunc(display);
    glutReshapeFunc(reshape);
    glutKeyboardFunc(keyboard);
    glutSpecialFunc(specialKeys);
    glutMouseFunc(mouse);
    glutMotionFunc(mouseMotion);
    glutTimerFunc(16,update,0);

    glutMainLoop();
    return 0;
}
