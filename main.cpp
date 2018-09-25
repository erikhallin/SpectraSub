#define _WIN32_WINNT 0x0500

#include <windows.h>
#include <gl/gl.h>
#include <gl/glu.h>
#include <stdio.h>
#include <fcntl.h>
#include <io.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>

using namespace std;

struct st_point
{
    st_point()
    {
        x=0;
        y=0;
    }
    st_point(float _x,float _y)
    {
        x=_x;
        y=_y;
    }

    float x,y;
};

// maximum mumber of lines the output console should have
static const WORD MAX_CONSOLE_LINES = 500;
//window size is the whole window
float g_window_width=600;
float g_window_height=300;
//screen is the area where the graph is shown
float g_screen_width=550;
float g_screen_height=230;

float g_mouse_pos[2]={0,0};
bool  g_mouse_but[4]={false,false,false,false};
bool  g_key_trigger_left=false;
bool  g_key_trigger_right=false;
//variables
float g_window_border_shift=15;
float g_scale1_x=1.0;
float g_scale1_y=1.0;
float g_scale2_x=1.0;
float g_scale2_y=1.0;
float g_zoom1_y=0.6;
float g_zoom2_y=0.9;
bool  g_zoom_target_2=false;
float g_value_to_draw_x=0;
float g_value_to_draw_y=0;
float g_value_to_draw_y2=0;
float g_text_scale=6;
float g_text_gap_size=0.4;
int   g_frame_shift_counter=0;

vector<st_point> g_vec_points_s1;
vector<st_point> g_vec_points_s2;
vector<st_point> g_vec_points_org_s1;
vector<st_point> g_vec_points_org_s2;

string g_sample_filename;
string g_blank_filename;
string g_folderpath;

HWND g_hwnd;

enum states
{
    state_load_sample=0,
    state_load_blank,
    state_ready
};

int g_state=state_load_sample;

LRESULT CALLBACK WindowProc(HWND, UINT, WPARAM, LPARAM);
void EnableOpenGL(HWND hwnd, HDC*, HGLRC*);
void DisableOpenGL(HWND, HDC, HGLRC);
void RedirectIOToConsole();

bool load_data(string input_file_name);
int  update(void);
bool draw(void);
bool scale_data(void);
void draw_number(float number,bool force_digits=true);


int WINAPI WinMain(HINSTANCE hInstance,
                   HINSTANCE hPrevInstance,
                   LPSTR lpCmdLine,
                   int nCmdShow)
{
    bool show_console=true;

    //get screen size
    RECT desktop;
    const HWND hDesktop = GetDesktopWindow();
    GetWindowRect(hDesktop, &desktop);
    //g_window_width = desktop.right;
    //g_window_height = desktop.bottom;

    WNDCLASSEX wcex;
    HWND hwnd;
    HWND hwnd_console;
    HDC hDC;
    HGLRC hRC;
    MSG msg;
    BOOL bQuit = FALSE;

    //register window class
    wcex.cbSize = sizeof(WNDCLASSEX);
    wcex.style = CS_OWNDC;
    wcex.lpfnWndProc = WindowProc;
    wcex.cbClsExtra = 0;
    wcex.cbWndExtra = 0;
    wcex.hInstance = hInstance;
    wcex.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
    wcex.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wcex.lpszMenuName = NULL;
    wcex.lpszClassName = "SpectraSub";
    wcex.hIconSm = LoadIcon(NULL, IDI_APPLICATION);;


    if (!RegisterClassEx(&wcex))
        return 0;

    //create main window
    hwnd = CreateWindowEx(0,
                          "SpectraSub",
                          "SpectraSub",
                          WS_POPUP | (WS_OVERLAPPEDWINDOW ^ WS_THICKFRAME),
                          0,//CW_USEDEFAULT,
                          0,//CW_USEDEFAULT,
                          g_window_width,
                          g_window_height,
                          NULL,
                          NULL,
                          hInstance,
                          NULL);
    g_hwnd=hwnd;

    ShowWindow(hwnd, nCmdShow);

    //enable OpenGL for the window
    EnableOpenGL(hwnd, &hDC, &hRC);

    //start console
    if(show_console)
    {
        RedirectIOToConsole();
        //cout<<"Debug mode enabled"<<endl;

        //move window
        hwnd_console=GetConsoleWindow();
        MoveWindow(hwnd_console,0,g_window_height,g_window_width,200,true);
    }

    //register dnd
    DragAcceptFiles(hwnd,true);

    //init
    cout<<"Drag and drop the SAMPLE file to the window above to load\n";

    //program main loop
    while (!bQuit)
    {
        //check for messages
        if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
        {
            //handle or dispatch messages
            if (msg.message == WM_QUIT)
            {
                bQuit = TRUE;
            }
            else
            {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
        }
        else
        {
            //OpenGL animation code goes here

            glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            //glRotatef(1.0,0,0,1);

            glPushMatrix();

            update();
            draw();

            glPopMatrix();

            SwapBuffers(hDC);

            //theta += 1.0f;
            //Sleep (1);
        }
    }

    //shutdown OpenGL
    DisableOpenGL(hwnd, hDC, hRC);

    //destroy the window explicitly
    DestroyWindow(hwnd);

    return msg.wParam;
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
        case WM_CLOSE:
            PostQuitMessage(0);
        break;

        case WM_DESTROY:
            return 0;

        case WM_MOUSEMOVE:
        {
             g_mouse_pos[0]=LOWORD(lParam);
             g_mouse_pos[1]=HIWORD(lParam);//+g_window_border_shift;

             //reset frameshifter
             g_frame_shift_counter=0;
        }
        break;

        case WM_LBUTTONDOWN:
        {
             g_mouse_but[0]=true;
        }
        break;

        case WM_LBUTTONUP:
        {
             g_mouse_but[0]=false;
        }
        break;

        case WM_RBUTTONDOWN:
        {
             g_mouse_but[1]=true;
        }
        break;

        case WM_RBUTTONUP:
        {
             g_mouse_but[1]=false;
        }
        break;

        case WM_MOUSEWHEEL:
        {
            if(HIWORD(wParam)>5000) {g_mouse_but[2]=true;}
            if(HIWORD(wParam)>100&&HIWORD(wParam)<5000) {g_mouse_but[3]=true;}
        }
        break;

        case WM_KEYDOWN:
        {
            switch (wParam)
            {
                case VK_ESCAPE: PostQuitMessage(0); break;

                case VK_LEFT:
                {
                    if(!g_key_trigger_left)
                    {
                        g_key_trigger_left=true;
                        g_frame_shift_counter-=1;
                    }
                }break;

                case VK_RIGHT:
                {
                    if(!g_key_trigger_right)
                    {
                        g_key_trigger_right=true;
                        g_frame_shift_counter+=1;
                    }
                }break;

                break;
            }
        }
        break;



        case WM_KEYUP:
        {
            switch (wParam)
            {
                case VK_BACK:
                {
                    //clear data
                    g_vec_points_s1.clear();
                    g_vec_points_s2.clear();
                    g_vec_points_org_s1.clear();
                    g_vec_points_org_s2.clear();
                    g_sample_filename="";
                    g_blank_filename="";

                    g_state=state_load_sample;
                    cout<<"Spectrum removed\n";
                    cout<<"Drag and drop the SAMPLE file to the window above to load\n";

                    //window name reset
                    SetWindowText(g_hwnd,"empty");

                }break;
            }
        }

        case WM_DROPFILES:
        {
            //cout<<"file dropped\n";

            TCHAR lpszFile[MAX_PATH] = {0};
            UINT uFile = 0;
            HDROP hDrop = (HDROP)wParam;

            uFile = DragQueryFile(hDrop, 0xFFFFFFFF, NULL, NULL);
            if (uFile != 1)
            {
                //MessageBox (NULL, "Dropping multiple files is not supported.", NULL, MB_ICONERROR);
                DragFinish(hDrop);
                break;
            }
            lpszFile[0] = '\0';
            if (DragQueryFile(hDrop, 0, lpszFile, MAX_PATH))
            {
                //MessageBox (NULL, lpszFile, NULL, MB_ICONINFORMATION);
                //cout<<"file path: "<<lpszFile<<endl;

                load_data(lpszFile);

            }

            DragFinish(hDrop);
        }break;

        default:
            return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }

    return 0;
}

void EnableOpenGL(HWND hwnd, HDC* hDC, HGLRC* hRC)
{
    PIXELFORMATDESCRIPTOR pfd;

    int iFormat;

    //get the device context (DC)
    *hDC = GetDC(hwnd);

    //set the pixel format for the DC
    ZeroMemory(&pfd, sizeof(pfd));

    pfd.nSize = sizeof(pfd);
    pfd.nVersion = 1;
    pfd.dwFlags = PFD_DRAW_TO_WINDOW |
                  PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 24;
    pfd.cDepthBits = 16;
    pfd.iLayerType = PFD_MAIN_PLANE;

    iFormat = ChoosePixelFormat(*hDC, &pfd);

    SetPixelFormat(*hDC, iFormat, &pfd);

    //create and enable the render context (RC)
    *hRC = wglCreateContext(*hDC);

    wglMakeCurrent(*hDC, *hRC);

    //set 2D mode

    glClearColor(0.0,0.0,0.0,0.0);  //Set the cleared screen colour to black
    glViewport(0,0,g_window_width,g_window_height);   //This sets up the viewport so that the coordinates (0, 0) are at the top left of the window

    //Set up the orthographic projection so that coordinates (0, 0) are in the top left
    //and the minimum and maximum depth is -10 and 10. To enable depth just put in
    //glEnable(GL_DEPTH_TEST)
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0,g_window_width,g_window_height,0,-1,1);

    //Back to the modelview so we can draw stuff
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    //Enable antialiasing
    glEnable(GL_LINE_SMOOTH);
    glHint(GL_LINE_SMOOTH_HINT,GL_NICEST);

    //opengl settings
    glEnable(GL_BLEND);
    glDisable(GL_DEPTH_TEST);
    glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
}

void DisableOpenGL (HWND hwnd, HDC hDC, HGLRC hRC)
{
    wglMakeCurrent(NULL, NULL);
    wglDeleteContext(hRC);
    ReleaseDC(hwnd, hDC);
}

void RedirectIOToConsole()
{
    int hConHandle;
    long lStdHandle;
    CONSOLE_SCREEN_BUFFER_INFO coninfo;
    FILE *fp;

    // allocate a console for this app
    AllocConsole();

    // set the screen buffer to be big enough to let us scroll text
    GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE),&coninfo);
    coninfo.dwSize.Y = MAX_CONSOLE_LINES;
    SetConsoleScreenBufferSize(GetStdHandle(STD_OUTPUT_HANDLE),coninfo.dwSize);

    // redirect unbuffered STDOUT to the console
    lStdHandle = (long)GetStdHandle(STD_OUTPUT_HANDLE);
    hConHandle = _open_osfhandle(lStdHandle, _O_TEXT);

    fp = _fdopen( hConHandle, "w" );

    *stdout = *fp;

    setvbuf( stdout, NULL, _IONBF, 0 );

    // redirect unbuffered STDIN to the console

    lStdHandle = (long)GetStdHandle(STD_INPUT_HANDLE);
    hConHandle = _open_osfhandle(lStdHandle, _O_TEXT);

    fp = _fdopen( hConHandle, "r" );
    *stdin = *fp;
    setvbuf( stdin, NULL, _IONBF, 0 );

    // redirect unbuffered STDERR to the console
    lStdHandle = (long)GetStdHandle(STD_ERROR_HANDLE);
    hConHandle = _open_osfhandle(lStdHandle, _O_TEXT);

    fp = _fdopen( hConHandle, "w" );

    *stderr = *fp;

    setvbuf( stderr, NULL, _IONBF, 0 );

    // make cout, wcout, cin, wcin, wcerr, cerr, wclog and clog
    // point to console as well
    ios::sync_with_stdio();
}

bool load_data(string input_file_name)
{
    //check state
    if(g_state==state_ready)
    {
        g_state=state_load_sample;
        //clear current data
        g_vec_points_s1.clear();
        g_vec_points_s2.clear();
        g_vec_points_org_s1.clear();
        g_vec_points_org_s2.clear();
    }

    //open file
    ifstream input_file(input_file_name.c_str());
    if(input_file==0)
    {
        //ERROR file not found
        return false;
    }
    //read data
    int line_counter=0;
    string line,word;
    getline(input_file,line);//skip title line
    while(getline(input_file,line))
    {
        if(line[0]==';') continue;

        st_point new_point1;
        st_point new_point2;
        stringstream ss(line);
        ss>>word;//x
        istringstream(word)>>new_point1.x;
        ss>>word;//y1
        istringstream(word)>>new_point1.y;
        new_point2.x=new_point1.x;
        ss>>word;//y2
        ss>>word;//y2
        ss>>word;//y2
        istringstream(word)>>new_point2.y;

        //check state
        if(g_state==state_load_blank)
        {
            //align check
            if(g_vec_points_org_s1[line_counter].x!=new_point1.x)
            {
                cout<<"ERROR: File to subtract is not aligned with the sample file\n";
                return false;
            }

            //subtraction
            g_vec_points_org_s1[line_counter].y-=new_point1.y;
            g_vec_points_s1[line_counter]=g_vec_points_org_s1[line_counter];
        }

        if(g_state==state_load_sample)
        {
            g_vec_points_s1.push_back(new_point1);
            g_vec_points_s2.push_back(new_point2);
            g_vec_points_org_s1.push_back(new_point1);
            g_vec_points_org_s2.push_back(new_point2);
        }

        if(g_state!=state_load_sample && g_state!=state_load_blank)
        {
            cout<<"ERROR: Not int a load state right now\n";
            return false;
        }

        line_counter++;
    }

    //load blank
    if(g_state==state_load_blank)
    {
        //get file name
        string filename_withdot;
        for(int i=0;i<(int)input_file_name.size();i++)
        {
            if(input_file_name[i]=='\\')
            {
                filename_withdot.clear();
            }
            else filename_withdot.append(1,input_file_name[i]);
        }
        //erase file extension
        string filename_nodot;
        for(int i=0;i<(int)filename_withdot.size();i++)
        {
            if(filename_withdot[i]=='.')
            {
                break;
            }
            else filename_nodot.append(1,filename_withdot[i]);
        }
        g_blank_filename=filename_nodot;

        g_state=state_ready;//next state

        //print data to file
        string filename(g_folderpath);
        filename.append("sub_");
        filename.append(g_sample_filename);
        filename.append("-");
        filename.append(g_blank_filename);
        filename.append(".txt");
        ofstream res_file(filename.c_str());
        if(res_file==0)
        {
            cout<<"ERROR: File could not be created\n";
            return false;
        }
        res_file<<"nm, mdeg, Servo_Volts\n";
        for(int i=0;i<(int)g_vec_points_org_s1.size();i++)
        {
            res_file<<g_vec_points_org_s1[i].x;
            res_file<<", ";
            res_file<<g_vec_points_org_s1[i].y;
            res_file<<", ";
            res_file<<g_vec_points_org_s2[i].y;
            res_file<<endl;
        }
        res_file.close();

        //rename window
        string window_name=g_sample_filename;
        window_name.append("-");
        window_name.append(g_blank_filename);
        SetWindowText(g_hwnd,window_name.c_str());

        cout<<"The subtracted spectrum has been printed\n";
        cout<<"Drag and drop a new SAMPLE file to the window above to load\n";
    }

    //load sample
    if(g_state==state_load_sample)
    {
        g_state=state_load_blank;
        cout<<"Drag and drop the BLANK file to be subtracted\n";

        //get file name
        string filename_withdot;
        for(int i=0;i<(int)input_file_name.size();i++)
        {
            if(input_file_name[i]=='\\')
            {
                filename_withdot.clear();
            }
            else filename_withdot.append(1,input_file_name[i]);
        }
        //erase file extension
        string filename_nodot;
        for(int i=0;i<(int)filename_withdot.size();i++)
        {
            if(filename_withdot[i]=='.')
            {
                break;
            }
            else filename_nodot.append(1,filename_withdot[i]);
        }
        g_sample_filename=filename_nodot;

        //rename window
        SetWindowText(g_hwnd,g_sample_filename.c_str());

        //get file path
        g_folderpath=string(input_file_name,0,(int)input_file_name.size()-(int)filename_withdot.size());
    }

    //scale data
    scale_data();

    return true;
}

int update(void)
{
    //mouse scroll input
    if(g_mouse_but[0]) g_zoom_target_2=false;
    if(g_mouse_but[1]) g_zoom_target_2=true;
    if(g_mouse_but[2])
    {
        if(g_zoom_target_2) g_zoom2_y+=0.01;
        else                g_zoom1_y+=0.01;
        g_mouse_but[2]=false;
        scale_data();
    }
    if(g_mouse_but[3])
    {
        if(g_zoom_target_2) g_zoom2_y-=0.01;
        else                g_zoom1_y-=0.01;
        g_mouse_but[3]=false;
        scale_data();
    }

    //calc number to draw
    if(g_mouse_pos[0]>(g_window_width-g_screen_width)*0.5 && g_mouse_pos[0]<g_screen_width+(g_window_width-g_screen_width)*0.5)
    {
        //find closest x value to cursor
        float value_to_draw_ind=-1;
        for(int i=0;i<(int)g_vec_points_s1.size();i++)
        {
            if(g_vec_points_s1[i].x*g_scale1_x<g_mouse_pos[0]-(g_window_width-g_screen_width)*0.5)
            {
                value_to_draw_ind=i;

                //adjust for frameshifter
                if(g_frame_shift_counter!=0)
                {
                    value_to_draw_ind+=g_frame_shift_counter;
                    //cap
                    if(value_to_draw_ind<0) value_to_draw_ind=0;
                    if(value_to_draw_ind>=(int)g_vec_points_org_s1.size()) value_to_draw_ind=(int)g_vec_points_org_s1.size()-1;
                }

                break;
            }
        }
        if(value_to_draw_ind!=-1 && value_to_draw_ind<(int)g_vec_points_org_s1.size())
        {
            g_value_to_draw_x=g_vec_points_org_s1[value_to_draw_ind].x;
            g_value_to_draw_y=g_vec_points_org_s1[value_to_draw_ind].y;

            g_value_to_draw_y2=g_vec_points_org_s2[value_to_draw_ind].y;
        }
    }

    return 0;
}

bool draw(void)
{
    glClearColor(1.0f, 1.0f, 1.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    glPushMatrix();
    //window border shift
    //glTranslatef(-g_window_border_shift*0.5,g_window_border_shift,0);
    glTranslatef(0,g_window_border_shift,0);

    //draw border
    glColor3f(0,0,0);
    glLineWidth(2);
    glBegin(GL_LINE_STRIP);
    glVertex2f((g_window_width-g_screen_width)*0.5,g_window_height-(g_window_height-g_screen_height)*0.5);
    glVertex2f(g_screen_width+(g_window_width-g_screen_width)*0.5,g_window_height-(g_window_height-g_screen_height)*0.5);
    glVertex2f(g_screen_width+(g_window_width-g_screen_width)*0.5,(g_window_height-g_screen_height)*0.5);
    glVertex2f((g_window_width-g_screen_width)*0.5,(g_window_height-g_screen_height)*0.5);
    glVertex2f((g_window_width-g_screen_width)*0.5,g_window_height-(g_window_height-g_screen_height)*0.5);
    glEnd();

    //draw plot1
    glPushMatrix();
    glTranslatef((g_window_width-g_screen_width)*0.5,g_window_height-(g_window_height-g_screen_height)*0.5,0);
    glColor3f(1.0,0.2,0.2);
    glLineWidth(1);
    glBegin(GL_LINE_STRIP);
    for(int i=0;i<(int)g_vec_points_s1.size();i++)
    {
        glVertex2f(g_vec_points_s1[i].x*g_scale1_x,-g_vec_points_s1[i].y*g_scale1_y);
    }
    glEnd();
    //draw plot2
    glColor3f(0.2,0.2,1.0);
    glLineWidth(1);
    glBegin(GL_LINE_STRIP);
    for(int i=0;i<(int)g_vec_points_s2.size();i++)
    {
        glVertex2f(g_vec_points_s2[i].x*g_scale2_x,-g_vec_points_s2[i].y*g_scale2_y);
    }
    glEnd();
    glPopMatrix();

    //draw vertical marker line
    if(g_mouse_pos[0]>(g_window_width-g_screen_width)*0.5 && g_mouse_pos[0]<g_screen_width+(g_window_width-g_screen_width)*0.5)
    {
        //find closest frame to cursor
        float frame_pos_to_draw_ind=-1;
        for(int i=0;i<(int)g_vec_points_s1.size();i++)
        {
            if(g_vec_points_s1[i].x*g_scale1_x<g_mouse_pos[0]-(g_window_width-g_screen_width)*0.5)
            {
                frame_pos_to_draw_ind=i;

                //adjust for frameshifter
                if(g_frame_shift_counter!=0)
                {
                    frame_pos_to_draw_ind+=g_frame_shift_counter;
                    //cap
                    if(frame_pos_to_draw_ind<0) frame_pos_to_draw_ind=0;
                    if(frame_pos_to_draw_ind>=(int)g_vec_points_org_s1.size()) frame_pos_to_draw_ind=(int)g_vec_points_org_s1.size()-1;
                }

                break;
            }
        }
        if(frame_pos_to_draw_ind!=-1)
        {
            glColor3f(0.4,0.4,0.4);
            glLineWidth(2);
            glBegin(GL_LINES);
            glVertex2f(g_vec_points_s1[frame_pos_to_draw_ind].x*g_scale1_x+(g_window_width-g_screen_width)*0.5,
                       g_window_height-(g_window_height-g_screen_height)*0.5);
            glVertex2f(g_vec_points_s1[frame_pos_to_draw_ind].x*g_scale1_x+(g_window_width-g_screen_width)*0.5,
                       (g_window_height-g_screen_height)*0.5);
            glEnd();
        }
    }

    //draw diagram values
    if(g_mouse_pos[0]>(g_window_width-g_screen_width)*0.5 && g_mouse_pos[0]<g_screen_width+(g_window_width-g_screen_width)*0.5)
    {
        //draw y value
        glPushMatrix();
        glTranslatef(g_mouse_pos[0]-g_text_scale,g_window_border_shift+4,0);
        draw_number(g_value_to_draw_y);
        glPopMatrix();

        //draw y2 value
        glPushMatrix();
        glTranslatef(g_mouse_pos[0]-g_text_scale+10,g_window_border_shift+25,0);
        draw_number(g_value_to_draw_y2);
        glPopMatrix();

        //draw x value
        glPushMatrix();
        glTranslatef(g_mouse_pos[0]-g_text_scale,g_window_height-(g_window_height-g_screen_height)*0.5+4,0);
        draw_number(g_value_to_draw_x,false);
        glPopMatrix();
    }

    glPopMatrix();

    return true;
}

bool scale_data(void)
{
    if(g_vec_points_s1.empty() || g_vec_points_s2.empty()) return false;

    //go through all points
    float x_min=g_vec_points_s1.front().x;
    float y_min=g_vec_points_s1.front().y;
    float x_max=g_vec_points_s1.front().x;
    float y_max=g_vec_points_s1.front().y;
    for(int i=1;i<(int)g_vec_points_s1.size();i++)
    {
        if(g_vec_points_s1[i].x<x_min) x_min=g_vec_points_s1[i].x;
        if(g_vec_points_s1[i].y<y_min) y_min=g_vec_points_s1[i].y;
        if(g_vec_points_s1[i].x>x_max) x_max=g_vec_points_s1[i].x;
        if(g_vec_points_s1[i].y>y_max) y_max=g_vec_points_s1[i].y;
    }
    //shift min values to 0
    for(int i=0;i<(int)g_vec_points_s1.size();i++)
    {
        g_vec_points_s1[i].x-=x_min;
        g_vec_points_s1[i].y-=y_min;
    }
    g_scale1_x=g_screen_width/(x_max-x_min);
    g_scale1_y=g_screen_height/(y_max-y_min)*g_zoom1_y;

    //repeat for second serie
    x_min=g_vec_points_s2.front().x;
    y_min=g_vec_points_s2.front().y;
    x_max=g_vec_points_s2.front().x;
    y_max=g_vec_points_s2.front().y;
    for(int i=1;i<(int)g_vec_points_s2.size();i++)
    {
        if(g_vec_points_s2[i].x<x_min) x_min=g_vec_points_s2[i].x;
        if(g_vec_points_s2[i].y<y_min) y_min=g_vec_points_s2[i].y;
        if(g_vec_points_s2[i].x>x_max) x_max=g_vec_points_s2[i].x;
        if(g_vec_points_s2[i].y>y_max) y_max=g_vec_points_s2[i].y;
    }
    //shift min values to 0
    for(int i=0;i<(int)g_vec_points_s2.size();i++)
    {
        g_vec_points_s2[i].x-=x_min;
        g_vec_points_s2[i].y-=y_min;
    }
    g_scale2_x=g_screen_width/(x_max-x_min);
    g_scale2_y=g_screen_height/(y_max-y_min)*g_zoom2_y;

    return true;
}

void draw_number(float number,bool force_digits)
{
    //convert to string
    stringstream ss;
    ss<<number;
    string string_number(ss.str());
    //force number of digits
    if(force_digits)
    {
        bool dot_found=false;
        for(int i=0;i<(int)string_number.size();i++)
        {
            if(string_number[i]=='.')
            {
                dot_found=true;
                while(i+3>(int)string_number.size()) string_number.append("0");
                break;
            }
        }
        if(!dot_found) string_number.append(".00");
    }

    float scale=g_text_scale;
    float gap_size=g_text_gap_size;

    glPushMatrix();
    glColor4f(0,0,0,0.5);
    glLineWidth(2);
    for(int i=0;i<(int)string_number.size();i++)
    {
        //draw digit
        switch(string_number[i])
        {
            case '-':
            {
                glBegin(GL_LINE_STRIP);
                glVertex2f(0.0*scale,1.0*scale);
                glVertex2f(1.0*scale,1.0*scale);
                glEnd();
            }break;

            case '0':
            {
                glBegin(GL_LINE_STRIP);
                glVertex2f(0.0*scale,0.0*scale);
                glVertex2f(0.0*scale,2.0*scale);
                glVertex2f(1.0*scale,2.0*scale);
                glVertex2f(1.0*scale,0.0*scale);
                glVertex2f(0.0*scale,0.0*scale);
                glEnd();
            }break;

            case '1':
            {
                glBegin(GL_LINE_STRIP);
                glVertex2f(0.7*scale,2.0*scale);
                glVertex2f(0.7*scale,0.0*scale);
                glEnd();
            }break;

            case '2':
            {
                glBegin(GL_LINE_STRIP);
                glVertex2f(0.0*scale,0.0*scale);
                glVertex2f(1.0*scale,0.0*scale);
                glVertex2f(1.0*scale,1.0*scale);
                glVertex2f(0.0*scale,1.0*scale);
                glVertex2f(0.0*scale,2.0*scale);
                glVertex2f(1.0*scale,2.0*scale);
                glEnd();
            }break;

            case '3':
            {
                glBegin(GL_LINE_STRIP);
                glVertex2f(0.0*scale,0.0*scale);
                glVertex2f(1.0*scale,0.0*scale);
                glVertex2f(1.0*scale,1.0*scale);
                glVertex2f(0.0*scale,1.0*scale);
                glVertex2f(1.0*scale,1.0*scale);
                glVertex2f(1.0*scale,2.0*scale);
                glVertex2f(0.0*scale,2.0*scale);
                glEnd();
            }break;

            case '4':
            {
                glBegin(GL_LINE_STRIP);
                glVertex2f(0.0*scale,0.0*scale);
                glVertex2f(0.0*scale,1.0*scale);
                glVertex2f(1.0*scale,1.0*scale);
                glVertex2f(1.0*scale,0.0*scale);
                glVertex2f(1.0*scale,2.0*scale);
                glEnd();
            }break;

            case '5':
            {
                glBegin(GL_LINE_STRIP);
                glVertex2f(1.0*scale,0.0*scale);
                glVertex2f(0.0*scale,0.0*scale);
                glVertex2f(0.0*scale,1.0*scale);
                glVertex2f(1.0*scale,1.0*scale);
                glVertex2f(1.0*scale,2.0*scale);
                glVertex2f(0.0*scale,2.0*scale);
                glEnd();
            }break;

            case '6':
            {
                glBegin(GL_LINE_STRIP);
                glVertex2f(1.0*scale,0.0*scale);
                glVertex2f(0.0*scale,0.0*scale);
                glVertex2f(0.0*scale,2.0*scale);
                glVertex2f(1.0*scale,2.0*scale);
                glVertex2f(1.0*scale,1.0*scale);
                glVertex2f(0.0*scale,1.0*scale);
                glEnd();
            }break;

            case '7':
            {
                glBegin(GL_LINE_STRIP);
                glVertex2f(0.0*scale,0.0*scale);
                glVertex2f(1.0*scale,0.0*scale);
                glVertex2f(1.0*scale,2.0*scale);
                glEnd();
            }break;

            case '8':
            {
                glBegin(GL_LINE_STRIP);
                glVertex2f(0.0*scale,1.0*scale);
                glVertex2f(0.0*scale,2.0*scale);
                glVertex2f(1.0*scale,2.0*scale);
                glVertex2f(1.0*scale,0.0*scale);
                glVertex2f(0.0*scale,0.0*scale);
                glVertex2f(0.0*scale,1.0*scale);
                glVertex2f(1.0*scale,1.0*scale);
                glEnd();
            }break;

            case '9':
            {
                glBegin(GL_LINE_STRIP);
                glVertex2f(1.0*scale,1.0*scale);
                glVertex2f(0.0*scale,1.0*scale);
                glVertex2f(0.0*scale,0.0*scale);
                glVertex2f(1.0*scale,0.0*scale);
                glVertex2f(1.0*scale,2.0*scale);
                glEnd();
            }break;

            case '.':
            {
                glBegin(GL_LINE_STRIP);
                glVertex2f(0.5*scale,1.9*scale);
                glVertex2f(0.5*scale,2.2*scale);
                glEnd();
            }break;
        }

        //move to next pos
        glTranslatef(scale+gap_size*scale,0,0);
    }
    glPopMatrix();

}

