#define _CRT_SECURE_NO_WARNINGS 1
#include <graphics.h>//graphical library 
#include <conio.h>
#include <stdio.h>
#include <windows.h>//to use sleep()
#include <math.h>
#include <stdint.h>
#include <string.h>
#include <iostream>
#include <fstream>
#include <vector>
#include <cstring>
#include <cstdint>
#include <cmath>
#include <thread>
#include <chrono>

int TAP_LENGTH = 20;
#define _CRT_SECURE_NO_WARNINGS


//structure of WAV headear
#pragma pack(1)
struct WavHeader {
    char riff[4];
    uint32_t chunk_size;
    char wave[4];
    char fmt[4];
    uint32_t subchunk1_size;
    uint16_t audio_format;
    uint16_t num_channels;
    uint32_t sample_rate;
    uint32_t byte_rate;
    uint16_t block_align;
    uint16_t bits_per_sample;
    char subchunk2_id[4];
    uint32_t subchunk2_size;
};

void clearSliderSmoothly() {
    for (int i=0; i<256; i+=5) {
        setfillcolor(RGB(255, 255, 255));//set fill color as white
        bar(150, 175, 480, 225);//clear slider and valu(area)
        Sleep(5);
    }
}

void drawSliders() {
    //draw TAP_LENGTH slider
    RECT textRect = {30, 180, 330, 220};//set silder below to the buttons
    drawtext(_T("Tap Length:"), &textRect, DT_LEFT|DT_VCENTER|DT_SINGLELINE);
    rectangle(150, 180, 330, 220);
    int sliderX=150+((TAP_LENGTH-20)*(180/(200-20)));//display the position of TAP_LENGTH 
    fillrectangle(sliderX-5, 185, sliderX+5, 215);//draw the slider

    //show the value of TAP_LENGTH
    TCHAR valueText[50];
    _stprintf_s(valueText, _T("Value: %d"), TAP_LENGTH);
    RECT valueRect={340, 180, 480, 220};
    drawtext(valueText, &valueRect, DT_LEFT|DT_VCENTER|DT_SINGLELINE);
}

void updateParameter(MOUSEMSG m) {
    /*if (m.x > 150 && m.x < 330 && m.y > 180 && m.y < 220 && (m.uMsg == WM_LBUTTONDOWN || m.uMsg == WM_MOUSEMOVE)) {
        TAP_LENGTH = 20 + static_cast<int>((m.x - 150) * (200.0 - 20) / 180.0);//calculate the value of slider 
        drawSliders();//update the slider
    }*/
    static int temp_TAP_LENGTH=TAP_LENGTH;
    if (m.x>150&&m.x<330&&m.y>180&&m.y<220){
        if (m.uMsg==WM_MOUSEMOVE) {
            temp_TAP_LENGTH=20+static_cast<int>((m.x-150)*(200.0-20)/180.0);
            //clearSliderArea();
            drawSliders();
        }
        if (m.uMsg==WM_LBUTTONDOWN) {
            TAP_LENGTH=temp_TAP_LENGTH;
            drawSliders();
        }
        TAP_LENGTH=20+static_cast<int>((m.x-150)*(200.0-20)/180.0);//calculate the value of slider
        //clearSliderArea();
        drawSliders();//update the slider
    }
}


//play audio file
void playWavFile(const char* filename) {
    //use PlaySound to play the music,then SND_ASYNC ensure asynchronous
    if (PlaySound(filename, NULL, SND_FILENAME|SND_ASYNC)) {
        std::cout<<"playing music: "<<filename<<std::endl;
    }
    else {
        std::cerr<<"Fail! The music can not be playing: "<<filename<<std::endl;
    }
}

void stopWavFile() {
    //stop playing the audio
    if (PlaySound(NULL, NULL, 0)) {
        std::cout<<"The music is stop."<<std::endl;
    }
    else {
        std::cerr<<"Fail! The music can not be stopping."<<std::endl;
    }
}

std::vector<float> LMS(const std::vector<float>& x, const std::vector<float>& d, int N, int M){
    std::vector<float> e(N, 0.0f);//save e
    std::vector<float> h(M, 0.0f);//intialize the h
    float Delta = 0.001f;//intialize the step size
    float power = 1e-6f;//intialize the power

    //check if the input parameters are validate
    if (x.empty()||d.empty()||N<=0||M<=0){
        std::cerr<<"Error! Non validate parameter"<< std::endl;
        return{};
    }

    for (int n=0; n<N-1; n++){
        float y=0.0f;

        //calculate FIR output
        for (int k=0; k<M-1; k++){
            if (n-k>=0) {
                y+=h[k]*x[n-k];
            }
        }

        //calculate filtered output
        e[n]=d[n]-y;

      
        //update the weight
        for (int k=0; k<M-1; k++){
            if (n-k>=0) {
                h[k]+=Delta*e[n]*x[n-k];
            }
        }
    }
    return e;
}

void LMS_output(const std::string& input_file, const std::string& output_file, const std::string& noise_file) {
    WavHeader input_header, noise_header;
    std::cout<<"current TAP_LENGTH: "<< TAP_LENGTH<<std::endl;

    //open the sound input
    std::ifstream input(input_file, std::ios::binary);
    if (!input.is_open()) {
        std::cerr<<"Failed！"<<std::endl;
        return;
    }
    input.read(reinterpret_cast<char*>(&input_header), sizeof(WavHeader));

    //open the noise input
    std::ifstream noise(noise_file, std::ios::binary);
    if (!noise.is_open()) {
        std::cerr<<"Failed！"<<std::endl;
        return;
    }
    noise.read(reinterpret_cast<char*>(&noise_header), sizeof(WavHeader));

    //calculate the number of samples
    int N=(input_header.subchunk2_size)/(input_header.block_align);

    //read samples from both input files
    std::vector<float> x(N), d(N);
    int16_t sample[1];
    for (int i=0; i<N; i++) {
        if (noise.read(reinterpret_cast<char*>(sample), sizeof(sample))) {
            x[i]=static_cast<float>(sample[0])/32768.0f;
        }
        if (input.read(reinterpret_cast<char*>(sample), sizeof(sample))) {
            d[i]=static_cast<float>(sample[0])/32768.0f;
        }
    }

    //apply the LMS filter
    std::vector<float>outputData=LMS(x, d, N, TAP_LENGTH);

    //write the output file
    std::ofstream output(output_file, std::ios::binary);
    if (!output.is_open()){
        std::cerr<<"Failed!"<<std::endl;
        return;
    }
    output.write(reinterpret_cast<char*>(&input_header), sizeof(WavHeader));

    for (int i=0; i<N; i++){
        int16_t sample_out[1]={
            static_cast<int16_t>(outputData[i]*32768.0f),
        };
        output.write(reinterpret_cast<char*>(sample_out), sizeof(sample_out));
    }
    std::cout<<"The filtered audio file is saved as: "<< output_file<<"。"<< std::endl;
}

int r[3][4]={{30,20,130,60},{170,20,220,60},{260,20,310,60}};//set buttons

int button_judge(int x, int y)
{
    if (x>r[0][0] && x<r[0][2] && y>r[0][1] && y< r[0][3])return 1;
    if (x> r[1][0] && x<r[1][2] && y>r[1][1] && y< r[1][3])return 2;
    if (x> r[2][0] && x<r[2][2] && y>r[2][1] && y< r[2][3])return 3;
    return 0;
}

int main()
{
    const char* wavFile1="sound.wav";//replaced by the name of input files(already saved in the same folder) 
    const char* wavFile2="output_LMS.wav";

    const char* input_wav="sound.wav";
    const char* noise_wav="noise.wav";
    const char* output_wav="output_LMS.wav";

    int i, event=0;
    short win_width, win_height;//determine the height and width of the window
    win_width=480; win_height=360;
    initgraph(win_width, win_height);//intialize the window
    for (i=0; i<256; i+=5)
    {
        setbkcolor(RGB(i, i, i));//set the background color
        cleardevice();//clear the window
        Sleep(15);//delay
    }
    RECT R1={r[0][0],r[0][1],r[0][2],r[0][3]};
    RECT R2={r[1][0],r[1][1],r[1][2],r[1][3]};
    RECT R3={r[2][0],r[2][1],r[2][2],r[2][3]};
    LOGFONT f;
    gettextstyle(&f);//get the word type
    strcpy_s(f.lfFaceName, _T("宋体"));//set the typeface
    f.lfQuality = ANTIALIASED_QUALITY;//set output anti-aliasing   
    settextstyle(&f);//set word type
    settextcolor(BLACK);//set color
    drawtext(_T("running"), &R1, DT_CENTER|DT_VCENTER|DT_SINGLELINE);//word is in the rectangular area，center|vertical horizontally，shown in single line
    drawtext(_T("start"), &R2, DT_CENTER|DT_VCENTER|DT_SINGLELINE);
    drawtext(_T("clean"), &R3, DT_CENTER|DT_VCENTER|DT_SINGLELINE);
    setlinecolor(BLACK);
    rectangle(r[0][0], r[0][1], r[0][2], r[0][3]);
    rectangle(r[1][0], r[1][1], r[1][2], r[1][3]);
    rectangle(r[2][0], r[2][1], r[2][2], r[2][3]);
    MOUSEMSG m;//pointer of mouse
    drawSliders(); 

    while (true)
    {
        m = GetMouseMsg();//get mouse information
        int button = button_judge(m.x, m.y);

        switch (m.uMsg)
        {
        case WM_LBUTTONUP:
            FlushMouseMsgBuffer();//clear mouse buffer
            FlushMouseMsgBuffer();//ensure fresh
            break;
        case WM_MOUSEMOVE:
            setrop2(R2_XORPEN);
            setlinecolor(LIGHTCYAN);//set line color
            setlinestyle(PS_SOLID, 3);//set line style
            setfillcolor(WHITE);//fill color
            if (button_judge(m.x, m.y)!=0)
            {
                if (event!=button_judge(m.x, m.y))
                {
                    event=button_judge(m.x, m.y);//record the button trigger
                    fillrectangle(r[event - 1][0], r[event - 1][1], r[event - 1][2], r[event - 1][3]);//有框填充矩形（X1,Y1,X2,Y2）
                }
            }
            else
            {
                if (event != 0)//last triggered buffer not return to the original color
                {
                    fillrectangle(r[event - 1][0], r[event - 1][1], r[event - 1][2], r[event - 1][3]);//两次同或为原来颜色
                    event=0;
                }
            }
            break;
        case WM_LBUTTONDOWN:
            setrop2(R2_NOTXORPEN);//NOT(window color XOR current color)
            for (i=0; i<=10; i++)
            {
                setlinecolor(RGB(25*i, 25*i, 25*i));//set color of circle
                circle(m.x, m.y, 2 * i);
                Sleep(30);//pause
                circle(m.x, m.y, 2 * i);//clear the lastes circle
            }
            //judge the operation of mouse
            switch (button_judge(m.x, m.y))
            {
                //button operation
            case 1:
                playWavFile(wavFile1);
                FlushMouseMsgBuffer();//clear mouse information after click button
                break;
            case 2:
                std::cout<<"Filtering:TAP_LENGTH: "<<TAP_LENGTH<<std::endl;
                LMS_output(input_wav, output_wav, noise_wav);
                playWavFile(wavFile2);
                FlushMouseMsgBuffer();
                break;
            case 3:
                /*stopWavFile();
                exit(0);*/
                // restart click
                TAP_LENGTH=20;//refresh
                drawSliders();//re-draw the slider
            default:
                FlushMouseMsgBuffer();
                updateParameter(m);//drag the slider
                //printf("\r\n(%d,%d)",m.x,m.y);//print mouse coordinate
                break;
            }
            break;
        }
        updateParameter(m);//update the value of slider
    }
    system("pause");//pause to show the window
    //closegraph();//close the window
    return 0;
}
