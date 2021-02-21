#define MINIAUDIO_IMPLEMENTATION

#include "miniaudio.h"
#include "Gist.h"
#include <fvad.h>

#include <stdio.h>
#include <stdint.h>

#define DEVICE_FORMAT           ma_format_s16
#define DEVICE_CHANNELS         1
#define DEVICE_SAMPLE_RATE      16000

#define FRAME_SIZE_MS    30

struct utils {
    Fvad *vad;
    Gist<float>* extractor;
};

float convertInt16ToFloat32(int16_t n) {
    float v = n < 0 ? (float)n / (float)32768 : (float)n / (float)32767;       // convert in range [-32768, 32767]
    // printf("%d -> %f\n", n, v);
    return v; //Math.max(-1, Math.min(1, v)); // clamp
}


void data_callback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount)
{
    struct utils* tools = (struct utils*)pDevice->pUserData;

    MA_ASSERT(tools->vad != NULL);

    int vadres;
    const int16_t* samples = (const int16_t*)pInput;

    vadres = fvad_process(tools->vad, samples, (size_t)frameCount);

    if ( vadres == 1 ) {
        MA_ASSERT(tools->extractor != NULL);

        std::vector<float> floatSamples;
        for (ma_uint32 i = 0 ; i < frameCount ; i++ ) {
            float v = convertInt16ToFloat32(samples[i]);
            floatSamples.push_back(v);
        }
        fflush(stdout);

        tools->extractor->processAudioFrame(floatSamples);

        const std::vector<float>& mfcc = tools->extractor->getMelFrequencyCepstralCoefficients();

        const int numMFCCs = mfcc.size();

        printf(".");
        fflush(stdout);

        // uint32_t idx = 0;
        // for (int i = 0; i < numMFCCs; i++) {
        //     printf("%f ", mfcc[i]);
        // }
        // printf("\n");
        // fflush(stdout);
    }
}

int main(int argc, char** argv)
{
    ma_device_config deviceConfig;
    ma_device device;
    Fvad* vad = NULL;

    vad = fvad_new();
    fvad_set_mode(vad, 2);

    if (fvad_set_sample_rate(vad, 16000) < 0) {
        fprintf(stderr, "invalid sample rate: %d Hz\n", 16);
        fflush(stdout);
        return -10;
    }

    Gist<float>* extractor = new Gist<float>(DEVICE_SAMPLE_RATE * FRAME_SIZE_MS / 1000, DEVICE_SAMPLE_RATE, WindowType::HammingWindow);

    struct utils *tools = (struct utils *)malloc(sizeof(struct utils));

    tools->vad = vad;
    tools->extractor = extractor;

    deviceConfig = ma_device_config_init(ma_device_type_capture);
    deviceConfig.capture.format           = DEVICE_FORMAT;
    deviceConfig.capture.channels         = DEVICE_CHANNELS;
    deviceConfig.sampleRate               = DEVICE_SAMPLE_RATE;
    deviceConfig.periodSizeInMilliseconds = FRAME_SIZE_MS;
    deviceConfig.dataCallback             = data_callback;
    deviceConfig.pUserData                = tools;

    if (ma_device_init(NULL, &deviceConfig, &device) != MA_SUCCESS) {
        printf("Failed to open capture device.\n");
        fflush(stdout);
        return -4;
    }

    printf("Device Name: %s\n", device.capture.name);
    fflush(stdout);

    if (ma_device_start(&device) != MA_SUCCESS) {
        printf("Failed to start capture device.\n");
        fflush(stdout);
        ma_device_uninit(&device);
        return -5;
    }

    printf("Press Enter to quit...\n");
    fflush(stdout);
    getchar();

    ma_device_uninit(&device);

    (void)argc;
    (void)argv;
    return 0;
}