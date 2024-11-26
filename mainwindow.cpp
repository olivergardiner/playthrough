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

    BASS_SetConfig(BASS_CONFIG_BUFFER, 10 + info.minbuf);
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

DWORD CALLBACK stream(HSTREAM handle, void *buffer, DWORD length, void *user)
{
    HRECORD *recordHandle = (HRECORD *) user;
    char *buf = (char *) buffer;

    unsigned int c = BASS_ChannelGetData(*recordHandle, 0, BASS_DATA_AVAILABLE) - length;
    if (c > 3 * CHUNK) { // we have more than 30ms of data in the record buffer so need to delete some (1 CHUNK = 10ms)
        BASS_ChannelGetData(*recordHandle, NULL, c - CHUNK); // this leaves 10ms of data in the buffer after fulfilling the playback request
    }

    // fetch recorded data into stream
    return BASS_ChannelGetData(*recordHandle, buf, length);

    /*
     * The initial check below is to see if there is a build up of data in the record buffer. If there is, then we remove some of it to keep the buffer size down.
     * This shouldn't ever happen if the sample rates for the record device and the playback device are the same, though.
     *
     * A "chunk" appears to be the standard amount of data typically read and equates to 10ms of data (1764 bytes @ 4 bytes per stereo 16 bit sample and 44.1kHz).
     * While the value of chunk is theoretically "read" from BASS, it appears to be a constant (1764) and the fact that the check is essentially whether we have
     * more than 2 chunks plus 1764 (after fulfilling the request) seems to confirm this.
     *
     * Given that BASS is initialsed with a 10ms update period, I assume that the value of "chunk" is actually dertermined directly from the update period and the
     * sample rate.
     *
     * In this context, it may be preferable to make chunk a constant rather than a variable.
     */

    /*
    unsigned int c = BASS_ChannelGetData(reader->recordHandle, 0, BASS_DATA_AVAILABLE);
    c -= length;
    if (c > 2 * reader->chunk + 1764) {				// buffer has gotten pretty large so remove some
        c -= reader->chunk;		// leave a single 'chunk'
        BASS_ChannelGetData(reader->recordHandle, 0, c);	// remove it
    }

    // fetch recorded data into stream
    c = BASS_ChannelGetData(reader->recordHandle, buf, length);

    // Rather than attempting to fulfil the whole request by padding with silence, it may be better to simply return the actual number of bytes read.
    if (c < length) {
        memset (buf + c, 0, length - c);	// short of data
    }

    return length;
    */
}

void MainWindow::on_runButton_clicked(bool checked)
{
    if (checked) {
        ui->runButton->setText("Stop");
        /*
         * This should be unnecessary as already handled by on_inputVolume_valueChanged.
         *
        float volume = (float) ui->inputVolume->value() / 99.0;
        BASS_RecordSetInput(0, BASS_INPUT_ON, volume);
        */

        recordHandle = BASS_RecordStart(SAMPLE_RATE, CHANNELS, 0, NULL, 0);

        /*
         * This should also be unnecessary as a lack of input data should simply result in no data being passed to the output stream.
         *
        //wait for data to arrive
        while (!BASS_ChannelGetData(recordHandle, 0, BASS_DATA_AVAILABLE));
        */

        //create Playback Stream
        playHandle = BASS_StreamCreate(SAMPLE_RATE, CHANNELS, 0, (STREAMPROC *) &stream, &recordHandle);
        BASS_ChannelPlay (playHandle, FALSE);
    } else {
        ui->runButton->setText("Run");
        BASS_ChannelStop(recordHandle);
        BASS_ChannelStop(playHandle);
    }
}
