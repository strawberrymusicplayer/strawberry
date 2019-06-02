#include "fht.h"

#include <QtTest>

class TestFHT: public QObject
{
    Q_OBJECT

private slots:

    void TestEWMA();
    void TestSpectrum();

};
