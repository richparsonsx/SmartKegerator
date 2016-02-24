#include <managers/flowmetermanager.h>
#include <data/settings.h>
#include <data/keg.h>


int tickThreshold = 3;
int endPourSeconds = 5;

// Statics
FlowMeterManager* FlowMeterManager::Instance;

Keg* FlowMeterManager::CurrentKeg;
BeerTapSide FlowMeterManager::CurrentTapSide;
double FlowMeterManager::LastTickAt;
int FlowMeterManager::Ticks;
bool FlowMeterManager::IsPouring;
bool FlowMeterManager::ConflictingTicks;

void FlowMeterManager::Init()
{
    FlowMeterManager::Instance = new FlowMeterManager();
}

void FlowMeterManager::Shutdown()
{
    FlowMeterManager::Instance->gpioProc->kill();
}


// Instance
FlowMeterManager::FlowMeterManager()
{
    gpioWFICommand = Settings::GetString("flowMeterWFICommand");
    leftTickString = QString("Left");
    izqCentroTickString = QString("Izquierda");
    centerTickString = QString("Center");
    derCentroTickString = QString("Derecha");
    rightTickString = QString("Right");

    gpioProc = new QProcess();
    connect(gpioProc, SIGNAL(readyReadStandardOutput()), this, SLOT(onFlowMeterData()));
    gpioProc->start(QString(gpioWFICommand.c_str()));

    connect(&updateTimer, SIGNAL(timeout()), this, SLOT(onUpdateTick()));

    qDebug("Flow meter thread created");
}

void FlowMeterManager::onUpdateTick()
{
    double now = time(NULL);
    if (now - LastTickAt >= endPourSeconds)
        FinishPour();
}

void FlowMeterManager::onFlowMeterData()
{
    QString out = QString(gpioProc->readAllStandardOutput());

    double now = time(NULL);
    // If it's been a couple seconds since the last tick, and we're not pouring, reset ticks
    if (IsPouring == false && now > LastTickAt)
        Ticks = 0;

    // Count the tick
    Ticks++;
    LastTickAt = now;

    // If we're still under the threshold, bail
    if (Ticks < tickThreshold)
        return;

    BeerTapSide tapSide = CENTER_TAP;
    if (out.startsWith(leftTickString))
        tapSide = LEFT_TAP;
    else if (out.startsWith(izqCentroTickString))
        tapSide = IZQ_CENTRO_TAP;
    else if (out.startsWith(derCentroTickString))
        tapSide = DER_CENTRO_TAP;
    else if (out.startsWith(rightTickString))
        tapSide = RIGHT_TAP;

    // If we just crossed the threshold, lets start the pour
    if (Ticks == tickThreshold)
    {
        beginPour(tapSide);
        return;
    }

    // Check for the other beer
    if (tapSide != CurrentTapSide)
        ConflictingTicks = true;

    emit FlowMeterTicked();
}

void FlowMeterManager::beginPour(BeerTapSide tapSide)
{
    CurrentTapSide = tapSide;
    // This is less error-prone than a bunch of nested ternary operators
    switch(CurrentTapSide)
    {
        case LEFT_TAP:        	CurrentKeg = Keg::LeftKeg; break;
        case IZQ_CENTRO_TAP:    CurrentKeg = Keg::IzqCentroKeg; break;
        case CENTER_TAP:        CurrentKeg = Keg::CenterKeg; break;
        case DER_CENTRO_TAP:  CurrentKeg = Keg::DerCentroKeg; break;
        case RIGHT_TAP:         CurrentKeg = Keg::RightKeg; break;
    }


	
	
    if (CurrentKeg == NULL)
    {
        Ticks = 0;
        return;
    }

    IsPouring = true;
    ConflictingTicks = false;

    updateTimer.start(1000);

    emit PourStarted();
}

void FlowMeterManager::FinishPour()
{
    IsPouring = false;
    updateTimer.stop();

    emit PourFinished();
}
