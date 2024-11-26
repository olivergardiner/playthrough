#include "mainwindow.h"
#include "ui_mainwindow.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    HWND hwnd = (HWND) winId();

    // 10ms update period
    BASS_SetConfig(BASS_CONFIG_UPDATEPERIOD, 10);

    // initialize default devices
    if (!BASS_Init(-1, 44100, BASS_DEVICE_LATENCY, hwnd, NULL))
    {
        // couldn't initialize device, so use "no sound"
        BASS_Init(0, 44100, BASS_DEVICE_LATENCY, hwnd, NULL);
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

    //ui->inputVolume->setSliderPosition(99);
    //ui->outputVolume->setSliderPosition(99);
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::on_inputVolume_valueChanged(int value)
{
    BASS_RecordSetInput (ui->inputDevice->currentIndex (), BASS_INPUT_ON, (float) value / 99.0);
}

void MainWindow::on_outputVolume_valueChanged(int value)
{
    BASS_SetVolume((float) value / 99.0);
}

void MainWindow::on_inputDevice_currentIndexChanged(int index)
{
    BASS_RecordInit (index);
    BASS_RecordSetDevice (index);

    float volume;
    int rval = BASS_RecordGetInput(0, &volume);

    if (rval != -1) {
        ui->inputVolume->setValue (99.0 * volume);
    }
}

void MainWindow::on_outputDevice_currentIndexChanged(int index)
{
    BASS_SetDevice (index + 1);
    ui->outputVolume->setValue(99.0 * BASS_GetVolume());
}

DWORD CALLBACK stream(HSTREAM handle, void *buffer, DWORD length, void *user)
{
    reader_t *reader = (reader_t *) user;
    char *buf = (char *) buffer;

    unsigned int c;
    // check how much recorded data is buffered
    c = BASS_ChannelGetData(reader->recordHandle, 0, BASS_DATA_AVAILABLE);
    c -= length;
    if (c > 2 * reader->chunk + 1764)
    {				// buffer has gotten pretty large so remove some
        c -= reader->chunk;		// leave a single 'chunk'
        BASS_ChannelGetData(reader->recordHandle, 0, c);	// remove it
    }
    // fetch recorded data into stream
    c = BASS_ChannelGetData(reader->recordHandle, buf, length);
    if (c < length)
        memset (buf + c, 0, length - c);	// short of data
    return length;
}

void MainWindow::on_runButton_clicked(bool checked)
{
    if (checked) {
        ui->runButton->setText("Stop");
        float volume = (float) ui->inputVolume->value() / 99.0;
        BASS_RecordSetInput(ui->inputDevice->currentIndex(), BASS_INPUT_ON, volume);
        reader.recordHandle = BASS_RecordStart(44100, 2, 0, NULL, 0);

        //wait for data to arrive
        while (!(reader.chunk = BASS_ChannelGetData(reader.recordHandle, 0, BASS_DATA_AVAILABLE)) );

        //create Playback Stream
        playHandle = BASS_StreamCreate(44100, 2, 0, (STREAMPROC *) &stream, &reader);
        BASS_ChannelPlay (playHandle, FALSE);
    } else {
        ui->runButton->setText("Run");
        BASS_ChannelStop(reader.recordHandle);
        BASS_ChannelStop(playHandle);
    }
}
