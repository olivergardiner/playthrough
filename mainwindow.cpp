#include "mainwindow.h"
#include "ui_mainwindow.h"

#define SAMPLE_RATE 44100
#define CHANNELS 2

// 10ms update period
#define UPDATE_PERIOD 10

#define CHUNK (SAMPLE_RATE * CHANNELS * 2 * UPDATE_PERIOD / 1000)

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    HWND hwnd = (HWND) winId();

    BASS_SetConfig(BASS_CONFIG_UPDATEPERIOD, UPDATE_PERIOD);

    // initialize default devices
    if (!BASS_Init(-1, SAMPLE_RATE, BASS_DEVICE_LATENCY, hwnd, NULL))
    {
        // couldn't initialize device, so use "no sound"
        BASS_Init(0, SAMPLE_RATE, BASS_DEVICE_LATENCY, hwnd, NULL);
    }
    BASS_INFO info;
    BASS_GetInfo(&info);

    BASS_SetConfig(BASS_CONFIG_BUFFER, UPDATE_PERIOD + info.minbuf);
    BASS_RecordInit(-1);

    BASS_DEVICEINFO recordInfo;
    BASS_DEVICEINFO playInfo;

    // Get all Recording Devices
    int count = 0;
    while (BASS_RecordGetDeviceInfo(count++, &recordInfo)) {
        ui->inputDevice->addItem (recordInfo.name);
    }

    // Get all Playback Devices
    count = 1;			// 0 would be "No Sound" but we don't need it
    while (BASS_GetDeviceInfo(count++, &playInfo)) {
        ui->outputDevice->addItem (playInfo.name);
    }

    ui->inputIcon->setPixmap(QPixmap("./icons/uil--microphone.svg"));
    ui->outputIcon->setPixmap(QPixmap("./icons/uil--volume-off.svg"));
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::on_inputVolume_valueChanged(int value)
{
    BASS_RecordSetInput(0, BASS_INPUT_ON, (float) value / 99.0);
}

void MainWindow::on_outputVolume_valueChanged(int value)
{
    BASS_SetVolume((float) value / 99.0);
}

void MainWindow::on_inputDevice_currentIndexChanged(int index)
{
    BASS_RecordInit(index);
    BASS_RecordSetDevice(index);

    float volume;
    int rval = BASS_RecordGetInput(0, &volume);

    if (rval != -1) {
        ui->inputVolume->setValue(99.0 * volume);
    }
}

void MainWindow::on_outputDevice_currentIndexChanged(int index)
{
    BASS_SetDevice (index + 1);
    ui->outputVolume->setValue(99.0 * BASS_GetVolume());
}

/**
 * @brief stream    A STREAMPROC callback that is responsible for servicing requests for data to deliver to the output stream
 * @param handle    A handle to the playback HSTREAM
 * @param buffer    The buffer to write the requested amount of data to
 * @param length    The number of bytes to write to the buffer
 * @param user      User data pointer, here used to provide a pointer to the record handle HRECORD
 * @return          The number of bytes actually read from the recording device into the playback device
 */
DWORD CALLBACK MainWindow::stream(HSTREAM handle, void *buffer, DWORD length, void *user)
{
    HRECORD *recordHandle = (HRECORD *) user;

    unsigned int c = BASS_ChannelGetData(*recordHandle, 0, BASS_DATA_AVAILABLE) - length;
    if (c > 3 * CHUNK) { // we have more than 30ms of data in the record buffer so need to delete some (1 CHUNK = 10ms) - this should never happen as the sample rates are the same
        BASS_ChannelGetData(*recordHandle, NULL, c - CHUNK); // this leaves 10ms of data in the buffer after fulfilling the playback request
    }

    // fetch recorded data into stream
    return BASS_ChannelGetData(*recordHandle, buffer, length);
}

void MainWindow::on_runButton_clicked(bool checked)
{
    if (checked) {
        ui->runButton->setText("Stop");
        recordHandle = BASS_RecordStart(SAMPLE_RATE, CHANNELS, 0, NULL, 0);
        playHandle = BASS_StreamCreate(SAMPLE_RATE, CHANNELS, 0, (STREAMPROC *) &stream, &recordHandle);
        BASS_ChannelPlay (playHandle, FALSE);
    } else {
        ui->runButton->setText("Run");
        BASS_ChannelStop(recordHandle);
        BASS_ChannelStop(playHandle);
    }
}
