#define _CRT_SECURE_NO_WARNINGS 1
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define WAV_HEADER_SIZE 44

//structure of WAV headear
typedef struct {
    char riff[4];
    uint32_t file_size;
    char wave[4];
    char fmt[4];
    uint32_t fmt_size;
    uint16_t audio_format;
    uint16_t num_channels;
    uint32_t sample_rate;
    uint32_t byte_rate;
    uint16_t block_align;
    uint16_t bits_per_sample;
    char data[4];
    uint32_t data_bytes;
} WAVHeader;

//read WAV header
void read_wav_header(FILE* file, WAVHeader* header) {
    fread(header, sizeof(WAVHeader), 1, file);
}

//Write WAV header
void write_wav_header(FILE* file, WAVHeader* header) {
    fwrite(header, sizeof(WAVHeader), 1, file);
}

//produce stereo audio
void merge_mono_to_stereo(const char* noise_file, const char* noisy_file, const char* output_file) {
    FILE* noise= fopen(noise_file, "rb");
    FILE* noisy=fopen(noisy_file, "rb");
    FILE* output=fopen(output_file, "wb");

    if (!noise||!noisy||!output) {
        printf("File can not be opened\n");
        return;
    }

    WAVHeader header_noise, header_noisy;
    read_wav_header(noise, &header_noise);
    read_wav_header(noisy, &header_noisy);

    //make sure the format of two files are matched
    if (header_noise.sample_rate!=header_noisy.sample_rate||
        header_noise.bits_per_sample!=header_noisy.bits_per_sample) {
        printf("Unmatched files\n");
        return;
    }

    //to support stereo audio
    WAVHeader header_stereo=header_noise;
    header_stereo.num_channels=2;//stereo
    header_stereo.byte_rate*=2; 
    header_stereo.block_align*=2;
    header_stereo.data_bytes*=2;
    header_stereo.file_size=header_stereo.data_bytes+WAV_HEADER_SIZE- 8;

    //write new WAV header
    write_wav_header(output, &header_stereo);

    int16_t sample_noise, sample_noisy;
    while (fread(&sample_noise, sizeof(int16_t), 1, noise) &&
        fread(&sample_noisy, sizeof(int16_t), 1, noisy)) {
        fwrite(&sample_noise, sizeof(int16_t), 1, output);//left channel, store the moise 
        fwrite(&sample_noisy, sizeof(int16_t), 1, output);//right channel, store the mixture signal
    }

    fclose(noise);
    fclose(noisy);
    fclose(output);
    printf("Complete\n", output_file);
}

int main() {
    merge_mono_to_stereo("1k.wav", "10k.wav", "combine.wav");
    return 0;
}

