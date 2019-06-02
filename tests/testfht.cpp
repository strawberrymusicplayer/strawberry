#include "testfht.h"
#include "fht.h"

#include <QVector>
#include <algorithm>
#include <cmath>
#include <iostream>

void TestFHT::TestEWMA()
{
    // Create test objects
    FHT exampleFHT1(3);
    FHT exampleFHT2(4);
    FHT exampleFHT3(5);
    FHT exampleFHT4(6);
    FHT exampleFHT5(7);

    int size = 4;
    float * testArray;

    // Test 1
    testArray = new float [size];

    for (int i = 0; i < size ; i++ ) testArray[i] = (float)i;

    exampleFHT1.scale( testArray, 5.0 );

    for (int i = 0; i < size ; i++ ) QCOMPARE(testArray[i], (float)(i*5));

    delete [] testArray;
    size *= 2;


    // Test 2
    testArray = new float [size];

    for (int i = 0; i < size ; i++ ) testArray[i] = (float)i;

    exampleFHT2.scale( testArray, 6.0 );

    for (int i = 0; i < size ; i++ ) QCOMPARE(testArray[i], (float)(i*6));

    delete [] testArray;
    size *= 2;


    // Test 3
    // Produces FHT with num_(8) and exp2_(3) and CasTable
    testArray = new float [size];

    for (int i = 0; i < size ; i++ ) testArray[i] = (float)i;

    exampleFHT3.scale( testArray, 7.0 );

    for (int i = 0; i < size ; i++ ) QCOMPARE(testArray[i], (float)(i*7));

    delete [] testArray;
    size *= 2;


    // Test 4
    // Produces FHT with num_(16) and exp2_(4) and CasTable
    testArray = new float [size];

    for (int i = 0; i < size ; i++ ) testArray[i] = (float)i;

    exampleFHT4.scale( testArray, 8.0 );

    for (int i = 0; i < size ; i++ ) QCOMPARE(testArray[i], (float)(i*8));

    delete [] testArray;
    size *= 2;


    // Test 5
    // Produces FHT with num_(32) and exp2_(5) and CasTable
    testArray = new float [size];

    for (int i = 0; i < size ; i++ ) testArray[i] = (float)i;

    exampleFHT5.scale( testArray, 9.0 );

    for (int i = 0; i < size ; i++ ) QCOMPARE(testArray[i], (float)(i*9));

    delete [] testArray;
    size *= 2;


    return;
};

void TestFHT::TestSpectrum()
{
    // Create test objects
    FHT exampleFHT1(3);
    FHT exampleFHT2(4);
    FHT exampleFHT3(5);
    FHT exampleFHT4(6);
    FHT exampleFHT5(7);

    int size = 8;
    float * testArray;

    // Test 1
    testArray = new float [size];

    for (int i = 0; i < size ; i++ ) testArray[i] = (float)2;

    exampleFHT1.spectrum( testArray );

    QCOMPARE(testArray[0], (float)(16));

    delete [] testArray;
    size *= 2;


    // Test 2
    testArray = new float [size];

    for (int i = 0; i < size ; i++ ) testArray[i] = (float)2;

    exampleFHT2.spectrum( testArray );

    QCOMPARE(testArray[0], (float)(32));

    delete [] testArray;
    size *= 2;

    // Test 3
    // Produces FHT with num_(8) and exp2_(3) and CasTable
    testArray = new float [size];

    for (int i = 0; i < size ; i++ ) testArray[i] = (float)2;

    exampleFHT3.spectrum( testArray );

    QCOMPARE(testArray[0], (float)(64));

    delete [] testArray;
    size *= 2;


    // Test 4
    // Produces FHT with num_(16) and exp2_(4) and CasTable
    testArray = new float [size];

    for (int i = 0; i < size ; i++ ) testArray[i] = (float)2;

    exampleFHT4.spectrum( testArray );

    QCOMPARE(testArray[0], (float)(128));

    delete [] testArray;
    size *= 2;


    // Test 5
    // Produces FHT with num_(32) and exp2_(5) and CasTable
    testArray = new float [size];

    for (int i = 0; i < size ; i++ ) testArray[i] = (float)2;

    exampleFHT5.spectrum( testArray );

    QCOMPARE(testArray[0], (float)(256));

    delete [] testArray;
    size *= 2;

    return;
};

QTEST_APPLESS_MAIN(TestFHT);

//#include "testfht.moc"
