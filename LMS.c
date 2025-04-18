#define _CRT_SECURE_NO_WARNINGS 1
#include <stdio.h>
#include <stdint.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

//#define numSamples 48000  
#define TAP_LENGTH 5
#define _CRT_SECURE_NO_WARNINGS

//structure of WAV headear
#pragma pack(1)
typedef struct {
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
} WavHeader;

float* LMS(const float* x, const float* d, int N, int M){
    //float* y = (float*)malloc(N * sizeof(float));
    float* e=(float*)malloc(N * sizeof(float));
    //float e[numSamples]
    float h[TAP_LENGTH]={0};//intialize the h
    float Delta=0.0009;//intialize the step size
    float power=1e-6;//intialize the power

    //check if the input parameters are validate
    if (x==NULL||d==NULL||N<=0||M<=0){
        printf("Error! Non validate parameter\n");
        return NULL;
    }

    for (int n = 0; n < N-1; n++){
        float y = 0.0;
        //calculate average power
        //power+=(x[n]*x[n])/(M+1);

        //update the value of step size
        //Delta=Delta/(10.0f*N*power);

         //calculate FIR output
        for (int k=0; k<(M-1); k++){
            if (n-k>=0){  //ensure the index is validate
                y+=h[k]*x[n-k];
            }
        }

        //calculate filtered output
        e[n]=d[n]-y;


        //update the weight
        for (int k=0; k<(M-1); k++){
            if (n-k>=0){
                h[k]+=Delta*e [n]*x[n - k];
            }
        }
    }
    return e;
}

void LMS_output(const char* input_fil, const char* output_fil, const char* noise_fil, const char* clean_fil){
    WavHeader Inputheader, Noiseheader, Cleanheader;

    FILE* input=fopen(input_fil, "rb");
    if (!input){
        printf("Error, the sound file can not be opened\n");
        return;
    }
    fread(&Inputheader, sizeof(WavHeader), 1, input);

    FILE* noise = fopen(noise_fil, "rb");
    if (!noise) {
        printf("Error, the noise file can not be opened.\n");
        fclose(input);
        return;
    }
    fread(&Noiseheader, sizeof(WavHeader), 1, noise);

    FILE* clean = fopen(clean_fil, "rb");
    if (!clean) {
        printf("Error, the output file can not be created.\n");
        return;
    }
    fread(&Cleanheader, sizeof(WavHeader), 1, clean);

    //int N=numSamples;
    int N=(Inputheader.subchunk2_size)/(Inputheader.block_align);//calculate the number of samples
    float* x = (float*)malloc(N * sizeof(float));
    //float* x_right = (float*)malloc(N * sizeof(float));
    float* d = (float*)malloc(N * sizeof(float));
    //float* d_right = (float*)malloc(N * sizeof(float));
    float* s = (float*)malloc(N * sizeof(float));

    //if (x_left==NULL||x_right==NULL||d_left==NULL||d_right==NULL){
    if(x==NULL||d==NULL){
        printf("Fail to allocate the memory.\n");
        fclose(input);
        fclose(noise);
        return;
    }

    if(s==NULL||x==NULL||d==NULL){
        printf("Fail to allocate the memory.\n");
        fclose(input);fclose(noise);fclose(clean);
        return;
    }

    //read samples from both input files
    int16_t sample[1];
    for (int i=0; i<N; i++){
        if (fread(sample, sizeof(int16_t), 1, noise)==1){
            x[i]=(float)sample[0]/32768.0f;
            //x_right[i]=(float)sample[1]/32768.0f;
        }
        if (fread(sample, sizeof(int16_t), 1, input)==1){
            d[i]=(float)sample[0]/32768.0f;
            //d_right[i]=(float)sample[1]/32768.0f;
        }
        if (fread(sample, sizeof(int16_t), 1, clean)==1)
            s[i]=(float)sample[0]/32768.0f;
    }
    
    fclose(input);
    fclose(noise);
    fclose(clean);

    //apply the LMS filter
    float*outputData=LMS(x, d, N, TAP_LENGTH);
    //float* outputData_right = LMS(x_right, d_right, N, TAP_LENGTH);

    FILE*output=fopen(output_fil, "wb");
    if (!output){
        printf("Error, the output file can not be created.\n");
        free(outputData);
        //free(outputData_right);
        free(x);
        //free(x_right);
        //free(d_left);
        free(d);
        return;
    }

    fwrite(&Inputheader, sizeof(WavHeader), 1, output);

    //write output file
    for (int i=0; i<N; i++) {
        int16_t sample_out[1];
        sample_out[0]=(int16_t)(outputData[i]*32768.0f);
        //sample_out[1]=(int16_t)(outputData_right[i]*32768.0f);
        fwrite(sample_out, sizeof(int16_t), 1, output);
    }

    float true_mse=0.0f;//intialize the MSE
    int start_index=(int)(Inputheader.sample_rate*1.73);//shield the noise part
    int count=0;
    for (int i=start_index; i<N; i++){//calculate the MSE
        float diff=s[i]-outputData[i];
        true_mse+=diff * diff;
        count++;
    }

    if (count>0){
        true_mse/=count;
    }
    else {
        printf("The sample length is not validate.\n");
        true_mse = 0.0f;
    }
    fclose(output);
    free(outputData);
    //free(outputData_right);
    //free(x_left);
    free(x);
    free(d);
    free(s);
    //free(d_right);
    printf("The filtered audio file is saved as %s¡£\n", output_fil);
    printf("The value of MSE is£º%.8f\n", true_mse);
}

int main() {
    const char* input_wav = "sine_combine.wav";//replaced by the name of input files(already saved in the same folder)
    const char* noise_wav = "noise.wav";
    const char* output_wav = "output_sine.wav";
    const char* clean_wav = "sine.wav";

    LMS_output(input_wav, output_wav, noise_wav, clean_wav);

    return 0;
}
